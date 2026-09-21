import QtQuick
import Bangke.Pages

// styleControls 里的一行:按 kind 挑控件,写回只有 model.value 一个出口
// (StyleControlModel::setData 会按 kind 收下 bool / int / 字符串)。
// kind 的取值: toggle / spin / edit / combo。
Item {
  id: row

  readonly property string label: model.label
  readonly property string helper: model.helper
  readonly property var rawValue: model.value
  readonly property var optionList: model.options === undefined ? [] : model.options
  readonly property int minValue: model.min === undefined ? 0 : Number(model.min)
  readonly property int maxValue: model.max === undefined ? 0 : Number(model.max)

  property bool showDivider: true

  implicitHeight: Theme.rowHeight

  function readBool() {
    var v = row.rawValue
    return v === true || v === 1 || v === "1" || v === "true"
  }

  function readNumber() {
    var v = Number(row.rawValue)
    return isNaN(v) ? 0 : v
  }

  function readText() {
    return row.rawValue === undefined || row.rawValue === null ? "" : String(row.rawValue)
  }

  function write(v) {
    if (row.rawValue !== v)
      model.value = v
  }

  Loader {
    anchors.fill: parent
    active: row.visible
    sourceComponent: model.kind === "toggle" ? toggleCell
                     : model.kind === "spin" ? spinCell
                     : model.kind === "edit" ? textCell
                     : model.kind === "combo" ? comboCell
                     : null
  }

  Component {
    id: toggleCell
    ToggleRow {
      label: row.label
      helper: row.helper
      showDivider: row.showDivider
      value: row.readBool()
      onEdited: function (v) {
        row.write(v)
      }
    }
  }

  Component {
    id: spinCell
    SpinRow {
      label: row.label
      helper: row.helper
      showDivider: row.showDivider
      min: row.minValue
      max: row.maxValue
      value: row.readNumber()
      onEdited: function (v) {
        row.write(v)
      }
    }
  }

  Component {
    id: textCell
    TextRow {
      label: row.label
      helper: row.helper
      showDivider: row.showDivider
      value: row.readText()
      onEdited: function (v) {
        row.write(v)
      }
    }
  }

  Component {
    id: comboCell
    ComboRow {
      label: row.label
      helper: row.helper
      showDivider: row.showDivider
      options: row.optionList
      value: row.rawValue
      onEdited: function (v) {
        row.write(v)
      }
    }
  }
}
