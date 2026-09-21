import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

// 页面的滚动壳:内容不出视口时不滚,卡片留顶部。滚动条压细到 6px,
// 因为 Basic style 那根默认滚动条跟这套尺度不搭。
Item {
  id: page
  default property alias contentData: column.data

  ScrollView {
    id: scroll
    anchors.fill: parent
    clip: true

    ScrollBar.vertical: ScrollBar {
      policy: ScrollBar.AsNeeded
      width: 8
      background: Item {}
      contentItem: Rectangle {
        implicitWidth: 6
        radius: 3
        color: Theme.ink3
        opacity: 0.35
      }
    }

    ColumnLayout {
      id: column
      width: scroll.availableWidth
      spacing: 12
    }
  }
}
