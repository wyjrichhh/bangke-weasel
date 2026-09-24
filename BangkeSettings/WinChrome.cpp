#include "WinChrome.h"

#include <windows.h>
#include <dwmapi.h>

#include <QGuiApplication>

#include "SystemTheme.h"

#pragma comment(lib, "dwmapi.lib")

namespace {

// 整窗 acrylic:SetWindowCompositionAttribute + ACCENT_ENABLE_ACRYLICBLURBEHIND。
// gradientColor 是 AABBGGRR 的 tint 色,acrylic 失效的环境(RDP 裁剪、旧系统)
// 退化为纯 tint 色 —— 正好是可接受的观感,不会露出桌面
void EnableAcrylic(HWND hwnd, bool dark) {
  struct ACCENT_POLICY {
    int accentState;
    int flags;
    int gradientColor;
    int animationId;
  };
  struct WINCOMPATTRDATA {
    int attribute;
    void* data;
    int size;
  };
  ACCENT_POLICY policy = {};
  policy.accentState = 4;  // ACCENT_ENABLE_ACRYLICBLURBEHIND
  // tint 浅 #F0F0F0 / 深 #202020,alpha 60%:模糊层上罩薄底,
  // 透感优先(模糊质感由系统的 blur 给);acrylic 被禁用的环境
  // (RDP)窗口仍按半透明合成,保住「透」的部分
  policy.gradientColor = dark ? 0x99202020 : 0x99F0F0F0;
  WINCOMPATTRDATA data = {};
  data.attribute = 19;  // WCA_ACCENT_POLICY
  data.data = &policy;
  data.size = sizeof(policy);
  typedef BOOL(WINAPI * PFN_SetWindowCompositionAttribute)(HWND, const void*);
  if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
    auto set = (PFN_SetWindowCompositionAttribute)GetProcAddress(
        user32, "SetWindowCompositionAttribute");
    if (set)
      set(hwnd, &data);
  }
}

// 原生标题栏跟随系统深浅:不设的话深色模式下标题栏仍是白的
void SetTitleBarDarkMode(HWND hwnd, bool dark) {
  const BOOL value = dark ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &value,
                        sizeof(value));
}

}  // namespace

void WinChrome::attach(QWindow* window, SystemTheme* theme) {
  window_ = window;
  applyBackdrop(theme->dark());
  // 系统切深浅时换 acrylic 的 tint 与标题栏模式
  QObject::connect(theme, &SystemTheme::changed, window, [this, theme] {
    applyBackdrop(theme->dark());
  });
}

void WinChrome::applyBackdrop(bool dark) {
  if (!window_)
    return;
  const HWND hwnd = (HWND)window_->winId();
  EnableAcrylic(hwnd, dark);
  SetTitleBarDarkMode(hwnd, dark);
}
