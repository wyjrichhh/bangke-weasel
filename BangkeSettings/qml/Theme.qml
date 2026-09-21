pragma Singleton

import QtQuick

// 观感 token 的唯一来源:提案稿里 --ui-l / --ui-d 两张表原样搬过来。
// dark 由 Main.qml 用 Binding 跟 sysTheme.dark 同步,单例不去碰注入的上下文属性。
QtObject {
  id: theme

  property bool dark: false

  readonly property color ground: dark ? "#141719" : "#F7F8F8"
  readonly property color surface: dark ? "#1C2022" : "#FFFFFF"
  readonly property color field: dark ? "#22282A" : "#FFFFFF"
  readonly property color hair: dark ? "#2A3032" : "#EBEEEF"
  readonly property color hairSoft: dark ? "#23282A" : "#F1F4F4"
  readonly property color ink: dark ? "#E8ECEC" : "#14181A"
  readonly property color ink2: dark ? "#A2ACAE" : "#5C6668"
  readonly property color ink3: dark ? "#7C8688" : "#8A9496"
  readonly property color accent: dark ? "#2DD4BF" : "#0E9384"
  readonly property color accentSoft: dark ? "#123B37" : "#E4F3F1"
  readonly property color accentInk: dark ? "#5EEAD4" : "#0A6A60"
  // 深色下主色太亮,按钮上的字得反过来用深色才够对比
  readonly property color onAccent: dark ? "#08201C" : "#FFFFFF"
  readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.8)
                                       : Qt.rgba(16 / 255, 24 / 255, 26 / 255, 0.28)
  readonly property color scrim: Qt.rgba(0, 0, 0, 0.4)

  readonly property int cardRadius: 13
  readonly property int fieldRadius: 9
  readonly property int pillRadius: 999

  // 提案里的 12.5 / 11.5 落到 font.pixelSize(int) 上由 Qt 截断,token 表仍与稿子一一对应
  readonly property real titleSize: 20
  readonly property real sectionSize: 16
  readonly property real bodySize: 13
  readonly property real subSize: 12.5
  readonly property real hintSize: 11.5

  readonly property int rowHeight: 46
  readonly property int controlHeight: 34

  readonly property string fontFamily: "Microsoft YaHei UI"

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
