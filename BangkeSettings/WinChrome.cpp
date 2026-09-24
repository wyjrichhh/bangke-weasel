#include "WinChrome.h"

#include <windows.h>
#include <windowsx.h>
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

}  // namespace

void WinChrome::attach(QWindow* window, SystemTheme* theme) {
  window_ = window;
  qApp->installNativeEventFilter(this);
  // Win11 圆角;Win10 无此属性,调用失败即维持方角(QML 里另有 1px 描边兜底)
  const DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
  DwmSetWindowAttribute((HWND)window->winId(), DWMWA_WINDOW_CORNER_PREFERENCE,
                        &pref, sizeof(pref));
  applyBackdrop(theme->dark());
  // 系统切深浅时换 acrylic 的 tint
  QObject::connect(theme, &SystemTheme::changed, window, [this, theme] {
    applyBackdrop(theme->dark());
  });
}

void WinChrome::applyBackdrop(bool dark) {
  if (!window_)
    return;
  EnableAcrylic((HWND)window_->winId(), dark);
}

bool WinChrome::nativeEventFilter(const QByteArray& eventType, void* message,
                                  qintptr* result) {
  if (eventType != QByteArrayLiteral("windows_generic_MSG") || !window_)
    return false;
  MSG* msg = (MSG*)message;
  if (msg->message != WM_NCHITTEST || msg->hwnd != (HWND)window_->winId())
    return false;

  RECT wr;
  if (!GetWindowRect(msg->hwnd, &wr))
    return false;
  // lParam 是物理像素;常量是逻辑像素,按消息时刻的 DPR 换算(多显示器安全)
  const double dpr = window_->devicePixelRatio();
  const int border = (int)(kResizeBorder * dpr);
  const int titleH = (int)(kTitleBarHeight * dpr);
  const int buttonZone = (int)(kButtonZoneWidth * dpr);
  const int x = GET_X_LPARAM(msg->lParam);
  const int y = GET_Y_LPARAM(msg->lParam);

  if (!IsZoomed(msg->hwnd)) {
    const bool left = x < wr.left + border;
    const bool right = x >= wr.right - border;
    const bool top = y < wr.top + border;
    const bool bottom = y >= wr.bottom - border;
    if (top && left) {
      *result = HTTOPLEFT;
      return true;
    }
    if (top && right) {
      *result = HTTOPRIGHT;
      return true;
    }
    if (bottom && left) {
      *result = HTBOTTOMLEFT;
      return true;
    }
    if (bottom && right) {
      *result = HTBOTTOMRIGHT;
      return true;
    }
    if (left) {
      *result = HTLEFT;
      return true;
    }
    if (right) {
      *result = HTRIGHT;
      return true;
    }
    if (top) {
      *result = HTTOP;
      return true;
    }
    if (bottom) {
      *result = HTBOTTOM;
      return true;
    }
  }

  // 标题栏拖动区;左上圆点区留给 QML 的按钮
  if (y < wr.top + titleH) {
    *result = x < wr.left + buttonZone ? HTCLIENT : HTCAPTION;
    return true;
  }
  return false;
}
