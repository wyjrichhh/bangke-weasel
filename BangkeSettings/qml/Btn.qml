import QtQuick
import QtQuick.Controls
import Bangke.Pages

// mac 按钮:普通 = 白底细边框,prominent = 强调色实心白字。
// Basic style 的默认外壳全部丢掉,圆角/字重由 Theme 说了算。
Button {
  id: btn
  property bool primary: false

  implicitHeight: Theme.controlHeight
  implicitWidth: Math.max(70, contentItem.implicitWidth + leftPadding + rightPadding)
  leftPadding: 16
  rightPadding: 16
  font.family: Theme.fontFamily
  font.pixelSize: Theme.bodySize

  readonly property color faceColor: !btn.enabled
                                    ? (btn.primary ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.4)
                                                   : Theme.hairSoft)
                                    : btn.primary
                                      ? (btn.pressed ? Qt.darker(Theme.accent, 1.12)
                                                     : btn.hovered ? Qt.lighter(Theme.accent, 1.06)
                                                                   : Theme.accent)
                                      : (btn.pressed || btn.hovered
                                             ? (Theme.dark ? Qt.rgba(1, 1, 1, 0.13)
                                                           : Qt.rgba(0, 0, 0, 0.05))
                                             : (Theme.dark ? Qt.rgba(1, 1, 1, 0.08)
                                                           : Theme.surface))

  contentItem: Txt {
    text: btn.text
    font: btn.font
    color: btn.primary ? Theme.accentText : Theme.ink
    opacity: btn.enabled ? 1 : 0.55
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
  }

  // 按压微缩放:mac 按钮的下压物理感
  scale: pressed ? 0.97 : 1
  Behavior on scale {
    NumberAnimation { duration: 100; easing.type: Easing.OutQuad }
  }

  background: Rectangle {
    radius: Theme.fieldRadius
    color: btn.faceColor
    border.width: btn.primary ? 0 : 1
    border.color: Theme.dark ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.12)
    Behavior on color {
      ColorAnimation { duration: 100 }
    }
  }
}
