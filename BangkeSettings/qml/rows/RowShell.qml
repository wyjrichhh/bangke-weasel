import QtQuick
import Bangke.Pages

// 设置行的公共骨架:标签(+灰色副文案)居左,控件贴右缘垂直居中,行顶一条发丝线。
// 控件宽度由子类报进 controlWidth —— 壳子不认识具体控件。
Item {
  id: shell
  property string label: ""
  property string helper: ""
  property bool showDivider: true
  property real controlWidth: 0

  implicitHeight: Math.max(Theme.rowHeight, labels.implicitHeight + 16)

  Hairline {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    visible: shell.showDivider
  }

  Column {
    id: labels
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.rightMargin: shell.controlWidth + 18
    anchors.verticalCenter: parent.verticalCenter
    spacing: 1

    Txt {
      text: shell.label
      width: parent.width
      elide: Text.ElideRight
    }

    Txt {
      text: shell.helper
      visible: shell.helper.length > 0
      color: Theme.ink3
      font.pixelSize: Theme.hintSize
      width: parent.width
      elide: Text.ElideRight
    }
  }
}
