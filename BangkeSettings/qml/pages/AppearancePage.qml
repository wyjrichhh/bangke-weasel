import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// Keep the editor scroll independent of the fixed preview.
RowLayout {
  id: page
  property bool advanced: false
  spacing: 28

  PageScroll {
    Layout.fillWidth: true
    Layout.fillHeight: true
    Layout.preferredWidth: 550

    Card {
      Layout.fillWidth: true
      title: "配色与主题"

      ComboRow {
        Layout.fillWidth: true
        showDivider: false
        label: "配色方案"

        options: app.style.schemes
        value: app.style.colorScheme
        onEdited: function (v) {
          app.style.colorScheme = v
        }
      }

    }

    Card {
      Layout.fillWidth: true
      title: "文字与形态"

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
        onEdited: function (v) { app.style.fontSize = Math.round(v) }
      }

      StyleRowGroup {
        Layout.fillWidth: true
        source: app.style.styleControls
        section: "布局"
        includeKeys: ["layout/corner_radius"]
      }

      ComboRow {
        Layout.fillWidth: true
        label: "候选排列"
        options: app.style.layoutOptions
        value: app.style.layout
        onEdited: function (v) { app.style.layout = v }
      }
    }

    Btn {
      text: page.advanced ? "收起更多外观选项" : "更多外观选项"
      quiet: true
      onClicked: page.advanced = !page.advanced
    }

    Card {
      Layout.fillWidth: true
      title: "颜色"
      visible: page.advanced
      StyleRowGroup {
        Layout.fillWidth: true
        source: app.style.styleControls
        section: "颜色"
      }
    }

    Card {
      Layout.fillWidth: true
      title: "布局细节"
      visible: page.advanced
      StyleRowGroup {
        Layout.fillWidth: true
        source: app.style.styleControls
        section: "布局"
        excludeKeys: ["layout/corner_radius"]
      }
    }

    Card {
      Layout.fillWidth: true
      title: "显示"
      visible: page.advanced

      StyleRowGroup {
        Layout.fillWidth: true
        source: app.style.styleControls
        section: "显示"
      }
    }

    Card {
      Layout.fillWidth: true
      title: "格式"
      visible: page.advanced

      StyleRowGroup {
        Layout.fillWidth: true
        source: app.style.styleControls
        section: "格式"
      }
    }
  }
  MacPreviewStrip {
    previewPageSize: app.general.pageSize
    showComments: app.general.commentHints
    Layout.fillWidth: true
    Layout.preferredWidth: 550
    Layout.alignment: Qt.AlignTop
  }

}
