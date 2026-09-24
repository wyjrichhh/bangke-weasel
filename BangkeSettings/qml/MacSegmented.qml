import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// mac 分段选择器(NSSegmentedControl):灰底轨道,选中段是浮起的白色圆角块。
// options 收 [{label, value}];宽度按内容,不拉伸。
Rectangle {
  id: seg
  property var options: []
  property var value
  signal edited(var value)

  implicitHeight: 24
  implicitWidth: track.implicitWidth + 4
  radius: 6
  color: Theme.dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)

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

      delegate: Rectangle {
        readonly property bool on: seg.pick(modelData) === seg.value
        readonly property var modelEntry: modelData
        Layout.preferredHeight: track.height
        Layout.preferredWidth: label.implicitWidth + 22
        radius: 4
        color: on ? (Theme.dark ? "#48484A" : Theme.surface) : "transparent"
        border.width: on && !Theme.dark ? 1 : 0
        border.color: Qt.rgba(0, 0, 0, 0.08)

        Txt {
          id: label
          anchors.centerIn: parent
          text: typeof modelEntry === "object" ? (modelEntry.label !== undefined ? modelEntry.label
                                                                                 : String(modelEntry.value))
                                               : String(modelEntry)
          color: Theme.ink
          font.pixelSize: Theme.subSize
        }

        MouseArea {
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          onClicked: seg.edited(seg.pick(modelData))
        }
      }
    }
  }
}
