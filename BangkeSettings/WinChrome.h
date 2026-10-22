#pragma once

#include <QObject>
#include <QWindow>

class SystemTheme;

// 原生标题栏跟随系统深浅；边框、按钮、圆角和缩放交给 Windows。
class WinChrome : public QObject {
 public:
  void attach(QWindow* window, SystemTheme* theme);

 private:
  void applyTheme(bool dark);

  QWindow* window_ = nullptr;
};
