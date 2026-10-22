import QtQuick
import QtQuick.Controls
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
    x: 0
    width: parent.width
    anchors.top: parent.top
    visible: shell.showDivider
    color: Theme.hairSoft
  }

  Txt {
    id: labelText
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.rightMargin: shell.controlWidth + 18
    anchors.verticalCenter: parent.verticalCenter
    text: shell.label
    elide: Text.ElideRight
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && (shell.helper.length > 0 || labelText.truncated)
    ToolTip.delay: 600
    ToolTip.text: shell.helper.length > 0 ? shell.helper : shell.label
  }
}
