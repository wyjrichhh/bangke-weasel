import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

// 原生窗口框架；取消丢弃草稿，恢复默认仍需显式保存。
Window {
  id: window

  width: 960
  height: 660
  minimumWidth: 780
  minimumHeight: 580
  visible: true
  title: "蚌壳拼音设置"
  color: Theme.ground
  onClosing: function (event) { if (app.saving) event.accepted = false }

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
      enabled: !app.saving
      Layout.preferredWidth: 194
      Layout.fillHeight: true
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 0

      PageHeader {
        Layout.fillWidth: true
        Layout.leftMargin: 30
        Layout.rightMargin: 30
        Layout.topMargin: 28
        title: app.pageTitles[app.pageIndex]

      }

      StackLayout {
        enabled: !app.saving
        currentIndex: app.pageIndex
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 30
        Layout.rightMargin: 30
        Layout.topMargin: 22

        AppearancePage {}
        BehaviorPage {}
        AiPage {}
        SchemasPage {}
        AboutPage {}
      }

      Hairline { Layout.fillWidth: true }

      RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 30
        Layout.rightMargin: 30
        Layout.topMargin: 12
        Layout.bottomMargin: 14
        spacing: 10

        Btn {
          text: "恢复默认"
          quiet: true
          enabled: !app.saving
          onClicked: app.restoreDefaults()
        }

        Item { Layout.fillWidth: true }

        Txt {
          text: "正在部署…"
          color: Theme.ink2
          visible: app.saving
          Layout.alignment: Qt.AlignVCenter
        }

        Btn {
          text: "取消"
          quiet: true
          enabled: !app.saving
          onClicked: window.close()
        }

        Btn {
          text: "保存并应用"
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
    function onSaved() { window.close() }
    function onToast(text) { toast.show(text) }
  }

  Connections {
    target: app
    function onInfo(title, body) { confirm.open(title, body) }
  }

  // Esc = 取消(mac cancelAction)
  Shortcut {
    sequence: "Esc"
    onActivated: window.close()
  }
}
