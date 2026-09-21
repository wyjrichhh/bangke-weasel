import QtQuick
import Bangke.Pages

// 一张卡片里的一段行(styleControls 按 section 切片)。
// 行顶的发丝线靠列内的 y:定位器把第一个可见行放在 0,它就不画线 —— 不必知道自己是第几行。
Column {
  id: group
  property var source: null
  property string section: ""

  spacing: 0

  Repeater {
    model: group.source

    delegate: StyleRow {
      width: group.width
      visible: model.section === group.section
      showDivider: y > 0
    }
  }
}
