import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

Item {
  id: tabs
  property var options: []
  property var value
  signal edited(var value)
  implicitHeight: 40
  implicitWidth: track.implicitWidth

  Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.hair }
  RowLayout {
    id: track
    height: parent.height
    spacing: 24
    Repeater {
      model: tabs.options
      delegate: AbstractButton {
        id: tab
        required property var modelData
        readonly property var entry: typeof modelData === "object" ? modelData.value : modelData
        readonly property bool selected: tabs.value === entry
        Layout.preferredWidth: Math.max(40, label.implicitWidth + 4)
        Layout.fillHeight: true
        onClicked: tabs.edited(entry)
        contentItem: Txt {
          id: label
          text: typeof tab.modelData === "object" ? tab.modelData.label : tab.modelData
          color: tab.selected ? Theme.accent : Theme.ink2
          font.weight: tab.selected ? Font.DemiBold : Font.Normal
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
        }
        background: Item {
          Rectangle { visible: tab.selected; anchors.bottom: parent.bottom; width: parent.width; height: 2; color: Theme.accent }
          Rectangle { visible: tab.activeFocus; anchors.fill: parent; color: "transparent"; border.width: 1; border.color: Theme.accent; radius: 3 }
        }
      }
    }
  }
}
