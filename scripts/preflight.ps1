# 蚌壳拼音部署自检：验证三件套二进制、运行态、模型与配置
# 用法：powershell -File scripts\preflight.ps1 [-ExpectBangke <sha>] [-ExpectLibrime <sha>]
param(
  [string]$ExpectBangke = "",
  [string]$ExpectLibrime = "",
  [switch]$AllowDirty
)
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$repo = Split-Path $PSScriptRoot -Parent
$pf = Join-Path $env:ProgramFiles "Bangke Pinyin"
$fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
  $mark = if ($ok) { '[PASS]' } else { $script:fail++; '[FAIL]' }
  "{0} {1}  {2}" -f $mark, $name, $detail
}

# Scan binaries in compiled code: PowerShell per-byte loops are prohibitively
# slow for rime.dll. Cache each file once for the related marker checks.
Add-Type @'
public static class BangkeBinarySearch {
  public static bool Contains(byte[] bytes, byte[] needle, int step) {
    if (needle.Length == 0) return true;
    for (int i = 0; i <= bytes.Length - needle.Length; i += step) {
      if (bytes[i] != needle[0]) continue;
      int j = 1;
      while (j < needle.Length && bytes[i + j] == needle[j]) ++j;
      if (j == needle.Length) return true;
    }
    return false;
  }
}
'@
$binaryCache = @{}
function FindBytes([string]$file, [string]$needle, [switch]$Wide) {
  if (-not (Test-Path $file)) { return $false }
  if (-not $binaryCache.ContainsKey($file)) { $binaryCache[$file] = [IO.File]::ReadAllBytes($file) }
  $enc = if ($Wide) { [Text.Encoding]::Unicode } else { [Text.Encoding]::ASCII }
  $step = if ($Wide) { 2 } else { 1 }
  return [BangkeBinarySearch]::Contains($binaryCache[$file], $enc.GetBytes($needle), $step)
}

"=== 蚌壳拼音部署自检 ==="

"`n--- 1. 代码版本 ---"
$w = git -C $repo rev-parse --short HEAD
$l = git -C "$repo\librime" rev-parse --short HEAD
Check "bangke HEAD" ($ExpectBangke -eq "" -or $w -eq $ExpectBangke) "$w$(if ($ExpectBangke) { " (期望 $ExpectBangke)" })"
Check "librime HEAD" ($ExpectLibrime -eq "" -or $l -eq $ExpectLibrime) "$l$(if ($ExpectLibrime) { " (期望 $ExpectLibrime)" })"
$dirty = @(git -C $repo status --porcelain | Where-Object { $_ -notmatch 'librime|output' })
Check "源码状态" ($AllowDirty -or $dirty.Count -eq 0) "dirty=$($dirty.Count); AllowDirty=$AllowDirty（开发验收可显式允许未提交源码）"

"`n--- 2. 核心二进制（安装位置）---"
$rime = Get-Item "$pf\rime.dll" -ErrorAction SilentlyContinue
Check "rime.dll 存在" ([bool]$rime) $(if ($rime) { $rime.LastWriteTime } else { '缺失' })
Check "rime.dll 含 ai-predict 插件" (FindBytes "$pf\rime.dll" 'ai_predict_translator') "组件注册名"
Check "rime.dll 含在途标记" (FindBytes "$pf\rime.dll" 'ai_predict/pending') "插件用它在推理在途时请前端重排"
Check "rime.dll 含 lua 插件" (FindBytes "$pf\rime.dll" 'lua_translator') "雾凇拼音的日期/农历/数字全靠它"
$tsf = Get-Item "$env:SystemRoot\System32\bangke.dll" -ErrorAction SilentlyContinue
Check "System32\bangke.dll 存在" ([bool]$tsf) $(if ($tsf) { $tsf.LastWriteTime } else { '缺失' })
Check "bangke.dll 含快照读取" (FindBytes "$env:SystemRoot\System32\bangke.dll" 'Local\BangkeSnapEvt_' -Wide) "共享内存事件名"
$sv = Get-Item "$pf\BangkeServer.exe" -ErrorAction SilentlyContinue
Check "BangkeServer.exe 存在" ([bool]$sv) $(if ($sv) { $sv.LastWriteTime } else { '缺失' })
Check "server 含快照推送" (FindBytes "$pf\BangkeServer.exe" 'Local\BangkeSnap_' -Wide) "共享内存槽名"
Check "server 含 AI 泵" (FindBytes "$pf\BangkeServer.exe" 'ai_predict/pending') "读在途标记以决定是否重排"

"`n--- 3. 运行态 ---"
$p = Get-Process BangkeServer -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -ge 1 } | Sort-Object StartTime -Descending | Select-Object -First 1
Check "服务进程运行中" ([bool]$p) $(if ($p) { "pid=$($p.Id) session=$($p.SessionId)" } else { '未运行' })
if ($p -and $sv) {
  Check "服务来自安装目录" ($p.Path -eq "$pf\BangkeServer.exe") $p.Path
  Check "服务跑的是新二进制" ($p.StartTime -gt $sv.LastWriteTime) "启动 $($p.StartTime.ToString('HH:mm:ss')) > 文件 $($sv.LastWriteTime.ToString('HH:mm:ss'))"
  Check "服务在交互会话" ($p.SessionId -ge 1) "session $($p.SessionId)（0=服务会话，UI推送不可达）"
}
$np = Get-Process notepad -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1
if ($tsf -and $np) {
  Check "最新记事本载入新 dll" ($np.StartTime -gt $tsf.LastWriteTime) "记事本 $($np.StartTime.ToString('HH:mm:ss'))（旧进程需新开窗口）"
}

"`n--- 4. 模型与方案 ---"
$user = "$env:APPDATA\Bangke"
foreach ($schema in @('rime_ice', 'luna_pinyin')) {
  Check "$schema 补丁" (Test-Path "$user\$schema.custom.yaml") "AI 方案补丁"
  $compiled = "$user\build\$schema.schema.yaml"
  Check "$schema 部署产物" (Test-Path $compiled) "build 目录"
  Check "$schema AI translator" (FindBytes $compiled 'ai_predict_translator') "已部署 AI 接线"
  if (Test-Path $compiled) {
    $yaml = [IO.File]::ReadAllText($compiled)
    $match = [regex]::Match($yaml, '(?m)^  model_path:\s*(.+?)\s*$')
    if ($match.Success) {
      $model = $match.Groups[1].Value.Trim().Trim('"').Trim("'")
      if (-not [IO.Path]::IsPathRooted($model)) { $model = Join-Path $user $model }
      foreach ($name in @('model.bin', 'tokenizer.json', 'shared_vocabulary.json')) {
        $file = Get-Item (Join-Path $model $name) -ErrorAction SilentlyContinue
        Check "$schema 模型 $name" ($null -ne $file -and -not $file.PSIsContainer -and $file.Length -gt 0) $model
      }
    } else { Check "$schema 模型路径" $false '部署配置缺少 model_path' }
  }
}

"`n--- 5. 会话要求 ---"
"以安装器完成页为准：若提示需要重启，请保存工作后重启 Windows。"

"`n=== 结果：$(if ($fail -eq 0) { '全部通过 (ALL GREEN)' } else { "$fail 项失败" }) ==="
exit $(if ($fail) { 1 } else { 0 })
