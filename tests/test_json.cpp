#include "../src/core/Json.h"
#include <cstdio>
#include <string>

static int fails = 0;
static int oks = 0;

static void check(const char* name, bool cond, const std::string& extra = "") {
    if (cond) { ++oks; std::printf("[PASS] %s\n", name); }
    else { ++fails; std::printf("[FAIL] %s %s\n", name, extra.c_str()); }
}

int main() {
    hs::Json j = hs::Json::parse(R"({"a":1,"b":"中文","c":true,"d":[1,2,3],"e":null})");
    check("parse object", j.isObject());
    check("int", j.getInt("a") == 1);
    check("string utf8", j.getString("b") == "中文", j.getString("b"));
    check("bool", j.getBool("c") == true);
    check("null field", j.has("e") && j.find("e")->isNull());

    const hs::Json* d = j.find("d");
    check("array size", d && d->size() == 3);
    check("array item", d && d->at(2) && d->at(2)->asInt() == 3);

    check("missing key default", j.getInt("zzz", 42) == 42);
    check("missing string default", j.getString("zzz", "x") == "x");

    hs::Json o = hs::Json::object();
    o.set("mode", "在线翻译（免费）");
    o.set("n", 3);
    o.set("on", true);
    std::string dumped = o.dump(2);
    hs::Json back = hs::Json::parse(dumped);
    check("roundtrip string", back.getString("mode") == "在线翻译（免费）");
    check("roundtrip int", back.getInt("n") == 3);
    check("roundtrip bool", back.getBool("on"));

    hs::Json e = hs::Json::object();
    std::string tricky = "a\"b";
    tricky += '\\';
    tricky += "c\nd";
    e.set("s", tricky);
    hs::Json e2 = hs::Json::parse(e.dump());
    check("escape roundtrip", e2.getString("s") == tricky, e2.getString("s"));

    check("invalid returns null", hs::Json::parse("{bad").isNull());
    check("empty returns null", hs::Json::parse("").isNull());

    hs::Json u = hs::Json::parse("{\"k\":\"\\u4e2d\\u6587\"}");
    check("unicode escape", u.getString("k") == "中文", u.getString("k"));

    std::printf("\n==== %d passed, %d failed ====\n", oks, fails);
    return fails ? 1 : 0;
}
