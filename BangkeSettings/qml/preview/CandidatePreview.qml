import QtQuick
import QtQuick.Effects
import Bangke.Pages

// 候选窗预览:按真实取值重画 —— 配色、圆角都取自 app.style.preview*。
// 画法不再复用面板的绘制代码,所以不追求像素级一致,但颜色和圆角是诚实的。
// 字体跟着界面走(模型没暴露候选窗字体,与旧预览一样是写死的一档)。
Item {
  id: preview

  implicitWidth: 380
  implicitHeight: 190

  readonly property var palette: app.style.previewColors
  readonly property bool horizontal: app.style.previewHorizontal
  readonly property int panelRadius: app.style.previewPanelRadius
  readonly property int hiliteRadius: app.style.previewHiliteRadius
  readonly property int fontPoint: 9
  readonly property string fontFace: Theme.fontFamily

  readonly property var candidates: [
    { label: "1.", text: "蚌壳", comment: "预测" },
    { label: "2.", text: "帮客", comment: "" },
    { label: "3.", text: "绑客", comment: "" }
  ]

  function tint(key, fallback) {
    return Theme.argb(preview.palette ? preview.palette[key] : undefined, fallback)
  }

  function candWidth(i) {
    return 74 + (preview.candidates[i].comment.length > 0 ? 46 : 0)
  }

  function candX(i) {
    var x = 10
    for (var k = 0; k < i; ++k)
      x += preview.candWidth(k) + 8
    return x
  }

  Rectangle {
    id: panel
    x: 16
    y: 10
    width: preview.width - 32
    height: preview.height - 26
    radius: preview.panelRadius
    color: preview.tint("back_color", "#FAFFFFFF")
    border.width: 1
    border.color: preview.tint("border_color", "#FFE3E3E8")
    visible: false
  }

  MultiEffect {
    source: panel
    anchors.fill: panel
    shadowEnabled: true
    shadowColor: preview.tint("shadow_color", "#28000000")
    shadowBlur: 0.6
    shadowVerticalOffset: 3
  }

  Item {
    id: face
    anchors.fill: panel

    Text {
      x: 12
      y: 8
      width: face.width - 24
      height: 20
      text: "bang'ke pinyin"
      color: preview.tint("text_color", "#FF1F2937")
      font.family: preview.fontFace
      font.pointSize: preview.fontPoint
      verticalAlignment: Text.AlignVCenter
      renderType: Text.NativeRendering
    }

    Repeater {
      model: 3

      delegate: Item {
        x: preview.horizontal ? preview.candX(index) : 8
        y: preview.horizontal ? 32 : 30 + index * 34
        width: preview.horizontal ? preview.candWidth(index) : face.width - 16
        height: 30

        Rectangle {
          anchors.fill: parent
          radius: preview.hiliteRadius
          color: preview.tint("hilited_candidate_back_color", "#FFA8C7F0")
          visible: index === 0
        }

        Text {
          x: 8
          height: parent.height
          text: preview.candidates[index].label
          color: index === 0 ? preview.tint("hilited_label_color", "#FF51709A")
                             : preview.tint("label_color", "#FF9CA3AF")
          font.family: preview.fontFace
          font.pointSize: preview.fontPoint
          verticalAlignment: Text.AlignVCenter
          renderType: Text.NativeRendering
        }

        Text {
          x: 30
          height: parent.height
          text: preview.candidates[index].text
          color: index === 0 ? preview.tint("hilited_candidate_text_color", "#FF16202B")
                             : preview.tint("candidate_text_color", "#FF2F3437")
          font.family: preview.fontFace
          font.pointSize: preview.fontPoint
          verticalAlignment: Text.AlignVCenter
          renderType: Text.NativeRendering
        }

        Text {
          x: 82
          height: parent.height
          text: preview.candidates[index].comment
          visible: text.length > 0
          color: index === 0 ? preview.tint("hilited_label_color", "#FF51709A")
                             : preview.tint("comment_text_color", "#FF9CA3AF")
          font.family: preview.fontFace
          font.pointSize: preview.fontPoint
          verticalAlignment: Text.AlignVCenter
          renderType: Text.NativeRendering
        }
      }
    }
  }
}
