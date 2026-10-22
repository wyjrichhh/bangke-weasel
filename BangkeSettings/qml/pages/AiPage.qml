import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 按方案、候选行为与模型分组，所有编辑保留在草稿中。
ColumnLayout {
  id: page
  property string section: "候选"
  spacing: 16

  readonly property var schemaOptions: {
    var out = []
    var ids = app.ai.schemas
    var names = app.ai.schemaNames
    for (var i = 0; i < ids.length; ++i)
      out.push({ label: i < names.length ? names[i] : ids[i], value: ids[i] })
    return out
  }

  function rangeMin(key, fallback) {
    var r = app.ai.fieldRanges ? app.ai.fieldRanges[key] : null
    return r && r.min !== undefined ? r.min : fallback
  }

  function rangeMax(key, fallback) {
    var r = app.ai.fieldRanges ? app.ai.fieldRanges[key] : null
    return r && r.max !== undefined ? r.max : fallback
  }

  Card {
    Layout.fillWidth: true
    ComboRow {
      Layout.fillWidth: true
      showDivider: false
      label: "输入方案"
      fieldWidth: 220
      options: schemaOptions
      value: app.ai.currentSchema
      onEdited: function (value) { app.ai.currentSchema = value }
    }
  }

  SectionTabs {
    Layout.fillWidth: true
    options: ["候选", "响应", "生成", "模型"]
    value: page.section
    onEdited: function (value) { page.section = value }
  }

  PageScroll {
    Layout.fillWidth: true
    Layout.fillHeight: true

  Card {
    Layout.fillWidth: true
    visible: page.section === "候选"
    ToggleRow {
      Layout.fillWidth: true
      showDivider: false
      label: "启用 AI 联想"
      value: app.ai.enabled
      onEdited: function (v) {
        app.ai.enabled = v
      }
    }

    ToggleRow {
      Layout.fillWidth: true
      label: "过滤标点符号"
      helper: "关掉后候选保留模型吐出的句读"
      value: app.ai.filterPunctuation
      onEdited: function (v) {
        app.ai.filterPunctuation = v
      }
    }

    ToggleRow {
      Layout.fillWidth: true
      label: "仅使用 AI 候选"
      helper: "候选只来自模型:一条、不带序号"
      value: app.ai.aiOnly
      onEdited: function (v) {
        app.ai.aiOnly = v
      }
    }

    SliderRow {
      Layout.fillWidth: true
      label: "AI 候选位置"
      helper: "从 0 开始计数，0 为首位；去重扫描范围不限制候选位置"
      min: rangeMin("targetIndex", 0)
      max: rangeMax("targetIndex", 20)
      value: app.ai.targetIndex
      onEdited: function (v) {
        app.ai.targetIndex = Math.round(v)
      }
    }

  }

  Card {
    visible: page.section === "响应"
    Layout.fillWidth: true

    SliderRow {
      Layout.fillWidth: true
      showDivider: false
      label: "最小输入长度"
      min: rangeMin("minInputLength", 1)
      max: rangeMax("minInputLength", 40)
      value: app.ai.minInputLength
      onEdited: function (v) {
        app.ai.minInputLength = Math.round(v)
      }
    }

    SliderRow {
      Layout.fillWidth: true
      label: "最少汉字数"
      min: rangeMin("minHanzi", 1)
      max: rangeMax("minHanzi", 10)
      value: app.ai.minHanzi
      onEdited: function (v) {
        app.ai.minHanzi = Math.round(v)
      }
    }

    SliderRow {
      Layout.fillWidth: true
      label: "防抖（毫秒）"
      helper: "对所有接了 AI 联想的方案生效"
      min: rangeMin("debounceMs", 0)
      max: rangeMax("debounceMs", 2000)
      step: 20
      value: app.ai.debounceMs
      onEdited: function (v) {
        app.ai.debounceMs = Math.round(v)
      }
    }

  }

  Card {
    visible: page.section === "生成"
    Layout.fillWidth: true

    SliderRow {
      Layout.fillWidth: true
      showDivider: false
      label: "去重扫描范围"
      min: rangeMin("searchRange", 1)
      max: rangeMax("searchRange", 50)
      value: app.ai.searchRange
      onEdited: function (v) {
        app.ai.searchRange = Math.round(v)
      }
    }

    SliderRow {
      Layout.fillWidth: true
      label: "上下文窗口"
      min: rangeMin("contextWindowSize", 1)
      max: rangeMax("contextWindowSize", 50)
      value: app.ai.contextWindowSize
      onEdited: function (v) {
        app.ai.contextWindowSize = Math.round(v)
      }
    }

    SliderRow {
      Layout.fillWidth: true
      label: "上下文最小拼音数"
      min: rangeMin("minContextPromptLength", 1)
      max: rangeMax("minContextPromptLength", 20)
      value: app.ai.minContextPromptLength
      onEdited: function (v) {
        app.ai.minContextPromptLength = Math.round(v)
      }
    }

    SliderRow {
      Layout.fillWidth: true
      label: "最大 token 数"
      min: rangeMin("maxTokens", 8)
      max: rangeMax("maxTokens", 512)
      step: 8
      value: app.ai.maxTokens
      onEdited: function (v) {
        app.ai.maxTokens = Math.round(v)
      }
    }

  }

  Card {
    visible: page.section === "模型"
    Layout.fillWidth: true

    ComboRow {
      Layout.fillWidth: true
      showDivider: false
      label: "运算设备"
      fieldWidth: 110
      options: app.ai.deviceOptions
      value: app.ai.device
      onEdited: function (v) {
        app.ai.device = v
      }
    }

    TextRow {
      Layout.fillWidth: true
      label: "模型路径"
      readOnly: true
      fieldWidth: Math.max(220, Math.min(440, page.width - 180))
      value: app.ai.modelPath
    }
    RowLayout {
      Layout.alignment: Qt.AlignRight
      Layout.topMargin: 6
      Layout.bottomMargin: 6
      Btn { text: "选择模型目录…"; enabled: app.ai.currentSchema.length > 0; onClicked: app.ai.chooseModel() }
      Btn { text: "恢复默认模型"; enabled: app.ai.currentSchema.length > 0; onClicked: app.ai.resetModel() }
    }
  }
  }
}
