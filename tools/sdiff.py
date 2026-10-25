"""对照 Python 版与 C++ 版的中文 UI 文案，找出 C++ 版可能缺失的功能。

只抽取「可能出现在界面上」的字符串，输出写到 tools/out/diff_strings.txt（UTF-8）。
"""
import glob
import io
import os
import re

BASE = "D:/Audivox事务"
PY = BASE + "/Hearizon Sync/src/HearizonSync.py"
CPP_ROOT = BASE + "/HearizonSync C.R.V/src"
OUT_DIR = BASE + "/HearizonSync C.R.V/tools/out"

STR_RE = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')
SPLIT = re.compile(r"\{[^}]*\}|%[sdf]")


def has_cjk(s):
    return any("\u4e00" <= ch <= "\u9fff" for ch in s)


def collect(text):
    out = set()
    for m in STR_RE.finditer(text):
        s = m.group(1)
        if has_cjk(s) and len(s) >= 2:
            out.add(s)
    return out


py_src = io.open(PY, encoding="utf-8").read()
py_no_doc = re.sub(r'"""[\s\S]*?"""', "", py_src)

UI_HINT = re.compile(
    r"(text\s*=|title\s*\(|title=|configure\s*\(|Label\s*\(|RoundedButton\s*\(|"
    r"messagebox\.|askyesno|askokcancel|showerror|showinfo|showwarning|"
    r"setStatus|_status_threadsafe|_dialog\s*\(|StringVar|"
    r"values\s*=|add_command\s*\()"
)

py_ui = set()
for m in STR_RE.finditer(py_no_doc):
    s = m.group(1)
    if not has_cjk(s) or len(s) < 2:
        continue
    ctx = py_no_doc[max(0, m.start() - 160):m.start()]
    if UI_HINT.search(ctx):
        py_ui.add(s)

cpp_strs = set()
paths = glob.glob(CPP_ROOT + "/**/*.cpp", recursive=True) + glob.glob(CPP_ROOT + "/**/*.h", recursive=True)
for p in paths:
    cpp_strs |= collect(io.open(p, encoding="utf-8", errors="ignore").read())

blob = "\n".join(cpp_strs)


def probe_of(s):
    parts = [x.strip() for x in SPLIT.split(s)]
    parts = [x for x in parts if len(x) >= 2 and has_cjk(x)]
    return max(parts, key=len) if parts else ""


missing = []
for s in sorted(py_ui, key=len, reverse=True):
    pr = probe_of(s)
    if not pr or pr in blob:
        continue
    missing.append((pr, s))

os.makedirs(OUT_DIR, exist_ok=True)
lines = ["Python UI 类中文串 %d 个, C++ 全部中文串 %d 个" % (len(py_ui), len(cpp_strs)),
         "C++ 中找不到对应文本的 %d 条：" % len(missing), ""]
for pr, s in missing:
    lines.append("[缺] " + pr[:70].replace("\n", "\\n"))
    lines.append("     原文: " + s[:200].replace("\n", "\\n"))
io.open(OUT_DIR + "/diff_strings.txt", "w", encoding="utf-8", newline="\n").write("\n".join(lines))
print("wrote", len(missing), "items")
