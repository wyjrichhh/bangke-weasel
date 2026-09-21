import QtQuick
import QtQuick.Controls
import Bangke.Pages

// 按钮:primary 实心主色、secondary 白底发丝边。Basic style 的默认外壳全部丢掉,
// 圆角/字重/投影由 Theme 说了算。
Button {
  id: btn
  property bool primary: false

  implicitHeight: Theme.controlHeight
  implicitWidth: Math.max(76, contentItem.implicitWidth + leftPadding + rightPadding)
  leftPadding: 18
  rightPadding: 18
  font.family: Theme.fontFamily
  font.pixelSize: Theme.bodySize
  font.weight: Font.DemiBold

  readonly property color faceColor: !btn.enabled
                                    ? (btn.primary ? Theme.accentSoft : Theme.hairSoft)
                                    : btn.primary
                                      ? (btn.pressed ? Qt.darker(Theme.accent, 1.12)
                                                     : btn.hovered ? Qt.lighter(Theme.accent, 1.06)
                                                                   : Theme.accent)
                                      : (btn.pressed ? Theme.hairSoft
                                                     : btn.hovered ? Theme.hairSoft : Theme.surface)

  contentItem: Txt {
    text: btn.text
    font: btn.font
    color: btn.primary ? Theme.onAccent : Theme.ink2
    opacity: btn.enabled ? 1 : 0.55
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
  }

  background: Rectangle {
    radius: Theme.fieldRadius
    color: btn.faceColor
    border.width: btn.primary ? 0 : 1
    border.color: Theme.hair
    Behavior on color {
      ColorAnimation { duration: 100 }
    }
  }
}
