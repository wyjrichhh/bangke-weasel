import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 行为页(mac BehaviorSettingsPage):mac 的「菜单栏」段在 Windows 只留通知 ——
// 语言栏(托盘)那一项是打开设置页的入口,不能做成开关(见 LanguageBar.cpp);
// 「输入」段照搬;
// 「拼音注释」是 Windows 特有保留(mac 无此开关,注释恒随方案)。
// mac 的「键盘布局」与「组合键时长」是引擎不认的 mac 专有键,不设。
RowLayout {
  spacing: 28
  PageScroll {
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredWidth: 550

    Card {
      Layout.fillWidth: true
      title: "通知"

      ComboRow {
        Layout.fillWidth: true
        showDivider: false
        label: "显示通知"

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

        options: app.general.shiftActions
        value: app.general.shiftL
        onEdited: function (v) {
          app.general.shiftL = v
        }
      }

      ComboRow {
        Layout.fillWidth: true
        label: "右 Shift"

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
  MacPreviewStrip {
    Layout.fillWidth: true
    Layout.preferredWidth: 550
    Layout.alignment: Qt.AlignTop
    previewPageSize: app.general.pageSize
    showComments: app.general.commentHints
  }
}
