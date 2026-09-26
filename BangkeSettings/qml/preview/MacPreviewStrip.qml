import QtQuick
import QtQuick.Layouts
import Bangke.Pages

// 外观页顶部的浅/深双面板预览条(mac AppearanceSettingsPage):
// 固定高 280、背板灰 C0C0C0/696969 圆角 10、直线分界,左上角「浅色/深色」
// 胶囊角标;单行横条上下切分(各得全宽),多行面板左右切分(判据同 mac
// isWidePanel = 浅色面板宽 > 高)。两块各自按内容测量等比缩放。
// 面板必须是半区的【子项】才是场景的一部分 —— 上一版把它写成属性值
// 声明(无视觉 parent),预览因此只剩背板。
ColumnLayout {
  id: strip
  spacing: 10

  Txt {
    text: "预览"
    color: Theme.ink2
    font.pixelSize: Theme.subSize
    Layout.fillWidth: true
  }

  component ModePill: Rectangle {
    id: pill
    required property string text
    height: 16
    width: label.implicitWidth + 14
    radius: height / 2
    color: Qt.rgba(0.45, 0.45, 0.45, 0.6)

    Txt {
      id: label
      anchors.centerIn: parent
      text: pill.text
      color: "#FFFFFF"
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
      color: half.dark ? "#696969" : "#C0C0C0"
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
      zoom: Math.min(z1, z2)
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
    Layout.preferredHeight: 280
    radius: 10
    clip: true

    // 切分方向直接跟随「候选排列」(mac 的测量判据本质就是区分单行/多行;
    // 用配置值零测量误差):单行横条上下切分(各得全宽),多行面板左右切分
    readonly property bool singleLine:
        app.style.previewMetrics.horizontal === true
    readonly property real gap: 1

    Half { id: lightHalf; dark: false }
    Half { id: darkHalf; dark: true }

    // 背板是【一整块】圆角矩形:浅深两半无缝拼接(直线分界),分界线只是
    // 1px 线,不是两个独立块(mac SplitBackdrop 同构)。面板间距由半区内
    // 的呼吸位(zoom 的 56)体现
    states: [
      State {
        name: "stackedPanels"
        when: !frame.singleLine
        PropertyChanges {
          target: lightHalf
          x: 0
          y: 0
          width: frame.width / 2
          height: frame.height
        }
        PropertyChanges {
          target: darkHalf
          x: frame.width / 2
          y: 0
          width: frame.width / 2
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
          height: frame.height / 2
        }
        PropertyChanges {
          target: darkHalf
          x: 0
          y: frame.height / 2
          width: frame.width
          height: frame.height / 2
        }
      }
    ]

    // 中线:与切分方向垂直 —— 单行(上下切分)画横线,多行(左右切分)
    // 画竖线。曾误绑已被改名的 frame.wide,恒 falsy 导致单行时残留竖线
    Rectangle {
      width: frame.singleLine ? parent.width : 1
      height: frame.singleLine ? 1 : parent.height
      anchors.centerIn: parent
      color: Qt.rgba(0.5, 0.5, 0.5, 0.5)
    }
  }
}
