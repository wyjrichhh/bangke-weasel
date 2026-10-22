import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 紧凑实时预览固定在上方，下方按主题分组滚动编辑。
ColumnLayout {
  id: page
  property string section: "颜色"
  spacing: 14

  MacPreviewStrip {
    Layout.fillWidth: true
  }

  SectionTabs {
    Layout.fillWidth: true
    options: ["颜色", "字体", "布局", "显示", "格式"]
    value: page.section
    onEdited: function (value) { page.section = value }
  }

  PageScroll {
    Layout.fillWidth: true
    Layout.fillHeight: true

  Card {
    Layout.fillWidth: true
    visible: page.section === "颜色"

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
    visible: page.section === "字体"

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
    visible: page.section === "布局"

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
    visible: page.section === "显示"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "显示"
    }
  }

  Card {
    Layout.fillWidth: true
    visible: page.section === "格式"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "格式"
    }
  }
  }
}
