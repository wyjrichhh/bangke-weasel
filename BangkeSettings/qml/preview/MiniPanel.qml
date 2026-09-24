import QtQuick
import Bangke.Pages

// 单块候选窗预览:配色/圆角/间距/字号/横竖排/字体全部取真实配置
// (app.style.previewLight|Dark + previewMetrics),按 mac 预览同一组样例
// 候选画。竖排(text_orientation)在 Windows 引擎上本就回落堆叠,这里同画法。
//
// 缩放:不用 Item.scale 整体缩(文本会被压糊),而是 zoom 因子乘进字号与
// 度量 —— 文字永远按真实字号光栅化。unitW/unitH 是 zoom=1 的裸尺寸,
// 容器(Half)用它算出 zoom 再传回来。
// 宽度/行高用每字段一枚 TextMetrics 声明式测出 —— 共享探针 + 函数内改属性
// 会把测量缠进依赖环(binding loop),这是 0.4.0 的教训。
Item {
  id: mini

  property var colors: ({})
  property var m: ({})
  property real zoom: 1
  property bool horizontal: m.horizontal === true

  // 样例池:首条高亮并带注释;显示条数 = 每页候选数(与实际一致)
  readonly property var candPool: [
    { label: "1.", text: "蚌壳", comment: "bàng ké" },
    { label: "2.", text: "帮客", comment: "" },
    { label: "3.", text: "绑客", comment: "" },
    { label: "4.", text: "榜课", comment: "" },
    { label: "5.", text: "磅科", comment: "" },
    { label: "6.", text: "傍可", comment: "" },
    { label: "7.", text: "谤克", comment: "" },
    { label: "8.", text: "塝棵", comment: "" }
  ]
  readonly property int visibleCount: {
    var ps = m.pageSize !== undefined ? m.pageSize : 5
    return Math.max(1, Math.min(candPool.length, ps))
  }
  readonly property var cands: candPool.slice(0, visibleCount)

  function tint(key, fallback) {
    var c = Theme.argb(colors ? colors[key] : undefined, fallback)
    if (key === "back_color" && m.alpha !== undefined)
      c = Qt.rgba(c.r, c.g, c.b, c.a * m.alpha)
    return c
  }

  // unit 值(未乘 zoom);pointSize 必须 > 0:0 表示「隐藏」,量宽时钳到 1
  function pt(v) {
    return v > 0 ? v : 1
  }
  function zz(v) {
    return v * mini.zoom
  }

  readonly property real fontPoint: m.fontPoint !== undefined ? m.fontPoint : 16
  readonly property real labelPoint: m.labelPoint !== undefined ? m.labelPoint : fontPoint
  readonly property real commentPoint: m.commentPoint !== undefined ? m.commentPoint : fontPoint
  readonly property real marginX: m.marginX !== undefined ? m.marginX : 5
  readonly property real marginY: m.marginY !== undefined ? m.marginY : 5
  readonly property real candSpacing: m.candidateSpacing !== undefined ? m.candidateSpacing : 5
  readonly property real spacing: m.spacing !== undefined ? m.spacing : 8
  readonly property real hiliteSpacing: m.hiliteSpacing !== undefined ? m.hiliteSpacing : 5
  readonly property bool commentVisible: commentPoint > 0
  // 配置的候选窗字体(fallback 到界面字体);权重后缀按名字映射
  readonly property string family: m.fontFamily !== undefined && m.fontFamily.length > 0
                                   ? m.fontFamily : Theme.fontFamily
  readonly property int weight: {
    var w = m.fontWeight !== undefined ? m.fontWeight : ""
    if (w === "light" || w === "thin")
      return Font.Light
    if (w === "medium" || w === "demi_bold" || w === "semi_bold")
      return Font.DemiBold
    if (w === "bold" || w === "extra_bold" || w === "ultra_bold" ||
        w === "black")
      return Font.Bold
    return Font.Normal
  }

  component Metrics: TextMetrics {
    font.family: mini.family
    font.weight: mini.weight
  }

  Metrics { id: mPre; text: "bang'ke"; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m0l; text: mini.cands[0].label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m0t; text: mini.cands[0].text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m0c; text: mini.cands[0].comment; font.pointSize: mini.pt(mini.commentPoint) }
  Metrics { id: m1l; text: mini.cands[1].label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m1t; text: mini.cands[1].text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m2l; text: mini.cands[2].label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m2t; text: mini.cands[2].text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m3l; text: mini.cands[3].label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m3t; text: mini.cands[3].text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m4l; text: mini.cands[4].label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m4t; text: mini.cands[4].text; font.pointSize: mini.pt(mini.fontPoint) }

  function lbl(i) {
    return [m0l, m1l, m2l, m3l, m4l][i]
  }
  function txt(i) {
    return [m0t, m1t, m2t, m3t, m4t][i]
  }

  // 行高 = 正文字体的行高(TextMetrics.height),unit 值
  readonly property real rowH: m0t.height

  // 行内布局: 序号 hiliteSpacing 候选 (hiliteSpacing 注释)
  function rowWidth(i) {
    var c = cands[i]
    var w = lbl(i).advanceWidth + hiliteSpacing + txt(i).advanceWidth
    if (c.comment.length > 0 && commentVisible)
      w += hiliteSpacing + m0c.advanceWidth
    return w
  }

  readonly property real preeditH: (m.inlinePreedit === false) ? rowH : 0
  readonly property real unitContentH: {
    var rowsH = horizontal ? rowH
                           : cands.length * rowH + (cands.length - 1) * candSpacing
    return preeditH + (preeditH > 0 ? spacing : 0) + rowsH
  }
  readonly property real unitContentW: {
    var w = 0
    if (horizontal) {
      for (var i = 0; i < cands.length; ++i)
        w += rowWidth(i) + (i > 0 ? candSpacing : 0)
    } else {
      for (var k = 0; k < cands.length; ++k)
        w = Math.max(w, rowWidth(k))
    }
    // 编码行(inline_preedit 关闭时)也要在面板内
    if (preeditH > 0)
      w = Math.max(w, marginX + mPre.advanceWidth)
    return w
  }
  // 给容器算 zoom 用的裸尺寸
  readonly property real unitW: Math.ceil(unitContentW + marginX * 2)
  readonly property real unitH: Math.ceil(unitContentH + marginY * 2)

  implicitWidth: Math.ceil(unitW * zoom)
  implicitHeight: Math.ceil(unitH * zoom)

  // 面板本体 + 两层同心递淡的叠影(不用 MultiEffect:shader 阴影在软件/
  // RDP 渲染下退化成实心轮廓块,即「双层面板」的根因;纯 Rectangle 可靠)
  Rectangle {
    anchors.fill: parent
    anchors.margins: -2
    anchors.bottomMargin: -3
    radius: zz((m.cornerRadius !== undefined ? m.cornerRadius : 9) + 2)
    color: Qt.rgba(0, 0, 0, 0.10)
  }
  Rectangle {
    anchors.fill: parent
    anchors.margins: -1
    anchors.bottomMargin: -2
    radius: zz((m.cornerRadius !== undefined ? m.cornerRadius : 9) + 1)
    color: Qt.rgba(0, 0, 0, 0.08)
  }
  Rectangle {
    id: panelFace
    anchors.fill: parent
    radius: zz(m.cornerRadius !== undefined ? m.cornerRadius : 9)
    color: mini.tint("back_color", "#FFFFFF")
    // 引擎只在 border_width > 0 时画边框(border_color 不透明),
    // 预览同判 —— 不额外加描边
    border.width: m.borderWidth !== undefined && m.borderWidth > 0 ? 1 : 0
    border.color: mini.tint("border_color", "#FF808080")
  }

  // 高亮块:首行整块(引擎 margin_x=0+padding_x 外扩 → 左右满铺贴边),
  // 纵向 = 行高 + 上下各外扩 hilite_padding_y。圆角逻辑同引擎
  // (_SnapHighlightToPanel):贴面板顶的那沿用面板半径(首行=上沿圆),
  // 另一沿用 round_corner(默认 0=直角)→ 首行高亮「上圆下直」
  readonly property real hilitePadY: m.hilitePaddingY !== undefined ? m.hilitePaddingY : 2
  readonly property real hiliteR: Math.max(m.hiliteRadius !== undefined ? m.hiliteRadius : 0,
                                           m.cornerRadius !== undefined ? m.cornerRadius : 9)
  readonly property color hiliteColor: mini.tint("hilited_candidate_back_color", "#FFB3D7FF")

  Rectangle {
    id: hilite
    y: zz(mini.preeditH + (mini.preeditH > 0 ? mini.spacing : 0) + mini.marginY) - zz(mini.hilitePadY)
    x: 0
    width: parent.width
    height: zz(mini.rowH + 2 * mini.hilitePadY)
    radius: zz(mini.hiliteR)
    color: mini.hiliteColor
  }
  // 下沿两角补直:高亮 Rectangle 的 radius 服务上沿(面板圆角),
  // 下沿应表现 round_corner(默认 0 直角)
  Rectangle {
    x: 0
    y: hilite.y + hilite.height - zz(mini.hiliteR)
    width: zz(mini.hiliteR)
    height: zz(mini.hiliteR)
    color: mini.hiliteColor
  }
  Rectangle {
    x: parent.width - zz(mini.hiliteR)
    y: hilite.y + hilite.height - zz(mini.hiliteR)
    width: zz(mini.hiliteR)
    height: zz(mini.hiliteR)
    color: mini.hiliteColor
  }
  // 编码行(inline_preedit 关闭时才有)
  Text {
    visible: mini.preeditH > 0
    x: zz(mini.marginX)
    y: zz(mini.marginY)
    height: zz(mini.rowH)
    verticalAlignment: Text.AlignVCenter
    text: "bang'ke"
    color: mini.tint("text_color", "#FF000000")
    font.family: mini.family
    font.weight: mini.weight
    font.pointSize: Math.max(1, mini.pt(mini.fontPoint) * mini.zoom)
  }

  // 候选行(横排 = 一行五块,首块坐高亮;竖排 = 每行一条)
  Repeater {
    model: mini.cands.length

    delegate: Item {
      id: candRow
      required property int index
      readonly property var c: mini.cands[candRow.index]
      readonly property bool first: candRow.index === 0
      readonly property real rowsTopUnit: mini.marginY + mini.preeditH + (mini.preeditH > 0 ? mini.spacing : 0)
      x: mini.horizontal ? zz(mini.marginX + (candRow.index > 0 ? precedingUnit() : 0)) : zz(mini.marginX)
      y: mini.horizontal ? zz(rowsTopUnit)
                         : zz(rowsTopUnit + candRow.index * (mini.rowH + mini.candSpacing))
      width: zz(mini.rowWidth(candRow.index))
      height: zz(mini.rowH)

      function precedingUnit() {
        var w = 0
        for (var k = 0; k < candRow.index; ++k)
          w += mini.rowWidth(k) + mini.candSpacing
        return w
      }

      Text {
        x: 0
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        text: candRow.c.label
        visible: mini.labelPoint > 0
        color: candRow.first ? mini.tint("hilited_label_color", "#FF000000")
                             : mini.tint("label_color", "#FF000000")
        font.family: mini.family
        font.weight: mini.weight
        font.pointSize: Math.max(1, mini.pt(mini.labelPoint) * mini.zoom)
      }

      Text {
        x: zz(mini.lbl(candRow.index).advanceWidth + mini.hiliteSpacing)
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        text: candRow.c.text
        color: candRow.first ? mini.tint("hilited_candidate_text_color", "#FF000000")
                             : mini.tint("candidate_text_color", "#FF000000")
        font.family: mini.family
        font.weight: mini.weight
        font.pointSize: Math.max(1, mini.pt(mini.fontPoint) * mini.zoom)
      }

      Text {
        x: zz(mini.lbl(candRow.index).advanceWidth + mini.hiliteSpacing
              + mini.txt(candRow.index).advanceWidth + mini.hiliteSpacing)
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        text: candRow.c.comment
        visible: candRow.c.comment.length > 0 && mini.commentVisible
        color: candRow.first ? mini.tint("hilited_comment_color", "#FF808080")
                             : mini.tint("comment_text_color", "#FF808080")
        font.family: mini.family
        font.weight: mini.weight
        font.pointSize: Math.max(1, mini.pt(mini.commentPoint) * mini.zoom)
      }
    }
  }
}
