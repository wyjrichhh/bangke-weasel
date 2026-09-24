import QtQuick
import Bangke.Pages

// 输入类控件共用的外壳:mac 文本框的观感 —— 纯色底、6 圆角、细边框,
// 聚焦时强调色描边 + 外圈柔环(环用一层外扩透明矩形画,Rectangle 没有
// box-shadow)。
Rectangle {
  id: field
  property bool focused: false
  implicitWidth: 96
  implicitHeight: 26
  radius: Theme.fieldRadius
  color: Theme.field
  border.width: 1
  border.color: focused ? Theme.accent
                        : Theme.dark ? Qt.rgba(1, 1, 1, 0.18)
                                     : Qt.rgba(0, 0, 0, 0.15)

  Rectangle {
    anchors.fill: parent
    anchors.margins: -3
    radius: field.radius + 3
    color: "transparent"
    border.width: 3
    border.color: Theme.accentSoft
    visible: field.focused
    z: -1
  }
}
