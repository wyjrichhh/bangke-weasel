import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 通用:行清单与文案照搬 GeneralPage.cpp(输入习惯 / 提示 两张卡)。
PageScroll {

  Card {
    Layout.fillWidth: true
    title: "输入习惯"

    SpinRow {
      Layout.fillWidth: true
      showDivider: false
      label: "每页候选数"
      min: 1
      max: 20
      value: app.general.pageSize
      onEdited: function (v) {
        app.general.pageSize = v
      }
    }

    ComboRow {
      Layout.fillWidth: true
      label: "左 Shift 切换行为"
      fieldWidth: 150
      options: app.general.shiftActions
      value: app.general.shiftL
      onEdited: function (v) {
        app.general.shiftL = v
      }
    }

    ComboRow {
      Layout.fillWidth: true
      label: "右 Shift 切换行为"
      fieldWidth: 150
      options: app.general.shiftActions
      value: app.general.shiftR
      onEdited: function (v) {
        app.general.shiftR = v
      }
    }

    ToggleRow {
      Layout.fillWidth: true
      label: "候选词后显示拼音注释"
      helper: "关闭时仅保留 AI 标记"
      value: app.general.commentHints
      onEdited: function (v) {
        app.general.commentHints = v
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "提示"

    ToggleRow {
      Layout.fillWidth: true
      showDivider: false
      label: "切换时弹出提示"
      helper: "切换方案、开关中英等"
      value: app.general.notifications
      onEdited: function (v) {
        app.general.notifications = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "提示停留时间"
      min: 0
      max: 5000
      suffix: " ms"
      value: app.general.notificationsTime
      onEdited: function (v) {
        app.general.notificationsTime = v
      }
    }
  }
}
