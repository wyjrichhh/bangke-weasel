import QtQuick
import Bangke.Pages

// 文字默认值只有这一份。西文 Segoe UI,中文经字形回退落到微软雅黑
// (回退链由 main.cpp 的应用字体携带)。
// 不用 NativeRendering:GDI 文本在非整数缩放/RDP 下发毛,
// Qt 自绘的文本亚像素更顺滑 —— 「光滑度」的一部分。
Text {
  font.family: Theme.fontFamily
  font.pixelSize: Theme.bodySize
  color: Theme.ink
  wrapMode: Text.NoWrap
}
