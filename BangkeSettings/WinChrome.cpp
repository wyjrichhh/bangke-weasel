#include "WinChrome.h"

#include <windows.h>
#include <dwmapi.h>

#include <QGuiApplication>

#include "SystemTheme.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

// 原生标题栏跟随系统深浅:不设的话深色模式下标题栏仍是白的
void SetTitleBarDarkMode(HWND hwnd, bool dark) {
  const BOOL value = dark ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &value,
                        sizeof(value));
}

}  // namespace

void WinChrome::attach(QWindow* window, SystemTheme* theme) {
  window_ = window;
  applyTheme(theme->dark());
  // 标题栏跟随系统深浅，内容区由 QML 的不透明语义色绘制。
  QObject::connect(theme, &SystemTheme::changed, window, [this, theme] {
    applyTheme(theme->dark());
  });
}

void WinChrome::applyTheme(bool dark) {
  if (!window_)
    return;
  const HWND hwnd = (HWND)window_->winId();
  SetTitleBarDarkMode(hwnd, dark);
}
