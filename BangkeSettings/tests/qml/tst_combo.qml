import QtQuick
import QtTest
import Bangke.Pages

TestCase {
  id: test
  name: "SettingsComponents"
  visible: true
  width: 500
  height: 500
  when: windowShown

  ComboRow {
    id: combo
    x: 20; y: 20; width: 400; height: 44
    label: "Options"
    options: ["One", "Two", "Three"]
    value: "One"
    onEdited: function (value) { combo.value = value }
  }

  Sidebar { id: sidebar; x: 450; y: 100; width: 194; height: 360 }

  Rectangle {
    id: board
    x: 20; y: 100; width: 200; height: 180
    color: "#0000FF"
    MiniPanel {
      id: preview
      x: 20; y: 20
      colors: ({back_color: 0xFFFFFFFF, hilited_candidate_back_color: 0xFFFF0000})
      m: ({alpha: 1, pageSize: 1, fontPoint: 16, labelPoint: 0, commentPoint: 0,
           marginX: 20, marginY: 20, cornerRadius: 0, borderWidth: 0,
           hilitePaddingX: 0, hilitePaddingY: 0, candidateSpacing: 0})
    }
  }

  function test_fullPageAndAiExamplesFollowDrafts() {
    const original = preview.m
    preview.m = Object.assign({}, original, {pageSize: 20, horizontal: false})
    compare(preview.cands.length, 20)
    const fullHeight = preview.unitH
    const dictionaryWidth = preview.unitW
    preview.aiSettings = {enabled: true, aiOnly: true, filterPunctuation: true, targetIndex: 0}
    compare(preview.cands.length, 1)
    compare(preview.cands[0].text, "让表达更自然")
    verify(preview.unitH < fullHeight)
    verify(preview.unitW > dictionaryWidth)
    const cleanWidth = preview.unitW
    preview.aiSettings = {enabled: true, aiOnly: true, filterPunctuation: false, targetIndex: 0}
    compare(preview.cands[0].text, "，让表达更自然。")
    verify(preview.unitW > cleanWidth)
    preview.aiSettings = {enabled: true, aiOnly: false, filterPunctuation: true, targetIndex: 2}
    compare(preview.cands[2].text, "让表达更自然")
    compare(preview.cands[2].label, "3.")
    preview.aiSettings = null
    preview.m = original
  }

  function test_previewOpacityIncludesHighlight() {
    const x = preview.x + 2
    const backY = preview.y + 2
    const hiliteY = Math.floor(preview.y + preview.hiliteTop + preview.rowH / 2)
    preview.m = Object.assign({}, preview.m, {alpha: 0.5})
    waitForRendering(board)
    tryVerify(function () {
      const image = grabImage(board)
      return Math.abs(image.red(x, backY) - 128) <= 2 &&
             Math.abs(image.green(x, backY) - 128) <= 2 &&
             image.blue(x, backY) >= 253 &&
             Math.abs(image.red(x, hiliteY) - 128) <= 2 &&
             image.green(x, hiliteY) <= 2 &&
             Math.abs(image.blue(x, hiliteY) - 128) <= 2
    })
    preview.m = Object.assign({}, preview.m, {alpha: 0})
    waitForRendering(board)
    const transparent = grabImage(board)
    compare(transparent.pixel(x, backY), Qt.rgba(0, 0, 1, 1))
    compare(transparent.pixel(x, hiliteY), Qt.rgba(0, 0, 1, 1))
    preview.m = Object.assign({}, preview.m, {alpha: 1})
  }

  function menu() { return findChild(combo, "comboMenu") }
  function entry(index) { return findChild(menu().contentItem, "comboItem" + index) }
  function open() {
    mouseClick(combo, combo.width - 30, combo.height / 2)
    tryCompare(menu(), "opened", true)
  }
  function highlightedCount() {
    var count = 0
    for (var i = 0; i < 3; ++i)
      if (Qt.colorEqual(entry(i).color, Theme.accent))
        ++count
    return count
  }
  function init() {
    Theme.dark = false
    combo.value = "One"
    mouseMove(test, 1, 1)
  }
  function cleanup() {
    menu().close()
    tryCompare(menu(), "visible", false)
  }

  function test_sidebarFollowsTheme() {
    const light = Theme.sidebarBg
    compare(sidebar.color, light)
    verify(Math.abs(sidebar.color.r - Theme.ground.r) < 0.08)
    Theme.dark = true
    tryCompare(sidebar, "color", Theme.sidebarBg)
    verify(sidebar.color.r < 0.15)
    verify(Math.abs(sidebar.color.r - Theme.ground.r) < 0.08)
    Theme.dark = false
    tryCompare(sidebar, "color", light)
  }

  function test_pointerClearsPreviousItem() {
    open()
    mouseMove(entry(0), 20, 15)
    compare(highlightedCount(), 1)
    mouseMove(entry(1), 20, 15)
    compare(entry(1).color, Theme.accent)
    compare(entry(0).color, Qt.rgba(0, 0, 0, 0))
    compare(highlightedCount(), 1)
    mouseMove(entry(2), 20, 15)
    compare(entry(1).color, Qt.rgba(0, 0, 0, 0))
    compare(highlightedCount(), 1)
    mouseMove(test, 1, 1)
    tryCompare(entry(2), "color", Qt.rgba(0, 0, 0, 0))
    compare(highlightedCount(), 0)
  }

  function test_keyboardAndPointerShareHighlight() {
    open()
    keyClick(Qt.Key_Down)
    compare(entry(1).color, Theme.accent)
    compare(highlightedCount(), 1)
    mouseMove(entry(2), 20, 15)
    compare(entry(1).color, Qt.rgba(0, 0, 0, 0))
    compare(highlightedCount(), 1)
    keyClick(Qt.Key_Up)
    compare(entry(1).color, Theme.accent)
    compare(highlightedCount(), 1)
    keyClick(Qt.Key_Return)
    compare(combo.value, "Two")
    tryCompare(menu(), "visible", false)
    open()
    compare(highlightedCount(), 1)
    compare(entry(1).color, Theme.accent)
  }
}
