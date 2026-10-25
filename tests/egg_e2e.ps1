$ErrorActionPreference = 'Stop'
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public class U {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowW(string c, string n);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessageW(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, IntPtr e);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  public struct RECT { public int L, T, R, B; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);

  public static List<IntPtr> OfPid(uint pid) {
    var r = new List<IntPtr>();
    EnumWindows((h,l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      if (p == pid && IsWindowVisible(h)) r.Add(h);
      return true;
    }, IntPtr.Zero);
    return r;
  }
  public static string Title(IntPtr h) {
    var sb = new StringBuilder(512); GetWindowTextW(h, sb, 512); return sb.ToString();
  }
  public static string Cls(IntPtr h) {
    var sb = new StringBuilder(256); GetClassNameW(h, sb, 256); return sb.ToString();
  }
  public static List<IntPtr> Kids(IntPtr p) {
    var r = new List<IntPtr>();
    EnumChildWindows(p, (h,l) => { r.Add(h); return true; }, IntPtr.Zero);
    return r;
  }
  [DllImport("user32.dll", EntryPoint="SendMessageW")] public static extern IntPtr SendMessagePtr(IntPtr h, uint m, IntPtr w, IntPtr l);
  // 消息点击：不依赖窗口位置、前台焦点、是否被遮挡
  public static void ClickMsg(IntPtr h, int cx, int cy) {
    IntPtr lp = (IntPtr)((cy << 16) | (cx & 0xFFFF));
    SendMessagePtr(h, 0x0201, (IntPtr)1, lp);   // WM_LBUTTONDOWN
    System.Threading.Thread.Sleep(80);
    SendMessagePtr(h, 0x0202, IntPtr.Zero, lp); // WM_LBUTTONUP
  }
  public static void Click(IntPtr h, int cx, int cy) {
    var pt = new POINT(); pt.X = cx; pt.Y = cy;
    ClientToScreen(h, ref pt);
    SetCursorPos(pt.X, pt.Y);
    System.Threading.Thread.Sleep(120);
    mouse_event(0x0002, 0, 0, 0, IntPtr.Zero);   // LEFTDOWN
    System.Threading.Thread.Sleep(60);
    mouse_event(0x0004, 0, 0, 0, IntPtr.Zero);   // LEFTUP
  }
  public static void Key(byte vk) {
    keybd_event(vk, 0, 0, IntPtr.Zero);
    System.Threading.Thread.Sleep(50);
    keybd_event(vk, 0, 2, IntPtr.Zero);          // KEYEVENTF_KEYUP
  }
}
"@

# 必须设 DPI 感知：否则 150% 缩放下 SetCursorPos/ClientToScreen 的坐标被系统虚拟化，
# 点出去的位置会偏移 1.5 倍，永远命中不了自绘按钮
[U]::SetProcessDPIAware() | Out-Null

$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root 'build\HearizonSync.exe'
$seen = Join-Path $env:USERPROFILE '.hearizon_sync_seen'
Set-Content -Path $seen -Value 'seen' -Encoding ascii -NoNewline
Remove-Item (Join-Path $root 'build\egg_result.txt') -ErrorAction SilentlyContinue

# 单实例机制：残留进程会让新实例直接退出并转发，必须先清干净
Get-Process HearizonSync -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800

$p = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 3
$log = New-Object System.Text.StringBuilder

# MainWindowHandle 常常还是 0，直接按 pid 枚举更稳
$main = [IntPtr]::Zero
for ($i = 0; $i -lt 20 -and $main -eq [IntPtr]::Zero; $i++) {
  $cand = @([U]::OfPid([uint32]$p.Id) | Where-Object { [U]::Title($_) -like 'Hearizon Sync*' })
  if ($cand.Count -ge 1) { $main = $cand[0] } else { Start-Sleep -Milliseconds 300 }
}
if ($main -eq [IntPtr]::Zero) {
  [System.IO.File]::WriteAllText((Join-Path $root 'build\egg_result.txt'), "FAIL: 找不到主窗口`r`n", [System.Text.Encoding]::UTF8)
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Write-Output "no main window"
  exit 1
}
[void]$log.AppendLine("main hwnd=$main title=" + [U]::Title($main))
[U]::SetForegroundWindow($main) | Out-Null
Start-Sleep -Milliseconds 400

