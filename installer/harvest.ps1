# 从 output\ 递归生成 Files.wxs（BangkeFiles ComponentGroup）
# 排除项由 $exclude 定义；bangkex64.dll 由主 wxs 单独装到 System32
param(
  [string]$OutputDir = "output",
  [string]$OutFile = "installer\Files.wxs"
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $OutputDir).Path

# 兜底：Qt(/MD) 依赖 VC 运行库，随程序目录捆绑，防目标机 System32 CRT 异常
if (-not (Test-Path "$root\msvcp140.dll")) {
  $crt = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT\msvcp140.dll' -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1
  if ($crt) {
    Copy-Item (Join-Path $crt.Directory '*.dll') $root
    "harvest: bundled VC runtime from $($crt.Directory)"
  } else {
    "harvest: WARNING - no VC runtime found in output and none to copy"
  }
}

$exclude = @('bangkex64.dll', 'weasel.log', 'BangkeDeployer.exe',
               'WinSparkle.dll', '7z.exe', '7z.dll', 'curl.exe',
               'install.bat', 'uninstall.bat', 'stop_service.bat')
# .dmp 曾把一个 534MB 崩溃转储打进 MSI;.wixpdb/.exp/.lib 是构建副产物
$excludeExt = @('.log', '.pdb', '.old', '.msi', '.dmp', '.bak', '.tmp',
                '.wixpdb', '.exp', '.lib')

$files = Get-ChildItem $root -Recurse -File | Where-Object {
  ($exclude -notcontains $_.Name) -and ($excludeExt -notcontains $_.Extension.ToLower())
}

function DirId([string]$rel) {
  if ($rel -eq '.') { return 'INSTALLDIR' }
  # WiX 标识符只收 A-Za-z0-9_，而目录名什么都可能有：连字符(zh-base-ct2-int8)、
  # 空格，以及 Qt qml 树里的 '+' (QtQuick/Dialogs/quickimpl/qml/+Material 这类)。
  # 一律兜底成下划线；只影响 Id，目录真实 Name 不动
  return 'dir_' + ($rel -replace '[^A-Za-z0-9_]', '_')
}

# 目录树登记：rel 为 output 下的相对路径（'.' 为根）
$dirs = @{}
function EnsureDir([string]$rel) {
  if ($rel -eq '.') { return }
  $did = DirId $rel
  if (-not $dirs.ContainsKey($did)) {
    $pp = Split-Path $rel -Parent
    if (-not $pp) { $pp = '.' }
    EnsureDir $pp
    $dirs[$did] = @{ Name = (Split-Path $rel -Leaf); Parent = (DirId $pp) }
  }
}

function Esc([string]$s) { return [System.Security.SecurityElement]::Escape($s) }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine('<?xml version="1.0" encoding="utf-8"?>')
[void]$sb.AppendLine('<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">')
[void]$sb.AppendLine('  <Fragment>')
[void]$sb.AppendLine('    <DirectoryRef Id="INSTALLDIR">')

# 深度优先,父元素嵌套包含子元素:WiX 按嵌套关系定目录父子,
# 平铺输出会把二级以上目录全部落到安装根(opencc 曾因此装到根而非 data\opencc)
$emitted = @{}
function EmitDirTree([string]$did, [int]$depth) {
  $kids = @($dirs.Keys | Where-Object { $dirs[$_].Parent -eq $did })
  foreach ($k in $kids) {
    $d = $dirs[$k]
    $indent = '      ' + ('  ' * $depth)
    $script:emitted[$k] = $true
    $grandkids = @($dirs.Keys | Where-Object { $dirs[$_].Parent -eq $k })
    if ($grandkids.Count -gt 0) {
      [void]$script:sb.AppendLine("$indent<Directory Id=""$k"" Name=""$(Esc $d.Name)"">")
      EmitDirTree $k ($depth + 1)
      [void]$script:sb.AppendLine("$indent</Directory>")
    } else {
      [void]$script:sb.AppendLine("$indent<Directory Id=""$k"" Name=""$(Esc $d.Name)"" />")
    }
  }
}

$comps = New-Object System.Text.StringBuilder
$i = 0
foreach ($f in $files) {
  $rel = $f.FullName.Substring($root.Length + 1)
  $parent = Split-Path $rel -Parent
  if (-not $parent) { $parent = '.' }
  EnsureDir $parent
  $i++
  $src = $f.FullName -replace '/', '\'
  $did = DirId $parent
  [void]$comps.AppendLine("      <Component Id=""comp$i"" Directory=""$did"" Guid=""*"">")
  [void]$comps.AppendLine("        <File Id=""file$i"" Source=""$(Esc $src)"" Name=""$(Esc $f.Name)"" KeyPath=""yes"" />")
  [void]$comps.AppendLine("      </Component>")
}
EmitDirTree 'INSTALLDIR' 0
[void]$sb.AppendLine('    </DirectoryRef>')
[void]$sb.AppendLine("    <ComponentGroup Id=""BangkeFiles"">")
[void]$sb.Append($comps.ToString())
[void]$sb.AppendLine('    </ComponentGroup>')
[void]$sb.AppendLine('  </Fragment>')
[void]$sb.AppendLine('</Wix>')

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($OutFile, $sb.ToString(), $utf8NoBom)
"harvest: $($files.Count) files -> $OutFile"
