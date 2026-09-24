import QtQuick
import Bangke.Pages

// mac 风标题栏:左上红黄绿圆点、居中标题、透明底(底下是侧栏/内容各自的
// 背景色,与 mac titlebarSeparatorStyle = none 的无缝观感一致)。
// 拖动/双击缩放由 WinChrome 的 WM_NCHITTEST 提供,这里只画和点。
Item {
  id: bar

  implicitHeight: Theme.titleBarHeight
  z: 10

  readonly property var win: bar.Window.window

  component Dot: Rectangle {
    id: dot
    required property color face
    required property string glyph
    readonly property bool hovered: hover.containsMouse
    width: 12
    height: 12
    radius: 6
    color: !bar.win || bar.win.active ? face : "#C8C8CD"

    Txt {
      anchors.centerIn: parent
      text: dot.hovered && bar.win && bar.win.active ? dot.glyph : ""
      color: Qt.rgba(0, 0, 0, 0.55)
      font.pixelSize: 9
      font.bold: true
    }

    MouseArea {
      id: hover
      anchors.fill: parent
      hoverEnabled: true
      cursorShape: Qt.ArrowCursor
      onClicked: {
        if (!bar.win)
          return
        if (dot.glyph === "×")
          bar.win.close()
        else if (dot.glyph === "−")
          bar.win.showMinimized()
        else
          bar.win.visibility === Window.Maximized
              ? bar.win.showNormal() : bar.win.showMaximized()
      }
    }
  }

  Row {
    x: 20
    y: (Theme.titleBarHeight - 12) / 2
    spacing: 8

    Dot { face: "#FF5F57"; glyph: "×" }
    Dot { face: "#FEBC2E"; glyph: "−" }
    Dot { face: "#28C840"; glyph: "+" }
  }

  Txt {
    anchors.centerIn: parent
    text: "蚌壳拼音设置"
    color: Theme.ink2
    font.pixelSize: 13
    font.weight: Font.DemiBold
  }
}
