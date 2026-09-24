pragma Singleton

import QtQuick

// 观感 token 的唯一来源,与 macOS 版设置窗一致:
// 语义色 2026-09 在 macOS 26 上实测(labelColor 0.85 / secondaryLabel 0.50 /
// separator 0.10 / controlAccentColor #007AFF),窗口底色取 macOS 分组表单的
// 系统灰阶。dark 由 Main.qml 用 Binding 跟 sysTheme.dark 同步。
QtObject {
  id: theme

  property bool dark: false

  // 画布与侧栏带 alpha:窗口本体已开 acrylic(WinChrome),半透明底色
  // 透出模糊层 = mac 侧栏 vibrancy 的近似;acrylic 被禁用的环境(RDP)
  // 窗口仍按半透明合成,「透」保得住,「磨砂」只在支持的环境出现。
  // 层次约定:侧栏最深且最透 → 画布居中 → 卡片不透明
  readonly property color ground: dark ? Qt.rgba(0.15, 0.15, 0.15, 0.88)
                                       : Qt.rgba(0.945, 0.945, 0.945, 0.88)
  readonly property color surface: dark ? "#1E1E1E" : "#FFFFFF"
  readonly property color sidebarBg: dark ? Qt.rgba(0.10, 0.10, 0.10, 0.58)
                                          : Qt.rgba(0.914, 0.914, 0.914, 0.58)
  readonly property color field: dark ? "#1E1E1E" : "#FFFFFF"

  readonly property color hair: dark ? Qt.rgba(1, 1, 1, 0.10)
                                      : Qt.rgba(0, 0, 0, 0.10)
  readonly property color hairSoft: dark ? Qt.rgba(1, 1, 1, 0.05)
                                          : Qt.rgba(0, 0, 0, 0.05)
  readonly property color ink: dark ? Qt.rgba(1, 1, 1, 0.85)
                                     : Qt.rgba(0, 0, 0, 0.85)
  readonly property color ink2: dark ? Qt.rgba(1, 1, 1, 0.55)
                                      : Qt.rgba(0, 0, 0, 0.50)
  readonly property color ink3: dark ? Qt.rgba(1, 1, 1, 0.38)
                                      : Qt.rgba(0, 0, 0, 0.38)

  readonly property color accent: "#007AFF"  // controlAccentColor,明暗同值
  readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, 0.22)
  readonly property color accentInk: accent
  readonly property color accentText: "#FFFFFF"

  readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.35)
                                       : Qt.rgba(0, 0, 0, 0.10)
  readonly property color scrim: Qt.rgba(0, 0, 0, 0.4)

  readonly property int cardRadius: 10
  readonly property int fieldRadius: 7
  readonly property int pillRadius: 999

  // .title2 / 正文 / caption / caption2 的近似像素值
  readonly property real titleSize: 22
  readonly property real sectionSize: 16
  readonly property real bodySize: 13
  readonly property real subSize: 12
  readonly property real hintSize: 11

  readonly property int rowHeight: 34
  readonly property int controlHeight: 28

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
