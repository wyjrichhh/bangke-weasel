import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 行为页(mac BehaviorSettingsPage):mac 的「菜单栏」段在 Windows 映射为
// 托盘与通知(Windows 没有菜单栏,托盘图标是等价入口);「输入」段照搬;
// 「拼音注释」是 Windows 特有保留(mac 无此开关,注释恒随方案)。
// mac 的「键盘布局」与「组合键时长」是引擎不认的 mac 专有键,不设。
PageScroll {

  Card {
    Layout.fillWidth: true
    title: "菜单栏"

    ToggleRow {
      Layout.fillWidth: true
      showDivider: false
      label: "显示托盘图标"
      value: app.general.trayIcon
      onEdited: function (v) {
        app.general.trayIcon = v
      }
    }

    ComboRow {
      Layout.fillWidth: true
      label: "通知"
      fieldWidth: 130
      options: app.general.notificationOptions
      value: app.general.notificationsMode
      onEdited: function (v) {
        app.general.notificationsMode = v
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "输入"

    SliderRow {
      Layout.fillWidth: true
      showDivider: false
      label: "每页候选数"
      min: 1
      max: 20
      value: app.general.pageSize
      onEdited: function (v) {
        app.general.pageSize = Math.round(v)
      }
    }

    ComboRow {
      Layout.fillWidth: true
      label: "左 Shift"
      fieldWidth: 130
      options: app.general.shiftActions
      value: app.general.shiftL
      onEdited: function (v) {
        app.general.shiftL = v
      }
    }

    ComboRow {
      Layout.fillWidth: true
      label: "右 Shift"
      fieldWidth: 130
      options: app.general.shiftActions
      value: app.general.shiftR
      onEdited: function (v) {
        app.general.shiftR = v
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "拼音注释"

    ToggleRow {
      Layout.fillWidth: true
      showDivider: false
      label: "候选词后显示拼音注释"
      helper: "关闭时仅保留 AI 标记"
      value: app.general.commentHints
      onEdited: function (v) {
        app.general.commentHints = v
      }
    }
  }
}
