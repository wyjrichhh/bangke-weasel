import QtQuick
import QtQuick.Controls
import Bangke.Pages

// 开关行。轨道与滑块都是自己画的,Basic style 的形状和字重不符合稿子。
RowShell {
  id: row
  property bool value: false
  signal edited(bool value)

  controlWidth: toggle.implicitWidth

  Switch {
    id: toggle
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    implicitWidth: 46
    implicitHeight: 26
    checked: row.value
    padding: 0
    spacing: 0
    onToggled: row.edited(checked)

    indicator: Rectangle {
      implicitWidth: 46
      implicitHeight: 26
      radius: height / 2
      color: toggle.checked ? Theme.accent : Theme.hair

      Behavior on color {
        ColorAnimation { duration: 120 }
      }

      Rectangle {
        width: 20
        height: 20
        radius: 10
        y: 3
        x: toggle.checked ? parent.width - width - 3 : 3
        color: toggle.checked && Theme.dark ? "#08201C" : "#FFFFFF"

        Behavior on x {
          NumberAnimation {
            duration: 120
            easing.type: Easing.OutQuad
          }
        }
      }
    }
  }
}
