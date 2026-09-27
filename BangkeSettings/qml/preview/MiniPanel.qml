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
  // 背板灰(浅/深半区):半透明时把「背板→面板→高亮」的叠层直接合成
  // 为单层不透明色 —— 半透明悬浮会露出叠层边缘(用户实验:alpha<1 时
  // 显形),mac 的真实 NSPanel 合成无此痕迹
  property color boardColor: "#C0C0C0"

  function blend(fg, bg, a) {
    return Qt.rgba(fg.r * a + bg.r * (1 - a),
                   fg.g * a + bg.g * (1 - a),
                   fg.b * a + bg.b * (1 - a), 1)
  }  // metrics map 由父级传入(非属性绑定,安全)

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
    // 单行横排限 2 个:5 词横排过宽,预览 zoom 被压到字号看不清
    if (m.horizontal === true)
      return Math.min(2, candPool.length)
    return Math.max(1, Math.min(candPool.length, ps))
  }
  readonly property var cands: candPool.slice(0, visibleCount)

  function tint(key, fallback) {
    var c = Theme.argb(colors ? colors[key] : undefined, fallback)
    // 整窗透明(mac alphaValue):alpha < 1 时不做半透明悬浮,而是与
    // 背板/面板合成后的不透明色 —— 视觉即 ULW 合成的最终观感,
    // 且无叠层痕迹
    if (m.alpha !== undefined && m.alpha < 1) {
      const panel = Theme.argb(colors ? colors.back_color : undefined,
                               "#FFFFFF")
      const panelFlat = blend(panel, boardColor, m.alpha)
      if (key === "back_color")
        return panelFlat
      if (key === "hilited_candidate_back_color" ||
          key === "hilited_back_color")
        return blend(c, panelFlat, m.alpha)
      // 文字/序号/注释:与各自底色合成
      const under = key.indexOf("hilited") === 0 ? panelFlat : panelFlat
      return blend(c, under, m.alpha)
    }
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
  readonly property real marginX: Math.max(
      m.marginX !== undefined ? m.marginX : 9, panelR - 2)
  readonly property real marginY: m.marginY !== undefined ? m.marginY : 9
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


  Rectangle {
    id: panelFace
    anchors.fill: parent
    radius: zz(panelR)
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
  readonly property real hilitePadY: m.hilitePaddingY !== undefined ? m.hilitePaddingY : 5
  // 高亮半径分态:贴面板顶/底(无拼音行、首末行)才用面板圆角(snap 语义);
  // 拼音行下方的第一候选不贴边 → 用 round_corner(出厂 0=直角,mac 同观感)
  readonly property real hiliteR: (preeditH > 0)
      ? (m.hiliteRadius !== undefined ? m.hiliteRadius : 0)
      : Math.max(m.hiliteRadius !== undefined ? m.hiliteRadius : 0,
                 panelR)
  readonly property color hiliteColor: mini.tint("hilited_candidate_back_color", "#FFB3D7FF")
  // 面板圆角全程跟配置(滑杆全程生效);边距随圆角放大(mac edgeInset
  // = corner+border 语义),序号永远在弧线之后
  readonly property real panelR: m.cornerRadius !== undefined ? m.cornerRadius : 9

  Rectangle {
    id: hilite
    y: zz(mini.preeditH + (mini.preeditH > 0 ? mini.spacing : 0) + mini.marginY) - zz(mini.hilitePadY)
    // 多行:整行满铺;单行(mac 同观感):只包住第一个候选词,左右留
    // margin 边距 —— 引擎 0.4.22+ 已同步(删 HorizontalLayout 拉右)
    x: mini.horizontal ? zz(mini.marginX) : 0
    width: mini.horizontal ? zz(mini.rowWidth(0)) : parent.width
    // 与引擎同步:多行首行向下补到下一行文字顶(行距-已扩 padding),
    // 高亮块与下一行零空隙且不盖字;单行不扩
    height: zz(mini.rowH + 2 * mini.hilitePadY) +
            (mini.horizontal || mini.cands.length < 2
                 ? 0 : zz(mini.candSpacing - mini.hilitePadY))
    radius: zz(mini.hiliteR)
    color: mini.hiliteColor
  }
  // 下沿两角补直(仅多行):多行高亮贴面板顶,上沿用面板圆角、下沿用
  // round_corner(默认 0 直角)。单行高亮是独立小块,四角全圆(mac 同观感)
  Rectangle {
    visible: !mini.horizontal
    x: 0
    y: hilite.y + hilite.height - zz(mini.hiliteR)
    width: zz(mini.hiliteR)
    height: zz(mini.hiliteR)
    color: mini.hiliteColor
  }
  Rectangle {
    visible: !mini.horizontal
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
