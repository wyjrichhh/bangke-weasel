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
    height: 28

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
      y: display.height + 5
      width: row.fieldWidth
      // 高度按条目数算(行高恒 24),不依赖内容实例化的结果
      height: Math.min(row.entries.length * 24 + 6, 300)
      padding: 3
      closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

      // Popup 的 enter/exit 只支持 opacity;130ms 渐入渐出即有 mac 菜单的顺滑感
      enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 130; easing.type: Easing.OutCubic }
      }
      exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100; easing.type: Easing.InCubic }
      }

      background: Item {
        // 软影 = 同心递淡的圆角矩形叠层。不用 MultiEffect:shader 阴影在
        // 软件/RDP 渲染下 blur 退化,变成实心轮廓块(「外层大方块」的根因);
        // 纯 Rectangle 在任何渲染器下都可靠
        Rectangle {
          anchors.fill: parent
          anchors.margins: -2
          anchors.bottomMargin: -7
          radius: Theme.fieldRadius + 3
          color: Qt.rgba(0, 0, 0, Theme.dark ? 0.30 : 0.13)
        }
        Rectangle {
          anchors.fill: parent
          anchors.margins: -1
          anchors.bottomMargin: -4
          radius: Theme.fieldRadius + 2
          color: Qt.rgba(0, 0, 0, Theme.dark ? 0.20 : 0.09)
        }
        Rectangle {
          anchors.fill: parent
          radius: Theme.fieldRadius + 1
          color: Theme.dark ? Qt.rgba(0.17, 0.17, 0.18, 0.90)
                            : Qt.rgba(1, 1, 1, 0.88)
          border.width: 1
          border.color: Theme.dark ? Qt.rgba(1, 1, 1, 0.14)
                                   : Qt.rgba(0, 0, 0, 0.12)
        }
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

        Column {
          id: column
          width: flick.width

          Repeater {
            model: row.entries

            delegate: Rectangle {
              id: item
              width: flick.width
              height: 24
              radius: 5
              readonly property bool active: hover.containsMouse ||
                                              index === menu.keyIndex
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
          menu.keyIndex = Math.max(0, menu.keyIndex - 1)
          flick.contentY = Math.min(flick.contentY, menu.keyIndex * 24)
          event.accepted = true
        }
        Keys.onDownPressed: function (event) {
          menu.keyIndex = Math.min(row.entries.length - 1, menu.keyIndex + 1)
          flick.contentY = Math.max(flick.contentY,
                                    (menu.keyIndex + 1) * 24 - flick.height)
          event.accepted = true
        }
        Keys.onReturnPressed: function (event) {
          if (menu.keyIndex >= 0 && menu.keyIndex < row.entries.length) {
            row.edited(row.entries[menu.keyIndex].value)
            menu.close()
          }
          event.accepted = true
        }
      }

      property int keyIndex: -1
      onOpened: keyIndex = Math.max(0, row.currentIndex)
      onClosed: keyIndex = -1
    }
  }
}
