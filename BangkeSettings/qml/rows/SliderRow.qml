import QtQuick
import QtQuick.Controls
import Bangke.Pages

// mac 滑杆行(SettingsSlider):标签(+问号) + 滑杆 + 尾部 40px 宽右对齐的
// 数值(step < 1 显示两位小数,与 mac 的 displayValue 同规则)。
RowShell {
  id: row
  property real value: 0
  property real min: 0
  property real max: 100
  property real step: 1
  signal edited(real value)

  controlWidth: slider.implicitWidth + valueLabel.width + 8

  readonly property int decimals: row.step >= 1 ? 0 : 2

  Slider {
    id: slider
    anchors.right: valueLabel.left
    anchors.rightMargin: 8
    anchors.verticalCenter: parent.verticalCenter
    implicitWidth: 170
    implicitHeight: 22
    from: row.min
    to: row.max
    stepSize: row.step
    value: row.value
    onMoved: row.edited(value)

    background: Rectangle {
      x: slider.leftPadding
      y: slider.topPadding + slider.availableHeight / 2 - height / 2
      width: slider.availableWidth
      height: 4
      radius: 2
      color: Theme.dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0, 0, 0, 0.12)

      Rectangle {
        width: slider.visualPosition * parent.width
        height: parent.height
        radius: 2
        color: Theme.accent
      }
    }

    handle: Rectangle {
      x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
      y: slider.topPadding + slider.availableHeight / 2 - height / 2
      width: 16
      height: 16
      radius: 8
      color: "#FFFFFF"
      border.width: 1
      border.color: slider.pressed ? Theme.accent
                                   : Theme.dark ? Qt.rgba(1, 1, 1, 0.3)
                                                : Qt.rgba(0, 0, 0, 0.18)
    }
  }

  Txt {
    id: valueLabel
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    width: 40
    horizontalAlignment: Text.AlignRight
    color: Theme.ink2
    text: row.value.toFixed(row.decimals)
  }
}
