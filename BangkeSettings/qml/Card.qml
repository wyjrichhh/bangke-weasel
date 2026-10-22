import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 分组标题与内容保留独立层次；使用单层细边框，避免叠影污染背景色。
ColumnLayout {
  id: card
  property string title: ""
  property int hPadding: 18
  default property alias contentData: body.data
  spacing: 0

  Txt {
    text: card.title
    visible: card.title.length > 0
    color: Theme.ink
    font.pixelSize: Theme.bodySize
    font.weight: Font.DemiBold
    Layout.fillWidth: true
    Layout.leftMargin: 2
    Layout.bottomMargin: 10
  }

  Rectangle {
    Layout.fillWidth: true
    implicitHeight: body.implicitHeight + 12
    radius: Theme.cardRadius
    color: Theme.surface
    border.width: 1
    border.color: Theme.hair

    ColumnLayout {
      id: body
      anchors.fill: parent
      anchors.leftMargin: card.hPadding
      anchors.rightMargin: card.hPadding
      anchors.topMargin: 6
      anchors.bottomMargin: 6
      spacing: 0
    }
  }
}
