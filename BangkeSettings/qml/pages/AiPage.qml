import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// AI 预测:一页管所有接了 AI 的方案,按方案各存一份(行清单照搬 AIPage.cpp)。
// 取值范围/后缀都由 AiModel::fieldRanges 给,不在 QML 里再抄一份。
PageScroll {

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

  function rangeSuffix(key) {
    var r = app.ai.fieldRanges ? app.ai.fieldRanges[key] : null
    return r && r.suffix ? r.suffix : ""
  }

  Card {
    Layout.fillWidth: true
    title: "基本"

    ComboRow {
      Layout.fillWidth: true
      showDivider: false
      label: "输入方案"
      options: schemaOptions
      value: app.ai.currentSchema
      onEdited: function (v) {
        app.ai.currentSchema = v
      }
    }

    ToggleRow {
      Layout.fillWidth: true
      label: "启用 AI 预测"
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

    ComboRow {
      Layout.fillWidth: true
      label: "设备"
      fieldWidth: 150
      options: app.ai.deviceOptions
      value: app.ai.device
      onEdited: function (v) {
        app.ai.device = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "最大 token 数"
      min: rangeMin("maxTokens", 1)
      max: rangeMax("maxTokens", 1024)
      value: app.ai.maxTokens
      onEdited: function (v) {
        app.ai.maxTokens = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "防抖"
      min: rangeMin("debounceMs", 0)
      max: rangeMax("debounceMs", 5000)
      suffix: rangeSuffix("debounceMs")
      value: app.ai.debounceMs
      onEdited: function (v) {
        app.ai.debounceMs = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "最小输入长度"
      min: rangeMin("minInputLength", 1)
      max: rangeMax("minInputLength", 100)
      value: app.ai.minInputLength
      onEdited: function (v) {
        app.ai.minInputLength = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "最少汉字数"
      min: rangeMin("minHanzi", 1)
      max: rangeMax("minHanzi", 10)
      value: app.ai.minHanzi
      onEdited: function (v) {
        app.ai.minHanzi = v
      }
    }

    Txt {
      text: app.ai.currentSchema.length > 0
            ? "设置按方案各存一份,当前编辑:" + app.ai.currentSchemaName
            : "没有接入 AI 预测的方案"
      color: Theme.ink3
      font.pixelSize: Theme.hintSize
      Layout.fillWidth: true
      Layout.topMargin: 8
      Layout.bottomMargin: 2
    }
  }

  Card {
    Layout.fillWidth: true
    title: "高级选项"

    SpinRow {
      Layout.fillWidth: true
      showDivider: false
      label: "AI 候选显示位置 (target_index)"
      min: rangeMin("targetIndex", 0)
      max: rangeMax("targetIndex", 20)
      value: app.ai.targetIndex
      onEdited: function (v) {
        app.ai.targetIndex = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "去重扫描范围 (search_range)"
      min: rangeMin("searchRange", 1)
      max: rangeMax("searchRange", 100)
      value: app.ai.searchRange
      onEdited: function (v) {
        app.ai.searchRange = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "上下文窗口 (context_window_size)"
      min: rangeMin("contextWindowSize", 1)
      max: rangeMax("contextWindowSize", 100)
      value: app.ai.contextWindowSize
      onEdited: function (v) {
        app.ai.contextWindowSize = v
      }
    }

    SpinRow {
      Layout.fillWidth: true
      label: "上下文最小拼音数"
      min: rangeMin("minContextPromptLength", 1)
      max: rangeMax("minContextPromptLength", 20)
      value: app.ai.minContextPromptLength
      onEdited: function (v) {
        app.ai.minContextPromptLength = v
      }
    }

    TextRow {
      Layout.fillWidth: true
      label: "模型路径"
      readOnly: true
      fieldWidth: 280
      value: app.ai.modelPath
    }
  }
}
