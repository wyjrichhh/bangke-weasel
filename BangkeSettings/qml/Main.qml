import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

// 原生窗口框架；取消丢弃草稿，恢复默认仍需显式保存。
Window {
  id: window

  width: Math.min(1240, Screen.desktopAvailableWidth - 48)
  height: Math.min(840, Screen.desktopAvailableHeight - 48)
  minimumWidth: Math.min(1100, Screen.desktopAvailableWidth - 48)
  minimumHeight: Math.min(720, Screen.desktopAvailableHeight - 48)
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

  ColumnLayout {
    anchors.fill: parent
    spacing: 0

    Item {
      Layout.fillWidth: true
      Layout.preferredHeight: 72
      Txt {
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.verticalCenter: parent.verticalCenter
        text: "蚌壳拼音"
        font.pixelSize: 18
        font.weight: Font.DemiBold
      }
      Row {
        anchors.centerIn: parent
        spacing: 8
        Repeater {
          model: app.pageTitles
          delegate: Btn {
            required property int index
            required property string modelData
            text: modelData
            quiet: true
            implicitHeight: 42
            background: Rectangle {
              radius: 8
              color: index === app.pageIndex ? Theme.accentSoft : "transparent"
              border.width: parent.activeFocus ? 1 : 0
              border.color: Theme.accent
            }
            contentItem: Txt {
              text: modelData
              horizontalAlignment: Text.AlignHCenter
              verticalAlignment: Text.AlignVCenter
              color: index === app.pageIndex ? Theme.accent : Theme.ink2
              font.weight: Font.DemiBold
            }
            enabled: !app.saving
            onClicked: app.pageIndex = index
          }
        }
      }
    }
    Hairline { Layout.fillWidth: true }

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

        Item { Layout.fillWidth: true }

        Btn {
          text: "恢复默认"
          quiet: true
          enabled: !app.saving
          onClicked: app.restoreDefaults()
        }

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
