# 生成单文件分发 exe:图形安装器 + 最新 MSI + Qt 运行库 全部嵌进
# 一个原生引导壳(installer\BangkeBoot)。
# 用法: 在仓库根,先完成 build.bat settings / build_installer.bat / build_msi.bat,
#       再 powershell -File installer\make_boot.ps1
# 产物: dist\BangkeSetup-<版本>-win64.exe —— 单文件,双击即图形安装
$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root "output"

$msi = Get-ChildItem $output -Filter "BangkeSetup-*.msi" |
  Sort-Object Name | Select-Object -Last 1
if (-not $msi) { throw "output 下没有 BangkeSetup-*.msi,先跑 installer\build_msi.bat" }
$version = $msi.BaseName -replace '^BangkeSetup-', '' -replace '-x64$', ''

# ---- 收集载荷清单(引导壳按相对路径解包) ----
$payload = [System.Collections.Generic.List[string]]::new()
$payload.Add("BangkeInstaller.exe")
$payload.Add($msi.Name)
# dxcompiler/dxil 是 Qt6Gui 的直接依赖,漏了即启动即崩(windeployqt 铺过它们)
# 设置程序改成 QML 之后还多出 Qt Quick 那一串 DLL:Qt6Qml 起手就解析 qml 树,
# 缺一个就是启动即白屏。列的是超集,Test-Path 兜底 —— 没铺出来的自然跳过
foreach ($n in @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Network.dll",
                 "Qt6Qml.dll", "Qt6QmlModels.dll", "Qt6QmlWorkerScript.dll",
                 "Qt6QmlMeta.dll", "Qt6Quick.dll", "Qt6QuickControls2.dll",
                 "Qt6QuickControls2Impl.dll", "Qt6QuickTemplates2.dll",
                 "Qt6QuickLayouts.dll", "Qt6QuickEffects.dll",
                 "Qt6QuickDialogs2.dll", "Qt6QuickDialogs2QuickImpl.dll",
                 "Qt6QuickDialogs2Utils.dll",
                 "dxcompiler.dll", "dxil.dll")) {
  if (Test-Path (Join-Path $output $n)) { $payload.Add($n) }
}
# qml 是 Qt Quick 的模块树(QtQuick/Controls/Layouts/Effects/Dialogs...),
# 引导壳按相对路径整棵收,漏了它 exe 里的 import 一个都解析不到
foreach ($d in @("platforms", "styles", "imageformats", "iconengines", "tls", "networkinformation", "generic", "qml")) {
  $p = Join-Path $output $d
  if (Test-Path $p) {
    Get-ChildItem $p -Recurse -File | ForEach-Object {
      $payload.Add($_.FullName.Substring($output.Length + 1))
    }
  }
}
# VC 运行库(Qt 为 /MD)
Get-ChildItem $output -File | Where-Object {
  $_.Name -match '^(msvcp|vcruntime|vccorlib|concrt|ucrtbase)' } | ForEach-Object {
  $payload.Add($_.Name)
}
if ($payload -notcontains "BangkeInstaller.exe") { throw "output\BangkeInstaller.exe 缺失" }

# ---- 生成资源脚本:IDR_MANIFEST(100) = 清单,101.. = 文件 ----
$build = Join-Path $PSScriptRoot "BangkeBoot\build"
New-Item $build -ItemType Directory -Force | Out-Null
$id = 100
$manifest = New-Object System.Text.StringBuilder
$rc = New-Object System.Text.StringBuilder
Copy-Item (Join-Path $root "resource\bangke.ico") $build -Force
Copy-Item (Join-Path $PSScriptRoot "BangkeBoot\app.manifest") $build -Force
[void]$rc.AppendLine("// 自动生成:make_boot.ps1,勿手改")
[void]$rc.AppendLine("1 ICON `"bangke.ico`"")
[void]$rc.AppendLine("1 24 `"app.manifest`"")  # RT_MANIFEST:requireAdministrator
foreach ($rel in $payload) {
  $id++
  [void]$manifest.AppendLine("$id|$rel")
  [void]$rc.AppendLine("$id RCDATA `"$($output.Replace('\', '\\'))\\$($rel.Replace('\', '\\'))`"")
}
[void]$rc.AppendLine("100 RCDATA `"$build\payload.manifest`"")
[IO.File]::WriteAllText("$build\payload.manifest", $manifest.ToString(),
                        (New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText("$build\boot_payload.rc", $rc.ToString(),
                        (New-Object Text.UTF8Encoding($false)))

# ---- 编译引导壳 ----
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
$devcmd = Join-Path $vs "Common7\Tools\VsDevCmd.bat"
# 在构建目录内编译,避开 /Fo"路径\ 末尾反斜杠吃引号的坑;
# .rc 需 rc.exe 先编成 .res,再随 cl 链接
$exe = Join-Path $build "BangkeBoot.exe"
$cmd = "`"$devcmd`" -arch=amd64 -host_arch=amd64 && cd /d `"$($build.Replace('\', '\'))`" && " +
       "rc /nologo /foboot.res boot_payload.rc && " +
       "cl /nologo /O2 /MT /utf-8 /std:c++17 /DUNICODE /D_UNICODE `"$($PSScriptRoot.Replace('\', '\'))\BangkeBoot\main.cpp`" boot.res /FeBangkeBoot.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shlwapi.lib shell32.lib"
$out = cmd /c $cmd 2>&1
$code = $LASTEXITCODE
if ($code -ne 0 -or -not (Test-Path $exe)) {
  $out | Select-Object -Last 10 | ForEach-Object { Write-Output $_ }
  throw "引导壳编译失败 (exit=$code)"
}

New-Item (Join-Path $root "dist") -ItemType Directory -Force | Out-Null
$dist = Join-Path $root "dist\BangkeSetup-$version-win64.exe"
Copy-Item $exe $dist -Force
Write-Output ("dist: " + $dist + "  " + [math]::Round((Get-Item $dist).Length / 1MB, 1) + "MB (单文件,双击即装)")
