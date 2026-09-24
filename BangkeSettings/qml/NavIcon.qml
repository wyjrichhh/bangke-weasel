import QtQuick
import QtQuick.Shapes
import Bangke.Pages

// 侧栏线性图标,16 逻辑像素,对应 mac 的 SF Symbols:
// 0 调色板(paintpalette) 1 键盘(keyboard) 2 星光(sparkles)
// 3 列表(方案与词典) 4 ⓘ(info.circle)。五条 Shape 各画一个,只显示
// which 对应的那条 —— 路径按 16 单位视图写死,不依赖图片资源。
Item {
  id: icon
  property int which: 0
  property color color: Theme.ink2
  // 调色板的指孔要挖空,颜色跟底下的选中药丸一致
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
      // 调色板盘体:大圆开一个右侧缺口
      PathMove { x: 12.9; y: 4.1 }
      PathArc { x: 4.1; y: 3.4; radiusX: 5.8; radiusY: 5.8; direction: PathArc.Clockwise }
      PathArc { x: 3.6; y: 12.6; radiusX: 5.8; radiusY: 5.8; direction: PathArc.Clockwise }
      PathArc { x: 9.2; y: 13.9; radiusX: 5.8; radiusY: 5.8; direction: PathArc.Clockwise }
      PathLine { x: 9.6; y: 11.7 }
      PathArc { x: 7.4; y: 8.2; radiusX: 2.9; radiusY: 2.9; useLargeArc: true }
      PathArc { x: 11.0; y: 9.0; radiusX: 2.9; radiusY: 2.9 }
      PathArc { x: 12.9; y: 4.1; radiusX: 2.6; radiusY: 2.6 }
    }
    ShapePath {
      strokeColor: "transparent"
      fillColor: icon.color
      PathMove { x: 5.6; y: 6.0 }
      PathArc { x: 6.8; y: 6.0; radiusX: 0.6; radiusY: 0.6 }
      PathArc { x: 5.6; y: 6.0; radiusX: 0.6; radiusY: 0.6 }
      PathMove { x: 8.4; y: 4.9 }
      PathArc { x: 9.6; y: 4.9; radiusX: 0.6; radiusY: 0.6 }
      PathArc { x: 8.4; y: 4.9; radiusX: 0.6; radiusY: 0.6 }
      PathMove { x: 4.4; y: 9.2 }
      PathArc { x: 5.6; y: 9.2; radiusX: 0.6; radiusY: 0.6 }
      PathArc { x: 4.4; y: 9.2; radiusX: 0.6; radiusY: 0.6 }
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
      PathMove { x: 2.2; y: 4.4 }
      PathLine { x: 13.8; y: 4.4 }
      PathArc { x: 14.6; y: 5.2; radiusX: 0.8; radiusY: 0.8; direction: PathArc.Clockwise }
      PathLine { x: 14.6; y: 10.8 }
      PathArc { x: 13.8; y: 11.6; radiusX: 0.8; radiusY: 0.8; direction: PathArc.Clockwise }
      PathLine { x: 2.2; y: 11.6 }
      PathArc { x: 1.4; y: 10.8; radiusX: 0.8; radiusY: 0.8; direction: PathArc.Clockwise }
      PathLine { x: 1.4; y: 5.2 }
      PathArc { x: 2.2; y: 4.4; radiusX: 0.8; radiusY: 0.8; direction: PathArc.Clockwise }
      PathMove { x: 3.4; y: 7.0 }
      PathLine { x: 3.6; y: 7.0 }
      PathMove { x: 5.4; y: 7.0 }
      PathLine { x: 5.6; y: 7.0 }
      PathMove { x: 7.4; y: 7.0 }
      PathLine { x: 7.6; y: 7.0 }
      PathMove { x: 9.4; y: 7.0 }
      PathLine { x: 9.6; y: 7.0 }
      PathMove { x: 11.4; y: 7.0 }
      PathLine { x: 12.6; y: 7.0 }
      PathMove { x: 3.4; y: 9.4 }
      PathLine { x: 12.6; y: 9.4 }
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
      PathMove { x: 5.2; y: 4.0 }
      PathLine { x: 13.6; y: 4.0 }
      PathMove { x: 5.2; y: 8.0 }
      PathLine { x: 13.6; y: 8.0 }
      PathMove { x: 5.2; y: 12.0 }
      PathLine { x: 13.6; y: 12.0 }
    }
    ShapePath {
      strokeColor: "transparent"
      fillColor: icon.color
      PathMove { x: 1.9; y: 4.0 }
      PathArc { x: 3.3; y: 4.0; radiusX: 0.7; radiusY: 0.7 }
      PathArc { x: 1.9; y: 4.0; radiusX: 0.7; radiusY: 0.7 }
      PathMove { x: 1.9; y: 8.0 }
      PathArc { x: 3.3; y: 8.0; radiusX: 0.7; radiusY: 0.7 }
      PathArc { x: 1.9; y: 8.0; radiusX: 0.7; radiusY: 0.7 }
      PathMove { x: 1.9; y: 12.0 }
      PathArc { x: 3.3; y: 12.0; radiusX: 0.7; radiusY: 0.7 }
      PathArc { x: 1.9; y: 12.0; radiusX: 0.7; radiusY: 0.7 }
    }
  }

  Shape {
    anchors.fill: parent
    antialiasing: true
    visible: icon.which === 4
    ShapePath {
      strokeColor: icon.color
      strokeWidth: 1.28
      fillColor: "transparent"
      capStyle: ShapePath.RoundCap
      PathMove { x: 8; y: 1.6 }
      PathArc { x: 14.4; y: 8; radiusX: 6.4; radiusY: 6.4; direction: PathArc.Clockwise }
      PathArc { x: 8; y: 14.4; radiusX: 6.4; radiusY: 6.4; direction: PathArc.Clockwise }
      PathArc { x: 1.6; y: 8; radiusX: 6.4; radiusY: 6.4; direction: PathArc.Clockwise }
      PathArc { x: 8; y: 1.6; radiusX: 6.4; radiusY: 6.4; direction: PathArc.Clockwise }
      PathMove { x: 8; y: 7.2 }
      PathLine { x: 8; y: 11.2 }
    }
    ShapePath {
      strokeColor: "transparent"
      fillColor: icon.color
      PathMove { x: 7.2; y: 4.8 }
      PathArc { x: 8.8; y: 4.8; radiusX: 0.8; radiusY: 0.8 }
      PathArc { x: 7.2; y: 4.8; radiusX: 0.8; radiusY: 0.8 }
    }
  }
}
