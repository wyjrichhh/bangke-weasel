pragma Singleton

import QtQuick

// 设置窗口使用不透明语义色，避免 Acrylic 与灰底重复合成导致色偏。
QtObject {
  id: theme

  property bool dark: false

  readonly property color ground: dark ? "#101010" : "#FFFFFF"
  readonly property color surface: dark ? "#1C1C1C" : "#FFFFFF"
  readonly property color sidebarBg: dark ? "#0C0C0C" : "#F2F5F7"
  readonly property color field: dark ? "#171717" : "#FFFFFF"

  readonly property color hair: dark ? Qt.rgba(1, 1, 1, 0.10)
                                      : Qt.rgba(0, 0, 0, 0.10)
  readonly property color hairSoft: dark ? Qt.rgba(1, 1, 1, 0.05)
                                          : Qt.rgba(0, 0, 0, 0.05)
  readonly property color ink: dark ? "#F2F6F8" : "#111827"
  readonly property color ink2: dark ? "#AFBEC8" : "#364152"
  readonly property color ink3: dark ? "#8B9EAA" : "#52616B"

  readonly property color accent: dark ? "#43D5BA" : "#087F70"
  readonly property color accentSoft: dark ? "#203C3B" : "#D9EEE9"
  readonly property color accentInk: accent
  readonly property color accentText: dark ? "#10221F" : "#FFFFFF"

  readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.35)
                                       : Qt.rgba(0, 0, 0, 0.10)
  readonly property color scrim: Qt.rgba(0, 0, 0, 0.4)

  readonly property int cardRadius: 9
  readonly property int fieldRadius: 8
  readonly property int pillRadius: 999

  // 标题、正文和辅助信息的字号层级
  readonly property real titleSize: 23
  readonly property real sectionSize: 16
  readonly property real bodySize: 14
  readonly property real subSize: 12
  readonly property real hintSize: 12

  readonly property int rowHeight: 44
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
