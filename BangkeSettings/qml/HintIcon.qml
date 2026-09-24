import QtQuick
import Bangke.Pages

// 行标签旁的小问号:悬停 0.8s 弹出自制说明气泡。系统气泡(tooltip)在
// 输入法进程里不可用(微软雅黑 UI 的 tooltip 也有延迟不可调的问题),
// 与 mac 版 HintIcon 同一套做法。
Item {
  id: icon
  property string hint: ""

  implicitWidth: 16
  implicitHeight: 16

  Txt {
    anchors.centerIn: parent
    text: "?"
    color: Theme.ink3
    font.pixelSize: Theme.subSize
    font.bold: true
  }

  MouseArea {
    id: hover
    anchors.fill: parent
    hoverEnabled: true
    onContainsMouseChanged: {
      if (hover.containsMouse)
        delay.restart()
      else {
        delay.stop()
        bubble.visible = false
      }
    }
  }

  Timer {
    id: delay
    interval: 800
    onTriggered: bubble.visible = true
  }

  Rectangle {
    id: bubble
    visible: false
    z: 100
    x: -8
    // 向上弹:QML 里后画的兄弟行盖前画的,气泡朝上才能压住上面的行
    y: -(height + 2)
    width: Math.min(320, label.implicitWidth + 16)
    height: label.implicitHeight + 10
    radius: 5
    color: Theme.dark ? Qt.rgba(0.12, 0.12, 0.12, 0.96) : Qt.rgba(1, 1, 1, 0.98)
    border.width: 1
    border.color: Theme.hair

    Txt {
      id: label
      x: 8
      y: 5
      width: bubble.width - 16
      text: icon.hint
      color: Theme.ink
      font.pixelSize: Theme.hintSize
      elide: Text.ElideRight
    }
  }
}
