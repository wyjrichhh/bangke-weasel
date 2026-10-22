import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 双主题实时预览；背景、文字和高亮统一透明，与实际候选窗一致。
ColumnLayout {
  id: strip
  spacing: 10

  component ModePill: Rectangle {
    id: pill
    required property string text
    height: 22
    width: label.implicitWidth + 14
    radius: height / 2
    color: Theme.dark ? "#273943" : "#FFFFFF"

    Txt {
      id: label
      anchors.centerIn: parent
      text: pill.text
      color: Theme.ink
      font.pixelSize: Theme.hintSize
    }
  }

  component Half: Item {
    id: half
    required property bool dark
    readonly property alias panel: panelItem
    clip: true

    Rectangle {
      anchors.fill: parent
      radius: Theme.cardRadius
      color: half.dark ? "#23333D" : "#EDF5F8"
    }

    MiniPanel {
      id: panelItem
      colors: half.dark ? app.style.previewDark : app.style.previewLight
      m: app.style.previewMetrics
      // 等比 fit,且四周留出均匀呼吸(两侧各 28)—— 面板永不顶边铺满,
      // 这是 mac 预览观感的另一半:等比、居中、有留白。
      // zoom 乘进字号与度量(不是 Item.scale,整体缩放会把文本压糊)
      readonly property real z1: (half.width - 56) / Math.max(1, panelItem.unitW)
      readonly property real z2: (half.height - 56) / Math.max(1, panelItem.unitH)
      zoom: Math.max(0.05, Math.min(1.15, z1, z2))
      x: (half.width - width) / 2
      y: (half.height - height) / 2
    }

    ModePill {
      text: half.dark ? "深色" : "浅色"
      x: 8
      y: 8
    }
  }

  Rectangle {
    id: frame
    Layout.fillWidth: true
    Layout.preferredHeight: 180
    color: "transparent"
    clip: true

    // 切分方向直接跟随「候选排列」(mac 的测量判据本质就是区分单行/多行;
    // 用配置值零测量误差):单行横条上下切分(各得全宽),多行面板左右切分
    readonly property bool singleLine: app.style.previewHorizontal
    readonly property real gap: 12

    Half { id: lightHalf; dark: false }
    Half { id: darkHalf; dark: true }

    // 竖排候选左右展示；横排候选上下展示，避免压缩长候选。
    states: [
      State {
        name: "stackedPanels"
        when: !frame.singleLine
        PropertyChanges {
          target: lightHalf
          x: 0
          y: 0
          width: (frame.width - frame.gap) / 2
          height: frame.height
        }
        PropertyChanges {
          target: darkHalf
          x: (frame.width + frame.gap) / 2
          y: 0
          width: (frame.width - frame.gap) / 2
          height: frame.height
        }
      },
      State {
        name: "sideBySidePanels"
        when: frame.singleLine
        PropertyChanges {
          target: lightHalf
          x: 0
          y: 0
          width: frame.width
          height: (frame.height - frame.gap) / 2
        }
        PropertyChanges {
          target: darkHalf
          x: 0
          y: (frame.height + frame.gap) / 2
          width: frame.width
          height: (frame.height - frame.gap) / 2
        }
      }
    ]

  }
}