# 侧栏宽 378 物理像素，齿轮盒在客户区 (315,27)-(354,66)：点中心
# 用消息点击，窗口在屏幕外或未激活也能命中
$opened = $false
for ($try = 0; $try -lt 3 -and -not $opened; $try++) {
  [U]::SetForegroundWindow($main) | Out-Null
  Start-Sleep -Milliseconds 300
  if ($try -eq 0) { [U]::Click($main, 334, 46) } else { [U]::ClickMsg($main, 334, 46) }
  Start-Sleep -Milliseconds 1500
  $wins = [U]::OfPid([uint32]$p.Id)
  if (@($wins | Where-Object { [U]::Title($_) -like '*设置*' }).Count -ge 1) { $opened = $true }
  else { Start-Sleep -Milliseconds 600 }
}

$wins = [U]::OfPid([uint32]$p.Id)
foreach ($w in $wins) { [void]$log.AppendLine("win: [" + [U]::Title($w) + "] cls=" + [U]::Cls($w)) }

$settings = $wins | Where-Object { [U]::Title($_) -like '*设置*' } | Select-Object -First 1
if (-not $settings) {
  [void]$log.AppendLine("FAIL: 设置窗口没打开（齿轮点击无效）")
} else {
  [void]$log.AppendLine("settings hwnd=$settings  OK 齿轮可点")
  $edits = @([U]::Kids($settings) | Where-Object { [U]::Cls($_) -eq 'Edit' })
  [void]$log.AppendLine("edit count=" + $edits.Count)
  if ($edits.Count -ge 1) {
    # 依次验证三个暗号
    $cases = @(
      @{ text = 'whomadeit'; expect = '作者'; name = '模块2 whomadeit' },
      @{ text = '山有木兮木有枝'; expect = '山有木兮'; name = '模块2-1 古诗' },
      @{ text = '致谢名单'; expect = '致谢'; name = '模块2-2 致谢名单' }
    )
    foreach ($c in $cases) {
      $e = $edits[0]
      [U]::SendMessageW($e, 0x000C, [IntPtr]0, $c.text) | Out-Null
      Start-Sleep -Milliseconds 250
      # 回车必须走 SendMessage 到被拖类化的 edit，keybd_event 依赖前台焦点不可靠
      [U]::SendMessageW($e, 0x0100, [IntPtr]0x0D, [IntPtr]0x001C0001) | Out-Null
      Start-Sleep -Milliseconds 120
      [U]::SendMessageW($e, 0x0101, [IntPtr]0x0D, [IntPtr]0xC01C0001) | Out-Null
      Start-Sleep -Milliseconds 1400
      $after = [U]::OfPid([uint32]$p.Id)
      $titles = @($after | ForEach-Object { [U]::Title($_) })
      $hit = $titles | Where-Object { $_ -like ('*' + $c.expect + '*') }
      $verdict = if ($hit) { 'PASS' } else { 'FAIL' }
      [void]$log.AppendLine(($c.name + ' -> ' + $verdict + ' | 窗口: ' + ($titles -join ' / ')))
      # 关掉弹出的卡片：直接给它发 ESC / WM_CLOSE，不依赖前台焦点
      if ($hit) {
        $card = $after | Where-Object { [U]::Title($_) -like ('*' + $c.expect + '*') } | Select-Object -First 1
        [U]::SendMessageW($card, 0x0100, [IntPtr]0x1B, [IntPtr]0x00010001) | Out-Null
        Start-Sleep -Milliseconds 300
        [U]::SendMessageW($card, 0x0010, [IntPtr]0, [IntPtr]0) | Out-Null   # WM_CLOSE
        Start-Sleep -Milliseconds 400
      }
      $wins2 = [U]::OfPid([uint32]$p.Id)
      $settings = $wins2 | Where-Object { [U]::Title($_) -like '*设置*' } | Select-Object -First 1
      if (-not $settings) { [void]$log.AppendLine('设置窗口没了，中止'); break }
      $edits = @([U]::Kids($settings) | Where-Object { [U]::Cls($_) -eq 'Edit' })
    }
  }
}

Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
[System.IO.File]::WriteAllText((Join-Path $root 'build\egg_result.txt'), $log.ToString(), [System.Text.Encoding]::UTF8)
Write-Output "done"
