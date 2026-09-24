import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

// 窗口用 Windows 原生标题栏(系统按钮与圆角);内容区保持 mac 布局:
// 左侧 200px 侧栏 + 底部「恢复默认 / 取消 / 保存并应用」。取消 = 关窗
// 丢弃草稿(模型只有显式保存才落盘);恢复默认只改草稿,仍要点保存。
Window {
  id: window

  width: 740
  height: 680
  minimumWidth: 700
  minimumHeight: 620
  visible: true
  title: "蚌壳拼音设置"
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
      Layout.preferredWidth: 200
      Layout.fillHeight: true
    }

    ColumnLayout {
      Layout.fillWidth: true
      Layout.fillHeight: true
      spacing: 0

      PageHeader {
        Layout.fillWidth: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.topMargin: 12
        title: app.pageTitles[app.pageIndex]
        sub: app.pageSubs[app.pageIndex]
      }

      StackLayout {
        currentIndex: app.pageIndex
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.leftMargin: 24
        Layout.rightMargin: 24
        Layout.topMargin: 14

        AppearancePage {}
        BehaviorPage {}
        AiPage {}
        SchemasPage {}
        AboutPage {}
      }

      Hairline { Layout.fillWidth: true }

      RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 20
        Layout.rightMargin: 20
        Layout.topMargin: 12
        Layout.bottomMargin: 14
        spacing: 10

        Btn {
          text: "恢复默认"
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
