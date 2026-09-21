import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import Bangke.Pages

// 卡片:surface + 1px 发丝边 + 13 圆角 + 真投影。
// 投影用 MultiEffect:QGraphicsDropShadowEffect 是 Widgets 的东西,而 QSS 根本没有
// box-shadow —— 这正是这次重写的理由之一。
Item {
  id: card
  property string title: ""
  property int hPadding: 20
  property int topPadding: 8
  property int bottomPadding: 6
  default property alias contentData: body.data

  implicitHeight: body.implicitHeight + card.topPadding + card.bottomPadding

  Rectangle {
    id: face
    anchors.fill: parent
    radius: Theme.cardRadius
    color: Theme.surface
    border.width: 1
    border.color: Theme.hair
    visible: false
  }

  MultiEffect {
    source: face
    anchors.fill: face
    shadowEnabled: true
    shadowColor: Theme.shadow
    shadowBlur: 0.8
    shadowVerticalOffset: 6
  }

  ColumnLayout {
    id: body
    anchors.fill: parent
    anchors.leftMargin: card.hPadding
    anchors.rightMargin: card.hPadding
    anchors.topMargin: card.topPadding
    anchors.bottomMargin: card.bottomPadding
    spacing: 0

    Txt {
      text: card.title
      visible: card.title.length > 0
      color: Theme.ink3
      font.pixelSize: Theme.hintSize
      font.weight: Font.DemiBold
      font.letterSpacing: 0.7
      Layout.fillWidth: true
      Layout.topMargin: 12
      Layout.bottomMargin: 4
    }
  }
}
