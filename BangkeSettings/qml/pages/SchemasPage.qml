import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import Bangke.Pages

// 方案与词典页(原「高级」页的两块,mac 没有这一页 —— 方案切换和词典在
// mac 上由菜单栏承担,Windows 没有菜单栏所以收进设置)。
// 词典的四个动作走 DictModel 的两段式:choose* 发请求 → QML 开文件对话框 →
// apply* 把路径交回去;模型自己没有文件对话框(Widgets 已经摘掉了)。
PageScroll {

  // FileDialog 给的是 url,C++ 侧要的是本地路径
  function toLocalPath(url) {
    var s = url.toString()
    if (s.indexOf("file:///") === 0)
      s = s.substring(8)
    return decodeURIComponent(s)
  }

  Card {
    Layout.fillWidth: true
    title: "方案选单"

    Txt {
      text: "已启用的方案置顶显示。"
      color: Theme.ink2
      font.pixelSize: Theme.subSize
      Layout.fillWidth: true
      Layout.topMargin: 10
    }

    RowLayout {
      Layout.fillWidth: true
      Layout.topMargin: 8

      Btn {
        text: "刷新"
        onClicked: app.switcher.refresh()
      }

      Item { Layout.fillWidth: true }
    }

    Column {
      Layout.fillWidth: true
      Layout.topMargin: 4

      Repeater {
        model: app.switcher

        delegate: ToggleRow {
          width: parent.width
          label: model.name
          value: model.enabled
          showDivider: index !== 0
          onEdited: function (v) {
            model.enabled = v
          }
        }
      }
    }
  }

  Card {
    Layout.fillWidth: true
    title: "用户词典"

    Column {
      Layout.fillWidth: true
      Layout.topMargin: 10

      Repeater {
        model: app.dict.dicts

        delegate: Rectangle {
          width: parent.width
          height: 30
          radius: 6
          color: index === app.dict.currentIndex ? Theme.accentSoft : "transparent"

          Behavior on color {
            ColorAnimation { duration: 100 }
          }

          Txt {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: modelData
            color: index === app.dict.currentIndex ? Theme.accentInk : Theme.ink
            elide: Text.ElideRight
          }

          MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: app.dict.currentIndex = index
          }
        }
      }
    }

    RowLayout {
      Layout.fillWidth: true
      Layout.topMargin: 10
      spacing: 8

      Btn {
        text: "备份…"
        enabled: app.dict.hasSelection
        onClicked: app.dict.backup()
      }

      Btn {
        text: "恢复…"
        onClicked: app.dict.chooseAndRestore()
      }

      Item { Layout.fillWidth: true }

      Btn {
        text: "导出为文本…"
        enabled: app.dict.hasSelection
        onClicked: app.dict.chooseAndExport()
      }

      Btn {
        text: "从文本导入…"
        enabled: app.dict.hasSelection
        onClicked: app.dict.chooseAndImport()
      }
    }
  }

  FileDialog {
    id: restoreDialog
    title: "恢复词典快照"
    fileMode: FileDialog.OpenFile
    nameFilters: ["词典快照 (*.userdb.txt *.userdb.kct.snapshot)", "所有文件 (*.*)"]
    onAccepted: app.dict.applyRestore(toLocalPath(selectedFile))
  }

  FileDialog {
    id: exportDialog
    title: "导出词典"
    fileMode: FileDialog.SaveFile
    nameFilters: ["文本文件 (*.txt)"]
    onAccepted: app.dict.applyExport(toLocalPath(selectedFile))
  }

  FileDialog {
    id: importDialog
    title: "导入词典"
    fileMode: FileDialog.OpenFile
    nameFilters: ["文本文件 (*.txt)", "所有文件 (*.*)"]
    onAccepted: app.dict.applyImport(toLocalPath(selectedFile))
  }

  Connections {
    target: app.dict
    function onRestoreRequested(path) { restoreDialog.open() }
  }

  Connections {
    target: app.dict
    function onExportRequested(path) { exportDialog.open() }
  }

  Connections {
    target: app.dict
    function onImportRequested(path) { importDialog.open() }
  }
}
