import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

Window {
  id: window

  width: 760
  height: 560
  minimumWidth: 720
  minimumHeight: 520
  visible: true
  title: "蚌壳拼音 · 设置"
  color: Theme.ground

  // 系统深浅:QML 单例拿不到注入的上下文属性,由这里把值喂进去
  Binding {
    target: Theme
    property: "dark"
    value: sysTheme.dark
  }

  RowLayout {
    anchors.fill: parent
    spacing: 0

    Sidebar {
      Layout.preferredWidth: 184
      Layout.fillHeight: true
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 0

      PageHeader {
        Layout.fillWidth: true
        Layout.leftMargin: 26
        Layout.rightMargin: 26
        Layout.topMargin: 24
        title: app.pageTitles[app.pageIndex]
        sub: app.pageSubs[app.pageIndex]
      }

      StackLayout {
        currentIndex: app.pageIndex
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 26
        Layout.rightMargin: 26
        Layout.topMargin: 16

        GeneralPage {}
        StylePage {}
        AiPage {}
        AdvancedPage {}
      }

      Hairline { Layout.fillWidth: true }

      RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 26
        Layout.rightMargin: 26
        Layout.topMargin: 12
        Layout.bottomMargin: 14
        spacing: 10

        Item { Layout.fillWidth: true }

        Txt {
          text: "正在部署…"
          color: Theme.ink2
          visible: app.saving
          Layout.alignment: Qt.AlignVCenter
        }

        Btn {
          text: "保存并重新部署"
          primary: true
          enabled: !app.saving
          onClicked: app.saveAndDeploy()
        }
      }
    }
  }

  Toast {
    id: toast
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 76
  }

  ConfirmDialog { id: confirm }

  Connections {
    target: app
    function onToast(text) { toast.show(text) }
  }

  Connections {
    target: app
    function onInfo(title, body) { confirm.open(title, body) }
  }
}
