import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 部署互斥一类的提示从 Configurator 的 QMessageBox 换成这个:自带遮罩,
// 于是 Widgets 可以从设置程序里彻底摘掉。
Item {
  id: dialog
  property string title: ""
  property string body: ""

  function open(title, body) {
    dialog.title = title
    dialog.body = body
    dialog.visible = true
  }

  anchors.fill: parent
  visible: false
  z: 100

  Rectangle {
    anchors.fill: parent
    color: Theme.scrim
    // 遮罩吃掉落在外面的点击,免得点到底下的卡片
    MouseArea { anchors.fill: parent }
  }

  Rectangle {
    id: box
    width: Math.min(420, dialog.width - 80)
    height: column.implicitHeight + 40
    anchors.centerIn: parent
    radius: Theme.cardRadius
    color: Theme.surface
    border.width: 1
    border.color: Theme.hair

    ColumnLayout {
      id: column
      anchors.fill: parent
      anchors.margins: 20
      spacing: 12

      Txt {
        text: dialog.title
        font.pixelSize: Theme.sectionSize
        font.weight: Font.DemiBold
        Layout.fillWidth: true
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }

      Txt {
        text: dialog.body
        color: Theme.ink2
        Layout.fillWidth: true
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
      }

      RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 4

        Item { Layout.fillWidth: true }

        Btn {
          text: "确定"
          primary: true
          onClicked: dialog.visible = false
        }
      }
    }
  }
}
