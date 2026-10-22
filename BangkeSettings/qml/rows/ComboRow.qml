import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Bangke.Pages

// mac 下拉行。弹层 = Flickable + Column + Repeater 全自绘(零 Controls 定制
// 点、零 ListView 视口依赖)。0.4.0 两版真机的教训:ComboBox 的
// delegateModel 弹层文字不可见;换成 ListView 后其视口死锁(height 绑
// contentHeight,delegate 不实例化,contentHeight 恒 0,弹层恒 6px 高)。
// Repeater 无条件实例化全部条目,高度按条目数算,不存在这个环。
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

  readonly property int currentIndex: {
    for (var i = 0; i < entries.length; ++i) {
      var v = entries[i].value
      if (v === row.value || String(v) === String(row.value))
        return i
    }
    return -1
  }
  readonly property string currentText: currentIndex >= 0
                                        ? entries[currentIndex].text
                                        : (row.value === undefined ? "" : String(row.value))

  // 显示层:文本 + chevron 的按钮壳
  Item {
    id: display
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    width: row.fieldWidth
    height: Theme.controlHeight

    Field {
      anchors.fill: parent
      focused: menu.visible
    }

    Txt {
      x: 9
      width: parent.width - 9 - 20
      height: parent.height
      text: row.currentText
      verticalAlignment: Text.AlignVCenter
      elide: Text.ElideRight
    }

    Shape {
      x: display.width - width - 9
      y: (display.height - height) / 2
      width: 9
      height: 6
      antialiasing: true
      ShapePath {
        strokeColor: Theme.ink2
        strokeWidth: 1.5
        fillColor: "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathMove { x: 1; y: 1 }
        PathLine { x: 4.5; y: 5 }
        PathLine { x: 8; y: 1 }
      }
    }

    MouseArea {
      anchors.fill: parent
      cursorShape: Qt.PointingHandCursor
      onClicked: menu.open()
    }

    Popup {
      id: menu
      objectName: "comboMenu"
      // 向下空间不够时向上翻(mac 弹出菜单同行为) —— 否则弹层超出
      // 窗口底部,最后几条永远看不到(窗口外的部分点不到也滚不到)
      readonly property bool flipUp: {
        var w = display.Window.window
        if (!w)
          return false
        var bottom = display.mapToItem(w.contentItem, 0, display.height + 5).y
        return bottom + menu.height > w.contentItem.height - 8
      }
      y: flipUp ? -height - 5 : display.height + 5
      width: row.fieldWidth
      // 高度按条目数算(行高取控件高度),不依赖内容实例化的结果
      height: Math.min(row.entries.length * Theme.controlHeight + 6, 300)
      padding: 3
      closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

      // Popup 的 enter/exit 只支持 opacity;130ms 渐入渐出即有 mac 菜单的顺滑感
      enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 130; easing.type: Easing.OutCubic }
      }
      exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100; easing.type: Easing.InCubic }
      }

      background: Rectangle {
        radius: Theme.fieldRadius
        color: Theme.surface
        border.width: 1
        border.color: Theme.hair
      }

      contentItem: Flickable {
        id: flick
        clip: true
        // 尺寸自报,不信 Popup 对 contentItem 的布局时机
        width: row.fieldWidth - 6
        height: menu.availableHeight
        contentWidth: width
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        onMovementStarted: menu.activeIndex = -1

        Column {
          id: column
          width: flick.width

          Repeater {
            model: row.entries

            delegate: Rectangle {
              id: item
              objectName: "comboItem" + index
              width: flick.width
              height: Theme.controlHeight
              radius: 5
              readonly property bool active: index === menu.activeIndex
              color: active ? Theme.accent : "transparent"

              Txt {
                x: 7
                width: parent.width - 14
                height: parent.height
                text: modelData.text
                color: item.active ? Theme.accentText : Theme.ink
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
              }

              MouseArea {
                id: hover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                // 单一活动项：鼠标与键盘共用，移出时只清理自己。
                onEntered: menu.activeIndex = index
                onPositionChanged: menu.activeIndex = index
                onExited: {
                  if (menu.activeIndex === index)
                    menu.activeIndex = -1
                }
                onClicked: {
                  row.edited(modelData.value)
                  menu.close()
                }
              }
            }
          }
        }

        // 键盘:上下移动、回车选中
        Keys.onUpPressed: function (event) {
          menu.activeIndex = Math.max(0, menu.activeIndex - 1)
          flick.contentY = Math.min(flick.contentY, menu.activeIndex * Theme.controlHeight)
          event.accepted = true
        }
        Keys.onDownPressed: function (event) {
          menu.activeIndex = Math.min(row.entries.length - 1, menu.activeIndex + 1)
          flick.contentY = Math.max(flick.contentY,
                                    (menu.activeIndex + 1) * Theme.controlHeight - flick.height)
          event.accepted = true
        }
        Keys.onReturnPressed: function (event) {
          if (menu.activeIndex >= 0 && menu.activeIndex < row.entries.length) {
            row.edited(row.entries[menu.activeIndex].value)
            menu.close()
          }
          event.accepted = true
        }
      }

      property int activeIndex: -1
      onOpened: {
        activeIndex = row.currentIndex
        flick.contentY = Math.max(0, Math.min(activeIndex * Theme.controlHeight,
                                            flick.contentHeight - flick.height))
        flick.forceActiveFocus()
      }
      onClosed: activeIndex = -1
    }
  }
}
