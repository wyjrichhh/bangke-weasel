# Apply repository defaults to a staged data tree, including cached/offline builds.
param([string]$DataDir = 'output\data')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$root = (Resolve-Path $DataDir).Path
Copy-Item (Join-Path $repo 'data\weasel.yaml') $root -Force
$utf8 = New-Object Text.UTF8Encoding($false)
foreach ($item in @(@('rime_ice', '蚌壳-雾凇'), @('luna_pinyin', '蚌壳-明月'))) {
  $file = Join-Path $root ($item[0] + '.schema.yaml')
  if (-not (Test-Path $file)) { throw "Missing required schema: $file" }
  $text = [IO.File]::ReadAllText($file)
  $text = [regex]::Replace($text, '(?m)^  name:.*$', ('  name: ' + $item[1]))
  if ($item[0] -eq 'luna_pinyin' -and $text -notmatch '(?m)^  spelling_hints:') {
    $text = [regex]::Replace($text, '(?m)^translator:\r?$', "translator:`n  spelling_hints: 8")
  }
  [IO.File]::WriteAllText($file, $text, $utf8)
}
