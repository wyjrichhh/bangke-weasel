import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Bangke.Pages

// 下拉行。Basic style 的箭头/弹层整个换掉:箭头是画出来的,弹层用卡片的样子。
// options 收 C++ 侧的 {label, value} 映射,也收纯字符串。
RowShell {
  id: row
  property var value
  property var options: []
  property real fieldWidth: 170
  signal edited(var value)

  controlWidth: row.fieldWidth

  readonly property var entries: {
    var out = []
    var src = row.options
    if (!src)
      return out
    for (var i = 0; i < src.length; ++i) {
      var item = src[i]
      if (item === null || item === undefined)
        continue
      if (typeof item === "object") {
        var text = item.label !== undefined ? item.label
                   : item.text !== undefined ? item.text
                   : item.name !== undefined ? item.name : item.value
        var val = item.value !== undefined ? item.value : item.id
        out.push({ text: String(text), value: val })
      } else {
        out.push({ text: String(item), value: item })
      }
    }
    return out
  }

  ComboBox {
    id: combo
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    implicitWidth: row.fieldWidth
    implicitHeight: Theme.controlHeight
    leftPadding: 11
    rightPadding: 25
    model: row.entries
    textRole: "text"
    valueRole: "value"
    // 列表里没有当前值时(比如选项还没载入)至少把值本身显示出来
    displayText: currentIndex >= 0 ? currentText : (row.value === undefined ? "" : String(row.value))
    onActivated: row.edited(currentValue)

    contentItem: Txt {
      text: combo.displayText
      verticalAlignment: Text.AlignVCenter
      elide: Text.ElideRight
    }

    indicator: Shape {
      x: combo.width - width - 11
      y: (combo.height - height) / 2
      width: 10
      height: 6
      antialiasing: true
      ShapePath {
        strokeColor: Theme.ink3
        strokeWidth: 1.5
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathMove { x: 1; y: 1 }
        PathLine { x: 5; y: 5 }
        PathLine { x: 9; y: 1 }
      }
    }

    background: Field { focused: combo.activeFocus }

    delegate: ItemDelegate {
      id: entry
      width: combo.width
      implicitHeight: 30
      highlighted: combo.highlightedIndex === index

      contentItem: Txt {
        text: modelData.text
        color: entry.highlighted ? Theme.accentInk : Theme.ink
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
      }

      background: Rectangle {
        radius: 6
        color: entry.highlighted ? Theme.accentSoft : "transparent"
      }
    }

    popup: Popup {
      y: combo.height + 4
      width: combo.width
      implicitHeight: Math.min(contentItem.implicitHeight + 8, 280)
      padding: 4

      background: Rectangle {
        radius: Theme.fieldRadius
        color: Theme.surface
        border.width: 1
        border.color: Theme.hair
      }

      contentItem: ListView {
        clip: true
        implicitHeight: contentHeight
        model: combo.delegateModel
        currentIndex: combo.highlightedIndex
        boundsBehavior: Flickable.StopAtBounds
      }
    }
  }
}
