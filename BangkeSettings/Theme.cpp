#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QWidget>
#include <QWindow>
#include <windows.h>

#include <utility>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

namespace Theme {
namespace {

QApplication* g_app = nullptr;
int g_appliedLight = -1;

bool SystemIsLight() {
  HKEY h = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\"
                    L"Personalize",
                    0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS)
    return true;
  DWORD v = 1, sz = sizeof(v), type = 0;
  const LONG r = RegQueryValueExW(h, L"AppsUseLightTheme", nullptr, &type,
                                  (LPBYTE)&v, &sz);
  RegCloseKey(h);
  return r != ERROR_SUCCESS || v != 0;
  }

// DWMWA_USE_IMMERSIVE_DARK_MODE:Win10 1809+ 为 19/20 均可
void ApplyTitleBars(bool light) {
  const BOOL dark = light ? FALSE : TRUE;
  for (auto* widget : QApplication::topLevelWidgets()) {
    if (auto* handle = widget->windowHandle())
      DwmSetWindowAttribute((HWND)handle->winId(), 20, &dark, sizeof(dark));
  }
}

// 一份模板 + 一张 token 表。改主色只动这里的 accent 三行,不必在两份 QSS 里找。
// 名值直接写进 QSS:Qt 的 px 是逻辑像素,会随 DPI 缩放,所以这里的数字就是设计稿上的数字。
struct Tokens {
  const char* ground;
  const char* surface;
  const char* hair;
  const char* hair_soft;
  const char* ink;
  const char* ink_2;
  const char* ink_3;
  const char* accent;
  const char* accent_soft;
  const char* accent_ink;
  const char* accent_hover;
  const char* accent_press;
  const char* on_accent;
  const char* field;
  const char* field_border;
  const char* track;
  const char* scroll;
  const char* scroll_hover;
  const char* toast_bg;
  const char* toast_fg;
};

// 浅色
const Tokens kLight{
    /*ground*/ "#F7F8F8",
    /*surface*/ "#FFFFFF",
    /*hair*/ "#EBEEEF",
    /*hair_soft*/ "#F1F4F4",
    /*ink*/ "#14181A",
    /*ink_2*/ "#5C6668",
    /*ink_3*/ "#8A9496",
    /*accent*/ "#0E9384",
    /*accent_soft*/ "#E4F3F1",
    /*accent_ink*/ "#0A6A60",
    /*accent_hover*/ "#0B8175",
    /*accent_press*/ "#096B61",
    /*on_accent*/ "#FFFFFF",
    /*field*/ "#FFFFFF",
    /*field_border*/ "#DFE3E4",
    /*track*/ "#E4E8E9",
    /*scroll*/ "#D3D8D9",
    /*scroll_hover*/ "#B9C0C1",
    /*toast_bg*/ "rgba(20, 24, 26, 235)",
    /*toast_fg*/ "#FFFFFF",
};

// 深色
const Tokens kDark{
    /*ground*/ "#141719",
    /*surface*/ "#1C2022",
    /*hair*/ "#2A3032",
    /*hair_soft*/ "#232829",
    /*ink*/ "#E8ECEC",
    /*ink_2*/ "#A2ACAE",
    /*ink_3*/ "#7C8688",
    /*accent*/ "#2DD4BF",
    /*accent_soft*/ "#123B37",
    /*accent_ink*/ "#5EEAD4",
    /*accent_hover*/ "#3FE0CC",
    /*accent_press*/ "#1FB9A6",
    /*on_accent*/ "#08201C",
    /*field*/ "#22282A",
    /*field_border*/ "#333B3D",
    /*track*/ "#333B3D",
    /*scroll*/ "#3A4244",
    /*scroll_hover*/ "#4A5456",
    /*toast_bg*/ "rgba(45, 212, 191, 235)",
    /*toast_fg*/ "#08201C",
};

const char* kQssTemplate = R"QSS(
/* 字号阶:页标题 20 / 正文 13 / 次级 12.5 / 辅助 11.5,只在这几行定义 */
* { font-family: "Microsoft YaHei UI"; font-size: 13px; }
QLabel#pageTitle { font-size: 20px; font-weight: 600; color: $ink; }
QLabel#pageSub { font-size: 12.5px; color: $ink_2; }
QLabel#cardTitle { font-size: 11.5px; font-weight: 600; letter-spacing: 0.7px; color: $ink_3; }
QLabel#rowTitle { font-size: 13px; color: $ink; }
QLabel#rowHint { font-size: 11.5px; color: $ink_3; }
QLabel#hint { font-size: 11.5px; color: $ink_3; }

QMainWindow, QDialog { background: $ground; }
QFrame#pageRoot { background: transparent; }
QLabel { color: $ink; background: transparent; }
QToolTip { background: $surface; color: $ink; border: 1px solid $field_border; padding: 6px 9px; }
QMessageBox { background: $surface; }

/* 卡片:1px 发丝边 + 投影。投影由 Ui.h 的 makeCard 用 QGraphicsDropShadowEffect 加,
   QSS 没有 box-shadow */
QFrame#card { background: $surface; border: 1px solid $hair; border-radius: 13px; }
QFrame#line { border: none; border-top: 1px solid $hair_soft; }

/* 输入类控件:34px 高、圆角 9。聚焦用 2px 主色边并把内边距各收 1px,
   文字位置不跳 —— QSS 做不到外发光,这是最接近设计稿焦点环的做法 */
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
  background: $field; border: 1px solid $field_border; border-radius: 9px;
  padding: 0 11px; min-height: 32px; color: $ink; selection-background-color: $accent;
  selection-color: $on_accent;
}
QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QLineEdit:hover { border-color: $ink_3; }
QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {
  border: 2px solid $accent; padding: 0 10px;
}
QComboBox::drop-down { border: none; width: 26px; }
QComboBox QAbstractItemView {
  background: $surface; border: 1px solid $field_border; border-radius: 10px;
  selection-background-color: $accent_soft; selection-color: $accent_ink;
  outline: none; padding: 5px; color: $ink;
}
QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
  background: transparent; border: none; width: 18px;
}

