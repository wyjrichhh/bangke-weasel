import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// mac 分组表单卡:小灰标题悬在卡片上方(SwiftUI Form .grouped 的 Section
// 头),卡片本体是纯色圆角面 + 两层极淡的同心叠影(mac 卡片在浅灰底上
// 若隐若现的浮起感),行间发丝线由行组件自己画。
// 叠影用纯 Rectangle 而不是 MultiEffect:shader 阴影在软件/RDP 渲染下
// blur 退化成实心轮廓块。
ColumnLayout {
  id: card
  property string title: ""
  property int hPadding: 18
  default property alias contentData: body.data

  spacing: 0

  Txt {
    text: card.title
    visible: card.title.length > 0
    // 分块标题用 50% 墨色(mac section header 的对比档),26% 在浅底上不可读
    color: Theme.ink2
    font.pixelSize: Theme.hintSize
    font.weight: Font.DemiBold
    Layout.fillWidth: true
    Layout.leftMargin: 10
    Layout.bottomMargin: 5
  }

  Item {
    Layout.fillWidth: true
    implicitHeight: body.implicitHeight

    Rectangle {
      anchors.fill: parent
      anchors.margins: -2
      anchors.bottomMargin: -3
      radius: Theme.cardRadius + 2
      color: Qt.rgba(0, 0, 0, Theme.dark ? 0.22 : 0.06)
    }
    Rectangle {
      anchors.fill: parent
      anchors.margins: -1
      anchors.bottomMargin: -2
      radius: Theme.cardRadius + 1
      color: Qt.rgba(0, 0, 0, Theme.dark ? 0.16 : 0.05)
    }
    Rectangle {
      anchors.fill: parent
      radius: Theme.cardRadius
      color: Theme.surface
    }

    ColumnLayout {
      id: body
      anchors.fill: parent
      anchors.leftMargin: card.hPadding
      anchors.rightMargin: card.hPadding
      spacing: 0
    }
  }
}
