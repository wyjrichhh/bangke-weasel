import QtQuick
import QtQuick.Controls
import Bangke.Pages

// 数字行:可键入的窄框 + 右侧上下两个小三角(Basic style 默认那两个太大)。
// 后缀(如 " ms")跟值一起显示在框里,IntValidator 会把空格也挡掉,而 Qt6 已没有
// RegExpValidator —— 所以不挂 validator,改在键入时过滤。
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
    // 给右侧那对三角留出位置(Basic 本来是按 up.indicator.width 算的,这里显式给)
    rightPadding: 26
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

    contentItem: TextInput {
      text: spin.textFromValue(spin.value, spin.locale)
      font.family: Theme.fontFamily
      font.pixelSize: Theme.bodySize
      color: Theme.ink
      selectionColor: Theme.accent
      selectedTextColor: Theme.onAccent
      verticalAlignment: TextInput.AlignVCenter
      readOnly: !spin.editable
      inputMethodHints: Qt.ImhFormattedNumbersOnly
      selectByMouse: true
      // 只放行数字、负号与空格(空格留给后缀),顶掉 RegExpValidator 那份
      onTextEdited: {
        var filtered = text.replace(/[^-0-9 ]/g, "")
        if (filtered !== text)
          text = filtered
      }
      // 键入会顶掉 text 上的绑定,编辑结束再挂回去,顺带把 "12abc" 归整成 "12 ms"
      onEditingFinished: {
        spin.value = spin.valueFromText(text, spin.locale)
        text = Qt.binding(function () {
          return spin.textFromValue(spin.value, spin.locale)
        })
      }
    }

    // 指示器的位置得自己给:Basic/Fusion 的 delegate 都自带 x/y/height(模板不摆它们),
    // 换成裸 Item 就丢了 —— 两个会一起塌到 (0,0) 且高度 0,▲▼ 叠在框顶边上成一团黑。
    up.indicator: Item {
      x: spin.width - width - 1
      y: 1
      height: spin.height / 2 - 1
      implicitWidth: 22
      implicitHeight: 10

      Txt {
        anchors.centerIn: parent
        text: "▲"
        color: Theme.ink3
        font.pixelSize: 8
      }
    }

    down.indicator: Item {
      x: spin.width - width - 1
      y: spin.height - height - 1
      height: spin.height / 2 - 1
      implicitWidth: 22
      implicitHeight: 10

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