/* 按钮:默认 34px 高、圆角 9;主按钮实心无边框 */
QPushButton {
  background: $surface; border: 1px solid $field_border; border-radius: 9px;
  padding: 0 16px; min-height: 32px; color: $ink_2;
}
QPushButton:hover { border-color: $ink_3; color: $ink; }
QPushButton:pressed { background: $hair_soft; }
QPushButton:disabled { color: $ink_3; border-color: $hair; }
QPushButton#primary {
  background: $accent; border: none; color: $on_accent; font-weight: 600;
  padding: 0 20px;
}
QPushButton#primary:hover { background: $accent_hover; }
QPushButton#primary:pressed { background: $accent_press; }
QPushButton#primary:disabled { background: $accent_soft; color: $ink_3; }

/* 侧栏:条目 36px、圆角 9,选中是主色淡底药丸 */
QListWidget#nav { background: transparent; border: none; outline: none; padding: 4px 2px; }
QListWidget#nav::item {
  padding: 9px 12px; margin: 1px 4px; border-radius: 9px; color: $ink_2;
}
QListWidget#nav::item:hover { background: $hair_soft; }
QListWidget#nav::item:selected {
  background: $accent_soft; color: $accent_ink; font-weight: 600;
}

QListWidget#dictList, QTextBrowser, QListWidget {
  background: $field; border: 1px solid $field_border; border-radius: 10px;
  padding: 5px; color: $ink; outline: none;
}
QListWidget::item { padding: 7px 9px; border-radius: 7px; }
QListWidget::item:hover { background: $hair_soft; }
QListWidget::item:selected { background: $accent_soft; color: $accent_ink; }
QCheckBox { color: $ink; spacing: 9px; }

QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: $track; }
QSlider::sub-page:horizontal { background: $accent; border-radius: 2px; }
QSlider::handle:horizontal {
  width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
  background: $surface; border: 2px solid $accent;
}
QSlider::handle:horizontal:hover { border-color: $accent_hover; }

QScrollArea#pageScroll { background: transparent; border: none; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: $scroll; border-radius: 5px; min-height: 36px; }
QScrollBar::handle:vertical:hover { background: $scroll_hover; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: $scroll; border-radius: 5px; min-width: 36px; }
QScrollBar::handle:horizontal:hover { background: $scroll_hover; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

QLabel#toast {
  background: $toast_bg; color: $toast_fg; border-radius: 10px;
  padding: 11px 20px; font-weight: 600; font-size: 12.5px;
}
)QSS";

QString BuildQss(const Tokens& t) {
  QString qss = QString::fromUtf8(kQssTemplate);
  const std::pair<const char*, const char*> subs[] = {
      {"$ground", t.ground},         {"$surface", t.surface},
      {"$hair_soft", t.hair_soft},   {"$hair", t.hair},
      {"$ink_3", t.ink_3},           {"$ink_2", t.ink_2},
      {"$ink", t.ink},               {"$accent_soft", t.accent_soft},
      {"$accent_ink", t.accent_ink}, {"$accent_hover", t.accent_hover},
      {"$accent_press", t.accent_press}, {"$accent", t.accent},
      {"$on_accent", t.on_accent},   {"$field_border", t.field_border},
      {"$field", t.field},           {"$track", t.track},
      {"$scroll_hover", t.scroll_hover}, {"$scroll", t.scroll},
      {"$toast_bg", t.toast_bg},     {"$toast_fg", t.toast_fg},
  };
  // 长名先替,否则 $ink 会先吃掉 $ink_2 的前缀
  for (const auto& kv : subs)
    qss.replace(QLatin1String(kv.first), QLatin1String(kv.second));
  return qss;
}

}  // namespace

void Apply(QApplication& app) {
  g_app = &app;
  const bool light = SystemIsLight();
  g_appliedLight = light ? 1 : 0;

  app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
  app.setStyleSheet(BuildQss(light ? kLight : kDark));

  // QSS 覆盖不到的零散颜色(菜单/弹出等)走调色板兜底
  QPalette pal;
  const Tokens& t = light ? kLight : kDark;
  const auto col = [](const char* hex) { return QColor(QString::fromLatin1(hex)); };
  pal.setColor(QPalette::Window, col(t.ground));
  pal.setColor(QPalette::Base, col(t.surface));
  pal.setColor(QPalette::Text, col(t.ink));
  pal.setColor(QPalette::WindowText, col(t.ink));
  pal.setColor(QPalette::ButtonText, col(t.ink));
  pal.setColor(QPalette::Highlight, col(t.accent));
  pal.setColor(QPalette::HighlightedText, col(t.on_accent));
  app.setPalette(pal);

  ApplyTitleBars(light);
  for (auto* widget : QApplication::topLevelWidgets())
    widget->update();
}

void RefreshIfChanged() {
  if (!g_app)
    return;
  const bool light = SystemIsLight();
  if ((light ? 1 : 0) != g_appliedLight)
    Apply(*g_app);
}

}  // namespace Theme
