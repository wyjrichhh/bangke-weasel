#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QObject>

// 系统深浅的唯一来源:QML 侧只读 dark,变化由 changed 通知。
// 系统切换主题不会走 Qt 事件循环,只能自己钩 WM_SETTINGCHANGE。
class SystemTheme : public QObject, public QAbstractNativeEventFilter {
  Q_OBJECT
  Q_PROPERTY(bool dark READ dark NOTIFY changed)

 public:
  explicit SystemTheme(QObject* parent = nullptr);
  ~SystemTheme() override;

  bool dark() const { return dark_; }

  // 供 QML/C++ 主动轮询(值没变不发信号)
  Q_INVOKABLE void refresh();

  bool nativeEventFilter(const QByteArray& eventType, void* message,
                         qintptr* result) override;

 signals:
  void changed();

 private:
  bool dark_ = false;
};
