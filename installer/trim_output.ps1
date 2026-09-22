# 从 output\ 里剪掉确定不会被加载的东西。清单只此一份:harvest.ps1 与 make_boot.ps1 都是
# 递归收 output\,剪在这里两边自然一致,不会各写一遍排除规则。
#
# 判据是「没有任何路径会加载它」,不是「看起来没用」:
#   - 非 Basic 的 Controls 样式 / NativeStyle:main.cpp 用 QQuickStyle::setStyle("Basic")
#     钉死了样式;BangkeInstaller 是 Widgets,根本不碰 Controls
#   - qmltooling:QML 调试器插件(qmldbg_*),release 不加载
#   - dxcompiler.dll + dxil.dll:只有 D3D12 后端会 LoadLibrary 它们,而 Qt 在 Windows 上
#     未设 QSG_RHI_BACKEND 时默认 D3D11,本程序没有改这个默认。哪天要开 d3d12 记得加回来
#
# 刻意不动 VC 运行库那几个卫星 DLL(concrt140/vccorlib140/msvcp140_*):它们是被 STL
# 按需 LoadLibrary 的,导入表里查不到,证明不了用不上,而一共才 0.8MB,不值得冒险。
param(
  [string]$OutputDir = "output"
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path $OutputDir).Path

$skipDirs = @(
  'qml\QtQuick\Controls\FluentWinUI3',
  'qml\QtQuick\Controls\Fusion',
  'qml\QtQuick\Controls\Imagine',
  'qml\QtQuick\Controls\Material',
  'qml\QtQuick\Controls\Universal',
  'qml\QtQuick\Controls\Windows',
  'qml\QtQuick\NativeStyle',
  'qmltooling'
)
$skipFiles = @('dxcompiler.dll', 'dxil.dll')

$freed = 0
foreach ($d in $skipDirs) {
  $p = Join-Path $root $d
  if (Test-Path $p) {
    $freed += (Get-ChildItem $p -Recurse -File | Measure-Object Length -Sum).Sum
    Remove-Item $p -Recurse -Force
    "trim: -$d"
  }
}
foreach ($f in $skipFiles) {
  $p = Join-Path $root $f
  if (Test-Path $p) {
    $freed += (Get-Item $p).Length
    Remove-Item $p -Force
    "trim: -$f"
  }
}
"trim: freed {0:N0} bytes" -f $freed
