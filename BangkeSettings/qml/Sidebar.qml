import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

Rectangle {
  id: side
  implicitWidth: 194
  color: Theme.sidebarBg

  Rectangle {
    width: 1
    height: parent.height
    anchors.right: parent.right
    color: Theme.hairSoft
  }

  Row {
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.leftMargin: 24
    anchors.topMargin: 28
    spacing: 10
    Image { width: 30; height: 30; source: "qrc:/bangke-appicon.png"; smooth: true; mipmap: true }
    Txt {
      text: "蚌壳拼音"
      color: Theme.ink
      font.pixelSize: 17
      font.weight: Font.DemiBold
      anchors.verticalCenter: parent.verticalCenter
    }
  }

  Column {
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.leftMargin: 14
    anchors.rightMargin: 14
    anchors.topMargin: 100
    spacing: 8
    Repeater {
      model: app.pageTitles
      delegate: AbstractButton {
        id: navItem
        required property int index
        required property string modelData
        width: side.width - 28
        height: 44
        readonly property bool selected: index === app.pageIndex
        onClicked: app.pageIndex = index
        Accessible.name: modelData
        background: Rectangle {
          radius: 7
          color: navItem.selected ? Theme.accentSoft : navItem.hovered ? Theme.hairSoft : "transparent"
          border.width: navItem.activeFocus ? 1 : 0
          border.color: Theme.accent
          Rectangle {
            visible: navItem.selected
            width: 3; height: 18; radius: 1.5
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.accent
          }
        }
        contentItem: RowLayout {
          spacing: 12
          NavIcon {
            which: navItem.index
            color: navItem.selected ? Theme.accent : Theme.ink2
            holeColor: navItem.selected ? Theme.accentSoft : Theme.sidebarBg
            Layout.leftMargin: 14
            Layout.alignment: Qt.AlignVCenter
          }
          Txt {
            text: navItem.modelData
            color: navItem.selected ? Theme.accent : Theme.ink2
            font.weight: navItem.selected ? Font.DemiBold : Font.Normal
            Layout.fillWidth: true
          }
        }
      }
    }
  }
}
