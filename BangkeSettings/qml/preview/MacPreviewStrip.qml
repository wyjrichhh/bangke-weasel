import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Bangke.Pages

ColumnLayout {
  id: strip
  spacing: 12
  property bool longSample: false
  property int previewPageSize: -1
  property bool showComments: true
  property bool previewAi: false
  onPreviewPageSizeChanged: refreshPreview()

  function copyMap(source) {
    let result = {}
    for (let key in source)
      result[key] = source[key]
    return result
  }

  function refreshPreview() {
    // Snapshot QVariantMaps after their notify signal; nested bindings can retain
    // the empty maps created before the C++ model's deferred first load.
    if (!panel) return
    panel.colors = copyMap(Theme.dark ? app.style.previewDark : app.style.previewLight)
    let metrics = copyMap(app.style.previewMetrics)
    if (previewPageSize > 0) metrics.pageSize = previewPageSize
    panel.m = metrics
  }

  Component.onCompleted: refreshPreview()
  Connections {
    target: app.style
    function onPreviewChanged() { strip.refreshPreview() }
  }
  Connections {
    target: Theme
    function onDarkChanged() { strip.refreshPreview() }
  }

  Txt { text: "实时预览"; color: Theme.ink2 }
  Rectangle {
    Layout.fillWidth: true
    Layout.preferredHeight: 360
    radius: Theme.cardRadius
    color: Theme.surface
    border.color: Theme.hair

    Canvas {
      id: sceneGlow
      anchors.fill: parent
      anchors.margins: 1
      visible: Theme.dark
      onWidthChanged: requestPaint()
      onHeightChanged: requestPaint()
      Connections {
        target: Theme
        function onDarkChanged() { sceneGlow.requestPaint() }
      }
      onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        const r = Theme.cardRadius - 1
        ctx.beginPath()
        ctx.moveTo(r, 0)
        ctx.lineTo(width - r, 0)
        ctx.quadraticCurveTo(width, 0, width, r)
        ctx.lineTo(width, height - r)
        ctx.quadraticCurveTo(width, height, width - r, height)
        ctx.lineTo(r, height)
        ctx.quadraticCurveTo(0, height, 0, height - r)
        ctx.lineTo(0, r)
        ctx.quadraticCurveTo(0, 0, r, 0)
        ctx.clip()
        // Match the demo's elliptical glow at the scene's upper-right corner.
        ctx.scale(1, height / width)
        const glow = ctx.createRadialGradient(width, 0, 0, width, 0, width)
        glow.addColorStop(0, Theme.accentSoft)
        glow.addColorStop(0.7, Theme.surface)
        glow.addColorStop(1, Theme.surface)
        ctx.fillStyle = glow
        ctx.fillRect(0, 0, width, width)
      }
    }

    ColumnLayout {
      anchors.fill: parent
      anchors.margins: 20
      spacing: 16
      RowLayout {
        Layout.fillWidth: true
        Txt { text: "文稿 · 输入场景"; color: Theme.ink2 }
        Item { Layout.fillWidth: true }
        Txt { text: Theme.dark ? "深色预览" : "浅色预览"; color: Theme.ink2 }
      }
      Txt { text: app.style.previewMetrics.inlinePreedit === false ? "今天，" : "今天，bangke"; font.pixelSize: 18 }
      Flickable {
        id: viewport
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: panel.width + 32
        contentHeight: panel.height + 32
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        MiniPanel {
          id: panel
          x: 16
          y: 16
          objectName: "candidatePreview"
          longSample: strip.longSample
          showComments: strip.showComments
          aiSettings: strip.previewAi ? ({
            enabled: app.ai.enabled, aiOnly: app.ai.aiOnly,
            filterPunctuation: app.ai.filterPunctuation, targetIndex: app.ai.targetIndex
          }) : null
          zoom: 1
        }
      }
      RowLayout {
        Layout.fillWidth: true
        Btn { text: "普通候选"; Layout.fillWidth: true; primary: !strip.longSample; onClicked: strip.longSample = false }
        Btn { text: "长句候选"; Layout.fillWidth: true; primary: strip.longSample; onClicked: strip.longSample = true }
      }
      Txt { text: "原始字号 · 超出区域可滚动查看"; color: Theme.ink2; font.pixelSize: Theme.hintSize }
    }
  }
  Txt { text: strip.previewAi ? "AI 示例用于预览样式与位置，不代表真实模型输出。" : "调整左侧设置，即可查看候选窗的变化。"; color: Theme.ink2; Layout.fillWidth: true; wrapMode: Text.Wrap }
}
