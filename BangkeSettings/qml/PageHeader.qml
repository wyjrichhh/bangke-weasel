import QtQuick
import Bangke.Pages

// 页面只保留一个主标题，解释性文字放到相关控件的悬停提示中。
Txt {
  id: heading
  property alias title: heading.text
  font.pixelSize: Theme.titleSize
  font.weight: Font.DemiBold
}
