import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 外观页(mac AppearanceSettingsPage):顶部浅/深双面板预览条 + 颜色/字体/
// 布局/显示/格式五段。配色方案、字号、候选排列是 StyleModel 直属属性,
// 其余行来自 styleControls 的同名段。
PageScroll {

  MacPreviewStrip {
    Layout.fillWidth: true
  }

  Card {
    Layout.fillWidth: true
    title: "颜色"

    ComboRow {
      Layout.fillWidth: true
      showDivider: false
      label: "配色方案"
      fieldWidth: 190
      options: app.style.schemes
      value: app.style.colorScheme
      onEdited: function (v) {
        app.style.colorScheme = v
      }
    }

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "颜色"
    }
  }

  Card {
    Layout.fillWidth: true
    title: "字体"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "字体"
    }

    SliderRow {
      Layout.fillWidth: true
      label: "字号"
      min: 8
      max: 48
      value: app.style.fontSize
      onEdited: function (v) {
        app.style.fontSize = Math.round(v)
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "布局"

    ComboRow {
      Layout.fillWidth: true
      showDivider: false
      label: "候选排列"
      fieldWidth: 120
      options: app.style.layoutOptions
      value: app.style.layout
      onEdited: function (v) {
        app.style.layout = v
      }
    }

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "布局"
    }
  }

  Card {
    Layout.fillWidth: true
    title: "显示"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "显示"
    }
  }

  Card {
    Layout.fillWidth: true
    title: "格式"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "格式"
    }
  }
}
