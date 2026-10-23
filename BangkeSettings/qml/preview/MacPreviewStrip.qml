import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

ColumnLayout {
  id: strip
  spacing: 12
  property bool longSample: false

  Txt { text: "实时预览"; color: Theme.ink2 }
  Rectangle {
    Layout.fillWidth: true
    Layout.preferredHeight: 360
    radius: Theme.cardRadius
    color: Theme.surface
    border.color: Theme.hair
    ColumnLayout {
      anchors.fill: parent
      anchors.margins: 20
      spacing: 16
      RowLayout {
        Layout.fillWidth: true
        Txt { text: "文稿 · 输入场景"; color: Theme.ink2 }
        Item { Layout.fillWidth: true }
        Txt { text: "跟随系统深浅主题"; color: Theme.ink2 }
      }
      Txt { text: "今天，bangke"; font.pixelSize: 18 }
      Flickable {
        id: viewport
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: panel.width
        contentHeight: panel.height
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        MiniPanel {
          id: panel
          colors: Theme.dark ? app.style.previewDark : app.style.previewLight
          m: app.style.previewMetrics
          longSample: strip.longSample
          zoom: 1
        }
      }
      RowLayout {
        Layout.fillWidth: true
        Btn { text: "普通候选"; Layout.fillWidth: true; primary: !strip.longSample; onClicked: strip.longSample = false }
        Btn { text: "长句候选"; Layout.fillWidth: true; primary: strip.longSample; onClicked: strip.longSample = true }
      }
      Txt { text: "原始字号 · 超出区域可滚动查看"; color: Theme.ink2; font.pixelSize: Theme.hintSize }
    }
  }
  Txt { text: "调整左侧设置，即可查看候选窗的变化。"; color: Theme.ink2 }
}
