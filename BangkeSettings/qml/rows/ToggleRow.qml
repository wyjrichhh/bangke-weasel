import QtQuick
import QtQuick.Controls
import Bangke.Pages

// mac 开关行:轨道 38×22,关闭态是中性灰半透明,开启态强调色。
// 轨道与滑块都是自己画的,Basic style 的形状和字重不符合稿子。
RowShell {
  id: row
  property bool value: false
  signal edited(bool value)

  controlWidth: toggle.implicitWidth

  Switch {
    id: toggle
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    implicitWidth: 38
    implicitHeight: 22
    checked: row.value
    padding: 0
    spacing: 0
    onToggled: row.edited(checked)

    indicator: Rectangle {
      implicitWidth: 38
      implicitHeight: 22
      radius: height / 2
      color: toggle.checked ? Theme.accent
                            : Theme.dark ? Qt.rgba(1, 1, 1, 0.16)
                                         : Qt.rgba(0, 0, 0, 0.14)

      Behavior on color {
        ColorAnimation { duration: 120 }
      }

      Rectangle {
        width: 18
        height: 18
        radius: 9
        y: 2
        x: toggle.checked ? parent.width - width - 2 : 2
        color: "#FFFFFF"

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
