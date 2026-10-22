import QtQuick
import Bangke.Pages

// 实色表单使用平台字体光栅化，避免小字号距离场文字的柔化感。
Text {
  renderType: Text.NativeRendering
  font.family: Theme.fontFamily
  font.pixelSize: Theme.bodySize
  color: Theme.ink
  wrapMode: Text.NoWrap
}
