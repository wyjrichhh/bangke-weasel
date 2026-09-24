import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// mac 侧栏:200 宽、条目 28 高(padding h8 v5)、选中态 accent 22% 圆角 6。
// 背板是 NSVisualEffectView(.sidebar) 的固定色近似(Windows 没有窗口局部
// 毛玻璃通道)。条目与页头文案同一个来源(app.pageTitles),分开写迟早对不上。
Rectangle {
  id: side
  implicitWidth: 200
  color: Theme.sidebarBg

  Rectangle {
    width: 1
    height: parent.height
    anchors.right: parent.right
    color: Theme.hair
  }

  Column {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.leftMargin: 10
    anchors.rightMargin: 10
    anchors.topMargin: 12
    spacing: 4

    Repeater {
      model: app.pageTitles

      delegate: Rectangle {
        id: navItem
        width: side.width - 20
        height: 28
        radius: 6
        readonly property bool on: index === app.pageIndex
        color: navItem.on ? Theme.accentSoft
                          : hover.containsMouse ? Theme.hairSoft : "transparent"

        Behavior on color {
          ColorAnimation { duration: 100 }
        }

        RowLayout {
          x: 8
          spacing: 8
          anchors.verticalCenter: parent.verticalCenter

          NavIcon {
            which: index
            // mac 侧栏:选中项图标与文字都是强调色,非选中为普通文字色
            color: navItem.on ? Theme.accent : Theme.ink
            holeColor: navItem.color
            Layout.alignment: Qt.AlignVCenter
          }

          Txt {
            text: modelData
            color: navItem.on ? Theme.accent : Theme.ink
            Layout.alignment: Qt.AlignVCenter
          }
        }

        MouseArea {
          id: hover
          anchors.fill: parent
          hoverEnabled: true
          cursorShape: Qt.PointingHandCursor
          onClicked: app.pageIndex = index
        }
      }
    }
  }
}
