#include "GeneralPage.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Ui.h"

#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

#include "Levers.h"

GeneralPage::GeneralPage(QWidget* parent) : QWidget(parent) {
  api_ = leversApi();
  settings_ = api_->custom_settings_init("default", "Bangke::GeneralPage");
  bangkeStyle_ = api_->custom_settings_init("weasel", "Bangke::GeneralPage");

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 12, 16, 12);
  layout->setSpacing(10);

  pageSize_ = new QSpinBox;
  pageSize_->setRange(1, 20);
  pageSize_->setValue(5);
  pageSize_->setFixedWidth(96);

  shiftL_ = new QComboBox;
  shiftR_ = new QComboBox;
  shiftL_->setFixedWidth(150);
  shiftR_->setFixedWidth(150);
  const struct { const char* val; const char* label; } actions[] = {
      {"commit_code", "提交编码"}, {"commit_text", "提交中文"},
      {"inline_ascii", "内嵌英文"}, {"clear", "清除"}, {"noop", "无操作"},
  };
  for (const auto& a : actions) {
    shiftL_->addItem(QString::fromUtf8(a.label), QString::fromLatin1(a.val));
    shiftR_->addItem(QString::fromUtf8(a.label), QString::fromLatin1(a.val));
  }

  commentHints_ = new ToggleSwitch;

  notifications_ = new ToggleSwitch;
  notificationsTime_ = new QSpinBox;
  notificationsTime_->setRange(0, 5000);
  notificationsTime_->setSuffix(QStringLiteral(u" ms"));
  notificationsTime_->setFixedWidth(96);

  layout->addWidget(makeCard(
      QStringLiteral(u"输入习惯"),
      makeSettingRows({
          makeSettingRow(QStringLiteral(u"每页候选数"), pageSize_),
          makeSettingRow(QStringLiteral(u"左 Shift 切换行为"), shiftL_),
          makeSettingRow(QStringLiteral(u"右 Shift 切换行为"), shiftR_),
          makeSettingRow(QStringLiteral(u"候选词后显示拼音注释"), commentHints_,
                         QStringLiteral(u"关闭时仅保留 AI 标记")),
      })));
  layout->addWidget(makeCard(
      QStringLiteral(u"提示"),
      makeSettingRows({
          makeSettingRow(QStringLiteral(u"切换时弹出提示"), notifications_,
                         QStringLiteral(u"切换方案、开关中英等")),
          makeSettingRow(QStringLiteral(u"提示停留时间"),
                         notificationsTime_),
      })));
  layout->addStretch(1);

  load();
}

void GeneralPage::load() {
  api_->load_settings(settings_);
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return;
  RimeApi* rime = rime_get_api();

  int ps = 5;
  if (rime->config_get_int(&config, "menu/page_size", &ps))
    pageSize_->setValue(ps);
  initPs_ = ps;

  // 注释显示:0=隐藏(共享默认),14=显示;未打补丁即默认隐藏
  api_->load_settings(bangkeStyle_);
  RimeConfig wcfg = {0};
  int cfp = 0;
  if (api_->settings_get_config(bangkeStyle_, &wcfg))
    rime->config_get_int(&wcfg, "style/comment_font_point", &cfp);
  commentHints_->setChecked(cfp >= 14);
  initCommentHints_ = cfp >= 14;

  // 共享 weasel.yaml 里 show_notifications 默认 false、停留 1200ms;
  // settings_get_config 只含补丁,取不到就按这两个默认显示
  Bool notify = False;
  const bool hasNotify =
      api_->settings_get_config(bangkeStyle_, &wcfg) &&
      rime->config_get_bool(&wcfg, "show_notifications", &notify);
  notifications_->setChecked(hasNotify && notify != False);
  initNotifications_ = notifications_->isChecked();
  int ntime = 1200;
  if (api_->settings_get_config(bangkeStyle_, &wcfg))
    rime->config_get_int(&wcfg, "show_notifications_time", &ntime);
  notificationsTime_->setValue(ntime);
  initNotificationsTime_ = ntime;

  // settings_get_config 只含 custom 补丁,不含合并后的共享默认;
  // 空 = 未打补丁,显示控件默认值
  const char* sl =
      rime->config_get_cstring(&config, "ascii_composer/switch_key/Shift_L");
  const char* sr =
      rime->config_get_cstring(&config, "ascii_composer/switch_key/Shift_R");
  initShiftL_ = sl ? QString::fromUtf8(sl) : QString();
  initShiftR_ = sr ? QString::fromUtf8(sr) : QString();
  for (int i = 0; i < shiftL_->count(); ++i) {
    if (shiftL_->itemData(i).toString() == initShiftL_)
      shiftL_->setCurrentIndex(i);
    if (shiftR_->itemData(i).toString() == initShiftR_)
      shiftR_->setCurrentIndex(i);
  }
  loaded_ = true;
}

bool GeneralPage::save() {
  if (!loaded_)
    return false;
  const int ps = pageSize_->value();
  const QString sl = shiftL_->currentData().toString();
  const QString sr = shiftR_->currentData().toString();
  const bool hints = commentHints_->isChecked();
  bool changed = false;

  // default 侧:有变化才写(save_settings 无变化时返回 false,不能连坐)
  if (ps != initPs_ || sl != initShiftL_ || sr != initShiftR_) {
    // 重读再写:与 SwitcherPage 共写 default.custom.yaml,不能拿旧树覆写对方
    api_->load_settings(settings_);
    if (ps != initPs_)
      api_->customize_int(settings_, "menu/page_size", ps);
    if (sl != initShiftL_)
      api_->customize_string(settings_, "ascii_composer/switch_key/Shift_L",
                             sl.toUtf8().constData());
    if (sr != initShiftR_)
      api_->customize_string(settings_, "ascii_composer/switch_key/Shift_R",
                             sr.toUtf8().constData());
    if (!api_->save_settings(settings_))
      return false;
    initPs_ = ps;
    initShiftL_ = sl;
    initShiftR_ = sr;
    changed = true;
  }

  // weasel 侧(注释总开关/提示):同样有变化才写;先 reload 防 StylePage 旧树覆写
  const bool notify = notifications_->isChecked();
  const int notifyTime = notificationsTime_->value();
  if (hints != initCommentHints_ || notify != initNotifications_ ||
      notifyTime != initNotificationsTime_) {
    api_->load_settings(bangkeStyle_);
    if (hints != initCommentHints_)
      api_->customize_int(bangkeStyle_, "style/comment_font_point",
                          hints ? 14 : 0);
    if (notify != initNotifications_)
      api_->customize_bool(bangkeStyle_, "show_notifications", notify);
    if (notifyTime != initNotificationsTime_)
      api_->customize_int(bangkeStyle_, "show_notifications_time", notifyTime);
    if (!api_->save_settings(bangkeStyle_))
      return false;
    initCommentHints_ = hints;
    initNotifications_ = notify;
    initNotificationsTime_ = notifyTime;
    changed = true;
  }
  return changed;
}
