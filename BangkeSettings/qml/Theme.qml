pragma Singleton

import QtQuick

// 设置窗口使用不透明语义色，避免 Acrylic 与灰底重复合成导致色偏。
QtObject {
  id: theme

  property bool dark: false

  readonly property color ground: dark ? "#11151a" : "#eeeae3"
  readonly property color surface: dark ? "#20262e" : "#ffffff"
  readonly property color sidebarBg: dark ? "#161b21" : "#f1eee7"
  readonly property color field: dark ? "#171c23" : "#f6f3ed"

  readonly property color hair: dark ? Qt.rgba(96/255, 108/255, 124/255, 0.35)
                                    : Qt.rgba(145/255, 135/255, 119/255, 0.35)
  readonly property color hairSoft: dark ? Qt.rgba(96/255, 108/255, 124/255, 0.18)
                                        : Qt.rgba(145/255, 135/255, 119/255, 0.18)
  readonly property color ink: dark ? "#edf1f5" : "#302c26"
  readonly property color ink2: dark ? "#adb8c7" : "#6b6257"
  readonly property color ink3: dark ? "#adb8c7" : "#6b6257"

  readonly property color accent: dark ? "#94c8bc" : "#785330"
  readonly property color accentSoft: dark ? "#2b403e" : "#eee2d3"
  readonly property color accentInk: accent
  readonly property color accentText: dark ? "#102722" : "#ffffff"

  readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.35)
                                       : Qt.rgba(0, 0, 0, 0.10)
  readonly property color scrim: Qt.rgba(0, 0, 0, 0.4)

  readonly property int cardRadius: 12
  readonly property int fieldRadius: 8
  readonly property int pillRadius: 999

  // 标题、正文和辅助信息的字号层级
  readonly property real titleSize: 23
  readonly property real sectionSize: 16
  readonly property real bodySize: 14
  readonly property real subSize: 12
  readonly property real hintSize: 12

  readonly property int rowHeight: 64
  readonly property int controlHeight: 34

  // 西文 Segoe UI,中文回落微软雅黑(决策:不随包分发字体)
  readonly property string fontFamily: "Segoe UI"
  readonly property var fontFamilies: ["Segoe UI", "Microsoft YaHei UI"]

  // 配置里存的是 0xAARRGGBB 的整数,和 QML 的 color 之间只有这一处换算
  function argb(value, fallback) {
    if (value === undefined || value === null)
      return fallback
    var n = Number(value)
    if (isNaN(n))
      return fallback
    n = n >>> 0
    return Qt.rgba(((n >>> 16) & 0xFF) / 255, ((n >>> 8) & 0xFF) / 255,
                   (n & 0xFF) / 255, ((n >>> 24) & 0xFF) / 255)
  }

  function toArgb(color) {
    var a = Math.round(color.a * 255)
    var r = Math.round(color.r * 255)
    var g = Math.round(color.g * 255)
    var b = Math.round(color.b * 255)
    return (((a << 24) | (r << 16) | (g << 8) | b) >>> 0)
  }
}
