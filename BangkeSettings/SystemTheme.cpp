#include "SystemTheme.h"

#include <QCoreApplication>
#include <windows.h>

namespace {

// 读不到键(键不存在/旧系统)按浅色处理
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

}  // namespace

SystemTheme::SystemTheme(QObject* parent) : QObject(parent) {
  dark_ = !SystemIsLight();
  QCoreApplication::instance()->installNativeEventFilter(this);
}

SystemTheme::~SystemTheme() {
  if (auto* app = QCoreApplication::instance())
    app->removeNativeEventFilter(this);
}

void SystemTheme::refresh() {
  const bool dark = !SystemIsLight();
  if (dark == dark_)
    return;
  dark_ = dark;
  emit changed();
}

bool SystemTheme::nativeEventFilter(const QByteArray& eventType, void* message,
                                    qintptr* result) {
  Q_UNUSED(result);
  if (eventType != QByteArrayLiteral("windows_generic_MSG"))
    return false;
  MSG* msg = static_cast<MSG*>(message);
  if (msg->message == WM_SETTINGCHANGE && msg->lParam &&
      wcscmp((const wchar_t*)msg->lParam, L"ImmersiveColorSet") == 0)
    refresh();
  return false;
}
