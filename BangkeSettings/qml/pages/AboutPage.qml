import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

PageScroll {
  id: page

  Card {
    Layout.fillWidth: true
    RowLayout {
      Layout.fillWidth: true
      Layout.topMargin: 12
      Layout.bottomMargin: 12
      spacing: 18
      Image {
        Layout.preferredWidth: 64
        Layout.preferredHeight: 64
        source: "qrc:/bangke-appicon.png"
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
      }
      ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        Txt { text: "蚌壳拼音"; font.pixelSize: 20; font.weight: Font.DemiBold }
        Txt { text: "版本 " + app.appVersion; color: Theme.ink2 }
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "更新"
    ToggleRow {
      Layout.fillWidth: true
      showDivider: false
      label: "每小时自动检查"
      value: app.updates.automatic
      onEdited: function (value) { app.updates.automatic = value }
    }
    RowShell {
      Layout.fillWidth: true
      label: "手动检查"
      controlWidth: updateButton.width
      Btn {
        id: updateButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: app.updates.checking ? "正在检查…" : "检查更新"
        enabled: !app.updates.checking
        onClicked: app.updates.check()
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "文件"
    RowLayout {
      Layout.fillWidth: true
      Layout.topMargin: 8
      Layout.bottomMargin: 8
      spacing: 12
      Btn { Layout.fillWidth: true; text: "打开用户文件夹"; onClicked: app.openUserFolder() }
      Btn { Layout.fillWidth: true; text: "打开日志文件夹"; onClicked: app.openLogFolder() }
    }
  }

  Dialog {
    id: updateDialog
    property string releaseVersion: ""
    property url downloadUrl
    anchors.centerIn: Overlay.overlay
    modal: true
    title: "蚌壳拼音有新版本"
    width: Math.min(480, page.width - 24)
    contentItem: ColumnLayout {
      Txt { id: releaseText; Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText }
      RowLayout {
        Btn { text: "前往下载"; onClicked: { Qt.openUrlExternally(updateDialog.downloadUrl); updateDialog.close() } }
        Btn { text: "跳过此版本"; onClicked: { app.updates.skipVersion(updateDialog.releaseVersion); updateDialog.close() } }
        Btn { text: "稍后提醒"; onClicked: updateDialog.close() }
      }
    }
  }
  Connections {
    target: app.updates
    function onAvailable(version, url, notes, manual) {
      updateDialog.releaseVersion = version
      updateDialog.downloadUrl = url
      releaseText.text = "版本 " + version + " 已发布。\n\n" + notes + "\n\n下载后使用系统安装器更新。"
      updateDialog.open()
    }
  }
}
