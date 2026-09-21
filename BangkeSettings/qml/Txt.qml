import QtQuick
import Bangke.Pages

// 文字默认值只有这一份。中文界面里 Text 必须显式指定字体族 —— Windows 上 Text 的
// 默认字体是 Segoe UI,中文会掉进后备字体,那就是"廉价感"的来源之一。
Text {
  font.family: Theme.fontFamily
  font.pixelSize: Theme.bodySize
  color: Theme.ink
  renderType: Text.NativeRendering
  wrapMode: Text.NoWrap
}
