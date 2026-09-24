#pragma once

#include <QAbstractNativeEventFilter>
#include <QWindow>

class SystemTheme;

// frameless 主窗的 Windows 侧配套:WM_NCHITTEST 划出标题栏拖动区(交给系统,
// 白得 Aero Snap)与八向缩放边,DWM 给 Win11 圆角,窗口合成属性给整窗
// acrylic(mac 侧栏 vibrancy 的 Windows 近似,QML 侧的半透明底色透出它)。
// 几何常量与 MacTitleBar.qml 一一对应(标题栏高 44 逻辑像素、圆点区在左上),
// 改一处要同步另一处。
class WinChrome : public QAbstractNativeEventFilter {
 public:
  static constexpr int kTitleBarHeight = 44;   // 逻辑像素
  static constexpr int kButtonZoneWidth = 72;  // 三圆点区,命中留给按钮
  static constexpr int kResizeBorder = 6;      // 逻辑像素

  void attach(QWindow* window, SystemTheme* theme);

  bool nativeEventFilter(const QByteArray& eventType, void* message,
                         qintptr* result) override;

 private:
  void applyBackdrop(bool dark);

  QWindow* window_ = nullptr;
};
