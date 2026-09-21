import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Bangke.Pages

// 界面样式:外观 / 字体 / 深色模式 / 显示 / 鼠标与渲染 / 竖排文字 / 格式 / 预览 /
// 布局微调 / 自定义配色 —— 卡片划分与行文案照搬 StylePage.cpp。
// 中间七张卡的每一行都来自 styleControls,卡片边界 = 模型里的 section。
PageScroll {

  function customColor(key, fallback) {
    var map = app.style.customColors
    return Theme.argb(map ? map[key] : undefined, fallback)
  }

  Card {
    Layout.fillWidth: true
    title: "外观"

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

    SpinRow {
      Layout.fillWidth: true
      label: "字体大小"
      min: 9
      max: 36
      value: app.style.fontSize
      onEdited: function (v) {
        app.style.fontSize = v
      }
    }

    ComboRow {
      Layout.fillWidth: true
      label: "候选窗排列"
      fieldWidth: 110
      options: app.style.layoutOptions
      value: app.style.layout
      onEdited: function (v) {
        app.style.layout = v
      }
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
  }

  Card {
    Layout.fillWidth: true
    title: "深色模式"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "深色模式"
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
    title: "鼠标与渲染"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "鼠标与渲染"
    }
  }

  Card {
    Layout.fillWidth: true
    title: "竖排文字"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "竖排文字"
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

  Card {
    Layout.fillWidth: true
    title: "预览"

    CandidatePreview {
      Layout.fillWidth: true
      Layout.topMargin: 12
      Layout.bottomMargin: 8
    }
  }

  Card {
    Layout.fillWidth: true
    title: "布局微调"

    StyleRowGroup {
      Layout.fillWidth: true
      source: app.style.styleControls
      section: "布局微调"
    }
  }

  Card {
    Layout.fillWidth: true
    title: "自定义配色"

    RowShell {
      Layout.fillWidth: true
      showDivider: false
      label: "背景颜色"
      controlWidth: 24

      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 6
        color: customColor("back_color", "#FFFFFFFF")
        border.width: 1
        border.color: Theme.hair

        MouseArea {
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          onClicked: picker.openFor("back_color")
        }
      }
    }

    RowShell {
      Layout.fillWidth: true
      label: "高亮颜色"
      controlWidth: 24

      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 6
        color: customColor("hilited_candidate_back_color", "#FFA8C7F0")
        border.width: 1
        border.color: Theme.hair

        MouseArea {
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          onClicked: picker.openFor("hilited_candidate_back_color")
        }
      }
    }

    RowShell {
      Layout.fillWidth: true
      label: "文字颜色"
      controlWidth: 24

      Rectangle {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 6
        color: customColor("text_color", "#FF1F2937")
        border.width: 1
        border.color: Theme.hair

        MouseArea {
          anchors.fill: parent
          cursorShape: Qt.PointingHandCursor
          onClicked: picker.openFor("text_color")
        }
      }
    }

    RowShell {
      Layout.fillWidth: true
      label: "背景不透明度"
      controlWidth: opacitySlider.implicitWidth

      Slider {
        id: opacitySlider
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: 180
        from: 55
        to: 100
        stepSize: 1
        value: app.style.customOpacity
        onMoved: app.style.setCustomOpacity(value)

        background: Rectangle {
          x: opacitySlider.leftPadding
          y: opacitySlider.topPadding + opacitySlider.availableHeight / 2 - height / 2
          width: opacitySlider.availableWidth
          height: 4
          radius: 2
          color: Theme.hair

          Rectangle {
            width: opacitySlider.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
          }
        }

        handle: Rectangle {
          x: opacitySlider.leftPadding + opacitySlider.visualPosition * (opacitySlider.availableWidth - width)
          y: opacitySlider.topPadding + opacitySlider.availableHeight / 2 - height / 2
          width: 16
          height: 16
          radius: 8
          color: Theme.surface
          border.width: 1
          border.color: opacitySlider.pressed ? Theme.accent : Theme.hair
        }
      }
    }

    Txt {
      text: "调整任一颜色或透明度后,将以「自定义」配色生效。"
      color: Theme.ink3
      font.pixelSize: Theme.hintSize
      Layout.fillWidth: true
      Layout.topMargin: 6
      Layout.bottomMargin: 4
    }
  }

  ColorDialog {
    id: picker

    property string colorKey: ""

    function openFor(key) {
      picker.colorKey = key
      picker.open()
    }

    title: "选择颜色"
    options: ColorDialog.ShowAlphaChannel
    onAccepted: app.style.setCustomColor(picker.colorKey, Theme.toArgb(selectedColor))
  }
}
