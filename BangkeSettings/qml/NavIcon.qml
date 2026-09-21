import QtQuick
import QtQuick.Shapes
import Bangke.Pages

// 侧栏线性图标,16 逻辑像素。四条 Shape 各画一个,只显示 which 对应的那条 ——
// 路径按稿子的 20 单位视图缩放 0.8 写死,不再依赖图片资源。
Item {
  id: icon
  property int which: 0
  property color color: Theme.ink2
  // 高级那个图标要把滑杆挖断,挖空的颜色得跟底下的药丸一致
  property color holeColor: Theme.surface

  implicitWidth: 16
  implicitHeight: 16

  Shape {
    anchors.fill: parent
    antialiasing: true
    visible: icon.which === 0
    ShapePath {
      strokeColor: icon.color
      strokeWidth: 1.28
      fillColor: "transparent"
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin
      PathMove { x: 4.48; y: 2.56 }
      PathLine { x: 11.52; y: 2.56 }
      PathArc { x: 13.92; y: 4.96; radiusX: 2.4; radiusY: 2.4; direction: PathArc.Clockwise }
      PathLine { x: 13.92; y: 11.04 }
      PathArc { x: 11.52; y: 13.44; radiusX: 2.4; radiusY: 2.4; direction: PathArc.Clockwise }
      PathLine { x: 4.48; y: 13.44 }
      PathArc { x: 2.08; y: 11.04; radiusX: 2.4; radiusY: 2.4; direction: PathArc.Clockwise }
      PathLine { x: 2.08; y: 4.96 }
      PathArc { x: 4.48; y: 2.56; radiusX: 2.4; radiusY: 2.4; direction: PathArc.Clockwise }
      PathMove { x: 2.08; y: 6.08 }
      PathLine { x: 13.92; y: 6.08 }
      PathMove { x: 5.92; y: 6.08 }
      PathLine { x: 5.92; y: 13.44 }
    }
  }

  Shape {
    anchors.fill: parent
    antialiasing: true
    visible: icon.which === 1
    ShapePath {
      strokeColor: icon.color
      strokeWidth: 1.28
      fillColor: "transparent"
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin
      PathMove { x: 2.08; y: 8 }
      PathArc { x: 13.92; y: 8; radiusX: 5.92; radiusY: 5.92 }
      PathArc { x: 2.08; y: 8; radiusX: 5.92; radiusY: 5.92 }
      PathMove { x: 4.96; y: 8 }
      PathArc { x: 11.04; y: 8; radiusX: 3.04; radiusY: 5.92 }
      PathArc { x: 4.96; y: 8; radiusX: 3.04; radiusY: 5.92 }
      PathMove { x: 2.4; y: 6.08 }
      PathLine { x: 13.6; y: 6.08 }
      PathMove { x: 2.4; y: 9.92 }
      PathLine { x: 13.6; y: 9.92 }
    }
  }

  Shape {
    anchors.fill: parent
    antialiasing: true
    visible: icon.which === 2
    ShapePath {
      strokeColor: icon.color
      strokeWidth: 1.28
      fillColor: "transparent"
      capStyle: ShapePath.RoundCap
      joinStyle: ShapePath.RoundJoin
      PathMove { x: 8; y: 1.92 }
      PathLine { x: 9.36; y: 5.36 }
      PathLine { x: 12.8; y: 6.72 }
      PathLine { x: 9.36; y: 8.08 }
      PathLine { x: 8; y: 11.52 }
      PathLine { x: 6.64; y: 8.08 }
      PathLine { x: 3.2; y: 6.72 }
      PathLine { x: 6.64; y: 5.36 }
      PathLine { x: 8; y: 1.92 }
      PathMove { x: 12.32; y: 10.88 }
      PathLine { x: 12.88; y: 12.24 }
      PathLine { x: 14.24; y: 12.8 }
      PathLine { x: 12.88; y: 13.36 }
      PathLine { x: 12.32; y: 14.72 }
      PathLine { x: 11.76; y: 13.36 }
      PathLine { x: 10.4; y: 12.8 }
      PathLine { x: 11.76; y: 12.24 }
      PathLine { x: 12.32; y: 10.88 }
    }
  }

  Shape {
    anchors.fill: parent
    antialiasing: true
    visible: icon.which === 3
    ShapePath {
      strokeColor: icon.color
      strokeWidth: 1.28
      fillColor: "transparent"
      capStyle: ShapePath.RoundCap
      PathMove { x: 2.4; y: 5.12 }
      PathLine { x: 13.6; y: 5.12 }
      PathMove { x: 2.4; y: 8 }
      PathLine { x: 13.6; y: 8 }
      PathMove { x: 2.4; y: 10.88 }
      PathLine { x: 13.6; y: 10.88 }
    }
    ShapePath {
      strokeColor: icon.color
      strokeWidth: 1.28
      fillColor: icon.holeColor
      PathMove { x: 3.76; y: 5.12 }
      PathArc { x: 6.8; y: 5.12; radiusX: 1.52; radiusY: 1.52 }
      PathArc { x: 3.76; y: 5.12; radiusX: 1.52; radiusY: 1.52 }
      PathMove { x: 9.2; y: 8 }
      PathArc { x: 12.24; y: 8; radiusX: 1.52; radiusY: 1.52 }
      PathArc { x: 9.2; y: 8; radiusX: 1.52; radiusY: 1.52 }
      PathMove { x: 5.2; y: 10.88 }
      PathArc { x: 8.24; y: 10.88; radiusX: 1.52; radiusY: 1.52 }
      PathArc { x: 5.2; y: 10.88; radiusX: 1.52; radiusY: 1.52 }
    }
  }
}
