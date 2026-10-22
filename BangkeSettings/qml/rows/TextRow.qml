import QtQuick
import QtQuick.Controls
import Bangke.Pages

// 单行文本行。TextField 自己就是 TextInput(颜色字号直接设在控件上)。
RowShell {
  id: row
  property string value: ""
  property bool readOnly: false
  property real fieldWidth: 220
  signal edited(string value)

  controlWidth: row.fieldWidth

  TextField {
    id: field
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    implicitWidth: row.fieldWidth
    implicitHeight: Theme.controlHeight
    topPadding: 0
    bottomPadding: 0
    leftPadding: 8
    rightPadding: 8
    text: row.value
    readOnly: row.readOnly
    color: row.readOnly ? Theme.ink2 : Theme.ink
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize
    selectionColor: Theme.accent
    selectedTextColor: Theme.accentText
    selectByMouse: true
    onTextEdited: row.edited(text)
    // 键入会顶掉 text 上的绑定,编辑结束再挂回去 —— 否则外部重载后这里显示旧值
    onEditingFinished: field.text = Qt.binding(function () {
      return row.value
    })

    background: Field {
      focused: field.activeFocus && !field.readOnly
    }
  }
}
