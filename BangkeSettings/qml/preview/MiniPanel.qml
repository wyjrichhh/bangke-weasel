import QtQuick
import Bangke.Pages

// 单块候选窗预览:配色/圆角/间距/字号/横竖排/字体全部取真实配置
// (app.style.previewLight|Dark + previewMetrics),按 mac 预览同一组样例
// 候选画。竖排(text_orientation)在 Windows 引擎上本就回落堆叠,这里同画法。
// 单行/多行都先占一行放编码(inline_preedit 关闭时),候选列表在下面 ——
// 单行的区别只是候选彼此横排,不是编码与候选挤同一行。
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
  property bool longSample: false
  property real zoom: 1
  property bool horizontal: m.horizontal === true
  // 与 macOS 和 Windows 实际候选窗一致：合成后统一应用整窗透明度。
  layer.enabled: opacity < 1
  opacity: Math.max(0, Math.min(1, m.alpha !== undefined ? m.alpha : 1))

  // 样例池:首条高亮并带注释;显示条数 = 每页候选数(与实际一致)。
  // 单行横排也曾限成 2 个(怕 zoom 把字号压小),但 mac 预览是照实画满
  // 一页的 —— 条数不一致比字号更刺眼,改回跟随每页候选数。
  readonly property var candPool: longSample ? [
    { label: "1.", text: "让每一次输入都恰到好处", comment: "AI" },
    { label: "2.", text: "今天的表达更自然一些", comment: "" },
    { label: "3.", text: "把想法写下来", comment: "" }
  ] : [
    { label: "1.", text: "蚌壳", comment: "bàng ké" },
    { label: "2.", text: "帮客", comment: "" },
    { label: "3.", text: "绑客", comment: "" },
    { label: "4.", text: "榜课", comment: "" },
    { label: "5.", text: "磅科", comment: "" },
    { label: "6.", text: "傍可", comment: "" },
    { label: "7.", text: "谤克", comment: "" },
    { label: "8.", text: "塝棵", comment: "" },
    { label: "9.", text: "螃壳", comment: "" }
  ]
  readonly property int visibleCount: {
    var ps = m.pageSize !== undefined ? m.pageSize : 5
    return Math.max(1, Math.min(candPool.length, ps))
  }
  readonly property var cands: candPool.slice(0, visibleCount)

  function tint(key, fallback) {
    var c = Theme.argb(colors ? colors[key] : undefined, fallback)
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
  Metrics { id: m0l; text: mini.candAt(0).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m0t; text: mini.candAt(0).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m0c; text: mini.candAt(0).comment; font.pointSize: mini.pt(mini.commentPoint) }
  Metrics { id: m1l; text: mini.candAt(1).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m1t; text: mini.candAt(1).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m2l; text: mini.candAt(2).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m2t; text: mini.candAt(2).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m3l; text: mini.candAt(3).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m3t; text: mini.candAt(3).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m4l; text: mini.candAt(4).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m4t; text: mini.candAt(4).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m5l; text: mini.candAt(5).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m5t; text: mini.candAt(5).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m6l; text: mini.candAt(6).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m6t; text: mini.candAt(6).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m7l; text: mini.candAt(7).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m7t; text: mini.candAt(7).text; font.pointSize: mini.pt(mini.fontPoint) }
  Metrics { id: m8l; text: mini.candAt(8).label; font.pointSize: mini.pt(mini.labelPoint) }
  Metrics { id: m8t; text: mini.candAt(8).text; font.pointSize: mini.pt(mini.fontPoint) }

  // 样例池按「每页候选数」切片,探针却按池子的全长声明 —— 条数少于池子时
  // 后面的下标取不到值,直接下标会报 "Cannot read property 'label' of
  // undefined"(每页 1~4 个候选时 m4 就已经越界了)
  function candAt(i) {
    return i < cands.length ? cands[i]
                            : { label: "", text: "", comment: "" }
  }
  function lbl(i) {
    return [m0l, m1l, m2l, m3l, m4l, m5l, m6l, m7l, m8l][i]
  }
  function txt(i) {
    return [m0t, m1t, m2t, m3t, m4t, m5t, m6t, m7t, m8t][i]
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

  // 编码独占最上面一行 —— 单行/多行都一样(引擎 HorizontalLayout 与
  // VerticalLayout 都是先放编码、再放候选)。单行只是「候选自己排成一行」,
  // 编码不与候选挤在同一行,所以有编码行时候选列表是第二行。
  readonly property bool showsPreedit: m.inlinePreedit === false
  readonly property real preeditH: showsPreedit ? rowH : 0
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
    // 编码行也要在面板内
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
    objectName: "candidateBackground"
    anchors.fill: parent
    radius: zz(panelR)
    color: mini.tint("back_color", "#FFFFFF")
    // 引擎只在 border_width > 0 时画边框(border_color 不透明),
    // 预览同判 —— 不额外加描边
    // 恒画 1px:Qt 的 Rectangle 边框是往内画的,按配置宽度(可能 9px)画会让
    // 可见面缩进去,而高亮按引擎语义只让 border/2,两者对不上就露边。
    // 引擎那边描边是以路径为中心画的,这里用 1px 近似即可
    border.width: m.borderWidth !== undefined && m.borderWidth > 0 ? 1 : 0
    border.color: mini.tint("border_color", "#FF808080")
  }

  // 高亮块:引擎里 = 首候选的文字矩形,四边各外扩 hilite_padding ——
  // 左缘因此顶到面板左边(margin_x 与 padding_x 相等时正好为 0),右缘
  // 超出文字一个 padding,纵向 = 行高 + 上下各一个 padding
  readonly property real hilitePadX: m.hilitePaddingX !== undefined ? m.hilitePaddingX : 5
  readonly property real hilitePadY: m.hilitePaddingY !== undefined ? m.hilitePaddingY : 5
  readonly property color hiliteColor: mini.tint("hilited_candidate_back_color", "#FFB3D7FF")
  // 面板圆角全程跟配置(滑杆全程生效);边距随圆角放大(mac edgeInset
  // = corner+border 语义),序号永远在弧线之后
  readonly property real panelR: m.cornerRadius !== undefined ? m.cornerRadius : 9

  readonly property real hiliteTop: zz(mini.preeditH + (mini.preeditH > 0 ? mini.spacing : 0)
                                       + mini.marginY) - zz(mini.hilitePadY)
  // 描边以面板轮廓为中心画,内侧半个线宽压在内容区上 —— 高亮贴边时让它停在
  // 描边内沿,否则会盖住上下边框(引擎同规则:_SnapHighlightToPanel 让 border/2)
  readonly property real hiliteInset:
      (m.borderWidth !== undefined && m.borderWidth > 0) ? m.borderWidth / 2 : 0
  readonly property real hiliteInsetTop: mini.atTop ? zz(mini.hiliteInset) : 0
  readonly property real hiliteInsetBottom: mini.atBottom ? zz(mini.hiliteInset) : 0
  readonly property real hiliteInsetLeft: mini.atLeft ? zz(mini.hiliteInset) : 0
  readonly property real hiliteInsetRight: mini.atRight ? zz(mini.hiliteInset) : 0
  // 圆角规则:一个角只有当「它的两条边都贴着面板边」时才圆 —— 那种角与面板
  // 的弧相邻,必须同心同半径;只贴一条边的角是直角。首候选的右缘后面还跟着
  // 下一个候选(不在面板右缘上),所以右上右下都是直角;左缘贴面板左边,左侧
  // 两角跟着面板圆角。四种组合与引擎圆角表一致:
  //   单行+有编码行 {直,圆,直,直}  单行+无编码行 {圆,圆,直,直}
  //   多行+有编码行 {直,直,直,直}  多行+无编码行 {圆,直,圆,直}
  // (顺序:左上, 左下, 右上, 右下)
  readonly property bool atTop: mini.preeditH <= 0
  readonly property bool atLeft: true
  readonly property bool atRight: mini.horizontal
      ? (hilite.x + hilite.width >= mini.width - 0.5) : true
  readonly property bool atBottom: mini.horizontal ? true : (mini.cands.length < 2)
  readonly property bool roundTopLeft: atTop && atLeft
  readonly property bool roundBottomLeft: atBottom && atLeft
  readonly property bool roundTopRight: atTop && atRight
  readonly property bool roundBottomRight: atBottom && atRight
  readonly property real hiliteR: (roundTopLeft || roundBottomLeft ||
                                   roundTopRight || roundBottomRight)
      ? mini.panelR : 0

  Rectangle {
    id: hilite
    objectName: "candidateHighlight"
    // 上沿 = 候选行顶减去纵向外扩(有编码行时就在编码行下方);贴面板上沿时再
    // 让出半个线宽给边框。最后两个边各自夹进面板内沿 —— mac 的 expand() 就是
    // 把高亮夹到 outerBox:padding_y 比 margin_y 大时,按 padding 外扩的高亮
    // 本来会顶到面板外面(实机与预览同一条规则)
    readonly property real topRaw: mini.hiliteTop + mini.hiliteInsetTop
    readonly property real bottomRaw: topRaw + zz(mini.rowH + 2 * mini.hilitePadY) +
            (mini.horizontal || mini.cands.length < 2
                 ? 0 : zz(mini.candSpacing - mini.hilitePadY))
    readonly property real topEdge: Math.max(mini.hiliteInsetTop, topRaw)
    readonly property real bottomEdge: Math.min(parent.height - mini.hiliteInsetBottom,
                                                bottomRaw)
    y: topEdge
    // 左缘贴面板左边:引擎 candidateRect.left - hilite_padding_x = 0,贴边时
    // 同样让出半个线宽给边框
    x: mini.hiliteInsetLeft
    // 多行整行满铺;单行只包住第一个候选词,右缘再多一个 padding
    width: (mini.horizontal ? zz(mini.marginX + mini.rowWidth(0) + mini.hilitePadX)
                            : parent.width)
           - mini.hiliteInsetLeft - mini.hiliteInsetRight
    // 至少留住两个 padding 的高度,别被夹没
    height: Math.max(zz(2 * mini.hilitePadY), bottomEdge - topEdge)
    radius: zz(mini.hiliteR)
    color: mini.hiliteColor
  }
  // 补直:半径 > 0 却有角不该圆时,用高亮色的小方块把那个角补回直角
  Rectangle {
    visible: hilite.radius > 0 && !mini.roundTopLeft
    x: hilite.x
    y: hilite.y
    width: hilite.radius
    height: hilite.radius
    color: mini.hiliteColor
  }
  Rectangle {
    visible: hilite.radius > 0 && !mini.roundTopRight
    x: hilite.x + hilite.width - hilite.radius
    y: hilite.y
    width: hilite.radius
    height: hilite.radius
    color: mini.hiliteColor
  }
  Rectangle {
    visible: hilite.radius > 0 && !mini.roundBottomLeft
    x: hilite.x
    y: hilite.y + hilite.height - hilite.radius
    width: hilite.radius
    height: hilite.radius
    color: mini.hiliteColor
  }
  Rectangle {
    visible: hilite.radius > 0 && !mini.roundBottomRight
    x: hilite.x + hilite.width - hilite.radius
    y: hilite.y + hilite.height - hilite.radius
    width: hilite.radius
    height: hilite.radius
    color: mini.hiliteColor
  }
  // 编码(inline_preedit 关闭时才有):单行多行都独占最上面一行
  Text {
    renderType: Text.NativeRendering
    visible: mini.showsPreedit
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
      x: mini.horizontal ? zz(mini.marginX + (candRow.index > 0 ? precedingUnit() : 0))
                         : zz(mini.marginX)
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
        renderType: Text.NativeRendering
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
        renderType: Text.NativeRendering
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
        renderType: Text.NativeRendering
        x: zz(mini.lbl(candRow.index).advanceWidth + mini.hiliteSpacing
              + mini.txt(candRow.index).advanceWidth + mini.hiliteSpacing)
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        text: candRow.c.comment
        visible: candRow.c.comment.length > 0 && mini.commentVisible
        color: candRow.first ? mini.tint("hilited_comment_text_color", "#FF808080")
                             : mini.tint("comment_text_color", "#FF808080")
        font.family: mini.family
        font.weight: mini.weight
        font.pointSize: Math.max(1, mini.pt(mini.commentPoint) * mini.zoom)
      }
    }
  }
}
