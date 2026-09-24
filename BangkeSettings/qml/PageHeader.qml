import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 页头:标题(title2 加粗) + 一句说明(caption 次要色)。放在滚动区外,
// 翻页时它不动。
ColumnLayout {
  property alias title: titleText.text
  property alias sub: subText.text

  spacing: 4

  Txt {
    id: titleText
    font.pixelSize: Theme.titleSize
    font.weight: Font.DemiBold
    Layout.fillWidth: true
  }

  Txt {
    id: subText
    color: Theme.ink2
    font.pixelSize: Theme.subSize
    visible: text.length > 0
    Layout.fillWidth: true
  }
}
