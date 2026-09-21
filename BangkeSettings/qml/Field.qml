import QtQuick
import Bangke.Pages

// 输入类控件共用的外壳:白底 + 发丝边 + 9 圆角,聚焦时主色描边加一圈 22% 的柔环。
// 环用一层外扩的透明矩形画出来 —— Rectangle 没有 box-shadow。
Rectangle {
  id: field
  property bool focused: false
  implicitWidth: 96
  implicitHeight: Theme.controlHeight
  radius: Theme.fieldRadius
  color: Theme.field
  border.width: 1
  border.color: focused ? Theme.accent : Theme.hair

  Rectangle {
    anchors.fill: parent
    anchors.margins: -3
    radius: field.radius + 3
    color: "transparent"
    border.width: 3
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
    visible: field.focused
    z: -1
  }
}
