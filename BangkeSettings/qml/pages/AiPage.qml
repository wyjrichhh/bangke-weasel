import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 智能联想页(mac AIPredictSettingsPage):单卡片,方案用分段选择器且与
// 配置项同卡 —— 切换滑块时整卡内容一起变化,直观传达「这套配置属于所选
// 方案」。行顺序与 mac 一致;取值范围/后缀都由 AiModel::fieldRanges 给,
// 不在 QML 里再抄一份。
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

  Card {
    Layout.fillWidth: true

    RowShell {
      Layout.fillWidth: true
      showDivider: false
      label: "输入方案"
      helper: "每个方案单独保存一套 AI 设置"
      controlWidth: seg.width

      MacSegmented {
        id: seg
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        options: schemaOptions
        value: app.ai.currentSchema
        onEdited: function (v) {
          app.ai.currentSchema = v
        }
      }
    }

    ToggleRow {
      Layout.fillWidth: true
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
      helper: "AI 候选必须排在扫描范围之后才会出现"
      min: rangeMin("targetIndex", 0)
      max: rangeMax("targetIndex", 20)
      value: app.ai.targetIndex
      onEdited: function (v) {
        app.ai.targetIndex = Math.round(v)
      }
    }

    SliderRow {
      Layout.fillWidth: true
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

    Hairline {
      Layout.fillWidth: true
      Layout.topMargin: 8
      Layout.bottomMargin: 8
      color: Theme.hairSoft
    }

    SliderRow {
      Layout.fillWidth: true
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

    ComboRow {
      Layout.fillWidth: true
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
      fieldWidth: 260
      value: app.ai.modelPath
    }
  }
}
