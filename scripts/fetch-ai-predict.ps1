# 把 AI 插件源码铺到 librime\plugins\ai-predict，供 build.bat rime 编进 rime.dll。
#
# librime 里这个位置记的是指向仓库外克隆的符号链接（绝对路径，mac 上的便利），
# Windows 检出后会退化成一个内容为路径的普通文件。不换成真目录的话，
# librime\plugins\CMakeLists.txt 的目录扫描扫不到它，rime.dll 会静默编成没有 AI 的样子。
#
# 用法：
#   powershell -File scripts\fetch-ai-predict.ps1                       # 用同级克隆 ..\..\bangke-ai-predict
#   powershell -File scripts\fetch-ai-predict.ps1 -Source D:\bk-ai.bundle -Ref feat/weasel-parity
param(
  [string]$Source = "",
  [string]$Ref = ""
)
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [Text.Encoding]::UTF8

$repo = Split-Path $PSScriptRoot -Parent
$target = Join-Path $repo 'librime\plugins\ai-predict'

function Fail([string]$msg) { Write-Error $msg; exit 1 }

if (-not (Test-Path (Join-Path $target 'CMakeLists.txt'))) {
  # 占位可能是普通文件（未解析的符号链接）或空目录，两种都要先清掉
  if (Test-Path $target) {
    $item = Get-Item $target -Force
    if ($item.PSIsContainer) { Remove-Item $target -Recurse -Force }
    else { Remove-Item $target -Force }
  }

  if (-not $Source) {
    # 同级克隆是最省事的来源：本机开发时两个仓库并排放
    $sibling = Join-Path (Split-Path $repo -Parent) 'bangke-ai-predict'
    if (Test-Path (Join-Path $sibling '.git')) { $Source = $sibling }
  }
  if (-not $Source) {
    Fail "没有可用的插件来源。用 -Source 指定 bundle / 本地克隆 / git URL，或把 bangke-ai-predict 克隆到 $repo 的同级目录。"
  }
  if (-not (Test-Path $Source) -and $Source -notmatch '^(https?|git|ssh)://') {
    Fail "插件来源不存在：$Source"
  }

  "铺料：$Source -> $target"
  git clone --quiet $Source $target
  if ($LASTEXITCODE -ne 0) { Fail "git clone 失败：$Source" }

  if ($Ref) {
    git -C $target checkout --quiet $Ref
    if ($LASTEXITCODE -ne 0) { Fail "git checkout $Ref 失败" }
  }
}

$head = git -C $target rev-parse --short HEAD
$branch = git -C $target rev-parse --abbrev-ref HEAD
"插件就位：$target @ $head ($branch)"
