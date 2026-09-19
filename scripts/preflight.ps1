# 蚌壳拼音部署自检：验证三件套二进制、运行态、模型与配置
# 用法：powershell -File scripts\preflight.ps1 [-ExpectBangke <sha>] [-ExpectLibrime <sha>]
param(
  [string]$ExpectBangke = "",
  [string]$ExpectLibrime = ""
)
$ErrorActionPreference = 'Continue'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$repo = Split-Path $PSScriptRoot -Parent
$pf = 'C:\Program Files\Bangke Pinyin'
$fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
  $mark = if ($ok) { '[PASS]' } else { $script:fail++; '[FAIL]' }
  "{0} {1}  {2}" -f $mark, $name, $detail
}

# 字节级子串搜索：默认窄(ANSI)，-Wide 走 UTF-16
function FindBytes([string]$file, [string]$needle, [switch]$Wide) {
  if (-not (Test-Path $file)) { return $false }
  $enc = if ($Wide) { [Text.Encoding]::Unicode } else { [Text.Encoding]::ASCII }
  $b = [IO.File]::ReadAllBytes($file)
  $n = $enc.GetBytes($needle)
  $step = if ($Wide) { 2 } else { 1 }
  for ($i = 0; $i -le $b.Length - $n.Length; $i += $step) {
    $ok = $true
    for ($j = 0; $j -lt $n.Length; $j++) { if ($b[$i+$j] -ne $n[$j]) { $ok = $false; break } }
    if ($ok) { return $true }
  }
  return $false
}

"=== 蚌壳拼音部署自检 ==="

"`n--- 1. 代码版本 ---"
$w = git -C $repo rev-parse --short HEAD
$l = git -C "$repo\librime" rev-parse --short HEAD
Check "bangke HEAD" ($ExpectBangke -eq "" -or $w -eq $ExpectBangke) "$w$(if ($ExpectBangke) { " (期望 $ExpectBangke)" })"
Check "librime HEAD" ($ExpectLibrime -eq "" -or $l -eq $ExpectLibrime) "$l$(if ($ExpectLibrime) { " (期望 $ExpectLibrime)" })"
$dirty = @(git -C $repo status --porcelain | Where-Object { $_ -notmatch 'librime|output' })
Check "工作树干净" ($dirty.Count -eq 0) "未提交改动已忽略子模块/产物"

"`n--- 2. 核心二进制（安装位置）---"
$rime = Get-Item "$pf\rime.dll" -ErrorAction SilentlyContinue
Check "rime.dll 存在" ([bool]$rime) $(if ($rime) { $rime.LastWriteTime } else { '缺失' })
Check "rime.dll 含 ai-predict 插件" (FindBytes "$pf\rime.dll" 'ai_predict_translator') "组件注册名"
Check "rime.dll 含在途标记" (FindBytes "$pf\rime.dll" 'ai_predict/pending') "插件用它在推理在途时请前端重排"
$tsf = Get-Item 'C:\Windows\System32\bangke.dll' -ErrorAction SilentlyContinue
Check "System32\bangke.dll 存在" ([bool]$tsf) $(if ($tsf) { $tsf.LastWriteTime } else { '缺失' })
Check "bangke.dll 含快照读取" (FindBytes 'C:\Windows\System32\bangke.dll' 'Local\BangkeSnapEvt_' -Wide) "共享内存事件名"
$sv = Get-Item "$pf\BangkeServer.exe" -ErrorAction SilentlyContinue
Check "BangkeServer.exe 存在" ([bool]$sv) $(if ($sv) { $sv.LastWriteTime } else { '缺失' })
Check "server 含快照推送" (FindBytes "$pf\BangkeServer.exe" 'Local\BangkeSnap_' -Wide) "共享内存槽名"
Check "server 含 AI 泵" (FindBytes "$pf\BangkeServer.exe" 'ai_predict/pending') "读在途标记以决定是否重排"

"`n--- 3. 运行态 ---"
$p = Get-Process BangkeServer -ErrorAction SilentlyContinue
Check "服务进程运行中" ([bool]$p) $(if ($p) { "pid=$($p.Id) session=$($p.SessionId)" } else { '未运行' })
if ($p -and $sv) {
  Check "服务跑的是新二进制" ($p.StartTime -gt $sv.LastWriteTime) "启动 $($p.StartTime.ToString('HH:mm:ss')) > 文件 $($sv.LastWriteTime.ToString('HH:mm:ss'))"
  Check "服务在交互会话" ($p.SessionId -ge 1) "session $($p.SessionId)（0=服务会话，UI推送不可达）"
}
$np = Get-Process notepad -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1
if ($tsf -and $np) {
  Check "最新记事本载入新 dll" ($np.StartTime -gt $tsf.LastWriteTime) "记事本 $($np.StartTime.ToString('HH:mm:ss'))（旧进程需新开窗口）"
}

"`n--- 4. 模型与方案 ---"
$user = "$env:APPDATA\Bangke"
Check "CT2 模型就位" (Test-Path "$user\predict_models\zh-base-ct2-int8\model.bin") "230MB int8"
Check "luna_pinyin 补丁" (Test-Path "$user\luna_pinyin.custom.yaml") "AI 方案补丁"
Check "部署产物 schema" (Test-Path "$user\build\luna_pinyin.schema.yaml") "build 目录"
Check "schema 含 ai_predict" (FindBytes "$user\build\luna_pinyin.schema.yaml" 'ai_predict_translator') "translator 列首"

"`n--- 5. 会话要求 ---"
"无需注销（TSF 按进程加载，新窗口即用新 dll）；无需重装（MSI 注册未动时）"

"`n=== 结果：$(if ($fail -eq 0) { '全部通过 (ALL GREEN)' } else { "$fail 项失败" }) ==="
exit $(if ($fail) { 1 } else { 0 })
