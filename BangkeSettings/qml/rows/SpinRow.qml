import QtQuick
import QtQuick.Controls
import Bangke.Pages

// 数字行:可键入的窄框 + 右侧上下两个小三角(Basic style 默认那两个太大)。
// 后缀(如 " ms")跟值一起显示在框里,所以 validator 不能再用 IntValidator:空格会被挡掉。
RowShell {
  id: row
  property int value: 0
  property int min: 0
  property int max: 100
  property string suffix: ""
  property real fieldWidth: 96
  signal edited(int value)

  controlWidth: row.fieldWidth

  SpinBox {
    id: spin
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    implicitWidth: row.fieldWidth
    implicitHeight: Theme.controlHeight
    leftPadding: 11
    rightPadding: 4
    from: row.min
    to: row.max
    value: row.value
    editable: true
    onValueModified: row.edited(value)

    textFromValue: function (value) {
      return row.suffix.length > 0 ? String(value) + row.suffix : String(value)
    }

    valueFromText: function (text) {
      var digits = String(text).replace(/[^-0-9]/g, "")
      var parsed = parseInt(digits, 10)
      return isNaN(parsed) ? spin.value : parsed
    }

    validator: RegExpValidator { regExp: /[-0-9 ]*/ }

    contentItem: TextInput {
      text: spin.textFromValue(spin.value, spin.locale)
      font.family: Theme.fontFamily
      font.pixelSize: Theme.bodySize
      color: Theme.ink
      selectionColor: Theme.accent
      selectedTextColor: Theme.onAccent
      verticalAlignment: TextInput.AlignVCenter
      readOnly: !spin.editable
      validator: spin.validator
      inputMethodHints: Qt.ImhFormattedNumbersOnly
      selectByMouse: true
      // 键入会顶掉 text 上的绑定,编辑结束再挂回去,顺带把 "12abc" 归整成 "12 ms"
      onEditingFinished: {
        spin.value = spin.valueFromText(text, spin.locale)
        text = Qt.binding(function () {
          return spin.textFromValue(spin.value, spin.locale)
        })
      }
    }

    up.indicator: Item {
      implicitWidth: 22

      Txt {
        anchors.centerIn: parent
        text: "▲"
        color: Theme.ink3
        font.pixelSize: 8
      }
    }

    down.indicator: Item {
      implicitWidth: 22

      Txt {
        anchors.centerIn: parent
        text: "▼"
        color: Theme.ink3
        font.pixelSize: 8
      }
    }

    background: Field { focused: spin.activeFocus }
  }
}
