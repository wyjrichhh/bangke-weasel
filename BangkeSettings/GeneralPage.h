#pragma once
#include <QWidget>

#include <rime_levers_api.h>

class QComboBox;
class QSpinBox;

class ToggleSwitch;

// 通用配置页:经 rime levers 写 default.custom.yaml
// (menu/page_size、ascii_composer/switch_key),与 SwitcherPage 共写一文件。
class GeneralPage : public QWidget {
  Q_OBJECT
 public:
  explicit GeneralPage(QWidget* parent = nullptr);
  void load();
  bool save();

 private:
  RimeLeversApi* api_ = nullptr;
  RimeCustomSettings* settings_ = nullptr;
  // 注释显示是 weasel 样式键,写 weasel.custom.yaml(与 default 分开)
  RimeCustomSettings* bangkeStyle_ = nullptr;
  QSpinBox* pageSize_ = nullptr;
  QComboBox* shiftL_ = nullptr;
  QComboBox* shiftR_ = nullptr;
  ToggleSwitch* commentHints_ = nullptr;
  ToggleSwitch* notifications_ = nullptr;
  QSpinBox* notificationsTime_ = nullptr;
  // 载入时的原值(Shift 空串 = custom yaml 无此键);保存只写有变化的键
  int initPs_ = 0;
  QString initShiftL_;
  QString initShiftR_;
  bool initCommentHints_ = false;
  bool initNotifications_ = false;
  int initNotificationsTime_ = 0;
  // 载过值才允许保存：init* 都是载入时记下的,没载过就还是 0/空,与控件默认值不等,
  // 保存会把默认值写进配置(见 StylePage::save 同类事故)
  bool loaded_ = false;
};
