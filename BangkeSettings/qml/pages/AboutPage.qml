import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 关于页(mac AboutSettingsPage):顶部 48 起、图标/名称/版本整体水平居中,
// 下部留白,两个文件夹入口收在底部。图标用 1024px 源图(ico 最大帧 48px,
// 拉到 128 会糊)。
ColumnLayout {
  spacing: 10

  Item { Layout.preferredHeight: 48 }

  Image {
    source: "qrc:/bangke-appicon.png"
    Layout.preferredWidth: 128
    Layout.preferredHeight: 128
    Layout.alignment: Qt.AlignHCenter
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
  }

  Txt {
    Layout.topMargin: 8
    text: "蚌壳拼音"
    font.pixelSize: Theme.titleSize
    font.weight: Font.DemiBold
    Layout.alignment: Qt.AlignHCenter
  }

  Txt {
    text: "版本 " + app.appVersion
    color: Theme.ink2
    Layout.alignment: Qt.AlignHCenter
  }

  Item { Layout.fillHeight: true }

  RowLayout {
    Layout.alignment: Qt.AlignHCenter
    Layout.bottomMargin: 24
    spacing: 24

    Txt {
      text: "打开用户文件夹"
      color: Theme.accent
      MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: app.openUserFolder()
      }
    }

    Txt {
      text: "打开日志文件夹"
      color: Theme.accent
      MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: app.openLogFolder()
      }
    }
  }
}
