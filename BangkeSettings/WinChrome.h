#pragma once

#include <QObject>
#include <QWindow>

class SystemTheme;

// 原生窗口的 Win32 配套:整窗 acrylic(mac 侧栏 vibrancy 的 Windows 近似,
// QML 侧的半透明底色透出它)+ 标题栏深浅跟随系统。窗口框架本身(边框/
// 按钮/圆角/缩放)全部交给系统,不再自绘
class WinChrome : public QObject {
 public:
  void attach(QWindow* window, SystemTheme* theme);

 private:
  void applyBackdrop(bool dark);

  QWindow* window_ = nullptr;
};
