import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 侧栏:184 固定宽,条目 36 高、选中态是 accent-soft 药丸。
// 条目与页头文案同一个来源(app.pageTitles),分开写迟早对不上。
Rectangle {
  id: side
  implicitWidth: 184
  color: Theme.surface

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
    anchors.leftMargin: 12
    anchors.rightMargin: 12
    anchors.topMargin: 16
    spacing: 3

    Repeater {
      model: app.pageTitles

      delegate: Rectangle {
        id: navItem
        width: side.width - 24
        height: 36
        radius: Theme.fieldRadius
        readonly property bool on: index === app.pageIndex
        color: navItem.on ? Theme.accentSoft
                          : hover.containsMouse ? Theme.hairSoft : "transparent"

        Behavior on color {
          ColorAnimation { duration: 100 }
        }

        RowLayout {
          x: 12
          spacing: 10
          anchors.verticalCenter: parent.verticalCenter

          NavIcon {
            which: index
            color: navItem.on ? Theme.accentInk : Theme.ink2
            holeColor: navItem.color
            Layout.alignment: Qt.AlignVCenter
          }

          Txt {
            text: modelData
            color: navItem.on ? Theme.accentInk : Theme.ink2
            font.weight: navItem.on ? Font.DemiBold : Font.Normal
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
