import QtQuick
import Bangke.Pages

// 浮层提示:底部居中,1.8 秒后自己淡走,不吃鼠标事件。
Rectangle {
  id: toast
  property string message: ""

  function show(text) {
    toast.message = text
    toast.opacity = 1
    hideTimer.restart()
  }

  width: label.implicitWidth + 32
  height: Theme.controlHeight
  radius: Math.min(Theme.pillRadius, height / 2)
  color: Theme.dark ? "#2E3537" : "#14181A"
  opacity: 0
  visible: opacity > 0

  Behavior on opacity {
    NumberAnimation { duration: 180 }
  }

  Txt {
    id: label
    anchors.centerIn: parent
    text: toast.message
    color: "#FFFFFF"
  }

  Timer {
    id: hideTimer
    interval: 1800
    onTriggered: toast.opacity = 0
  }
}
