import QtQuick
import Bangke.Pages

// 设置行的公共骨架(mac LabeledContent):标签(+问号说明)居左,控件贴右缘
// 垂直居中,行顶一条内缩发丝线(从标签起笔到卡片右缘)。
// 控件宽度由子类报进 controlWidth —— 壳子不认识具体控件。
Item {
  id: shell
  property string label: ""
  property string helper: ""
  property bool showDivider: true
  property real controlWidth: 0

  implicitHeight: Theme.rowHeight

  Hairline {
    x: 16
    width: parent.width - 16
    anchors.top: parent.top
    visible: shell.showDivider
    color: Theme.hairSoft
  }

  Row {
    id: labels
    spacing: 4
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.rightMargin: shell.controlWidth + 14
    anchors.verticalCenter: parent.verticalCenter

    Txt {
      text: shell.label
      width: Math.min(labels.width - (hintIcon.visible ? hintIcon.width + 4 : 0),
                      implicitWidth)
      anchors.verticalCenter: parent.verticalCenter
      elide: Text.ElideRight
    }

    HintIcon {
      id: hintIcon
      hint: shell.helper
      visible: shell.helper.length > 0
      anchors.verticalCenter: parent.verticalCenter
    }
  }
}
