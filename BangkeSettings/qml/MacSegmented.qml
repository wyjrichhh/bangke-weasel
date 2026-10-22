import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

// 紧凑分段导航，共享焦点、键盘与选中态。
Rectangle {
  id: seg
  property var options: []
  property var value
  signal edited(var value)

  implicitHeight: Theme.controlHeight + 4
  implicitWidth: track.implicitWidth + 4
  radius: Theme.fieldRadius
  color: Theme.hairSoft

  function pick(o) {
    return o.value !== undefined ? o.value : o
  }

  RowLayout {
    id: track
    x: 2
    y: 2
    height: parent.height - 4
    spacing: 0

    Repeater {
      model: seg.options

      delegate: AbstractButton {
        id: segment
        readonly property bool on: seg.pick(modelData) === seg.value
        readonly property var modelEntry: modelData
        Layout.preferredHeight: track.height
        Layout.preferredWidth: label.implicitWidth + 22
        onClicked: seg.edited(seg.pick(modelData))
        background: Rectangle {
          radius: 6
          color: segment.on ? Theme.surface : segment.hovered ? Theme.hairSoft : "transparent"
          border.width: segment.activeFocus ? 1 : 0
          border.color: Theme.accent
        }

        contentItem: Txt {
          id: label
          horizontalAlignment: Text.AlignHCenter
          verticalAlignment: Text.AlignVCenter
          text: typeof modelEntry === "object" ? (modelEntry.label !== undefined ? modelEntry.label
                                                                                 : String(modelEntry.value))
                                               : String(modelEntry)
          color: segment.on ? Theme.accent : Theme.ink2
          font.pixelSize: Theme.subSize
        }


      }
    }
  }
}
