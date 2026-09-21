#include "GeneralModel.h"

#include <QMap>
#include <QStringList>
#include <QVariant>

#include "Levers.h"

#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

namespace {

// 与原下拉框的条目表同源:label 显示,val 写进 ascii_composer/switch_key
const struct {
  const char* val;
  const char* label;
} kShiftActions[] = {
    {"commit_code", "提交编码"}, {"commit_text", "提交中文"},
    {"inline_ascii", "内嵌英文"}, {"clear", "清除"}, {"noop", "无操作"},
};

}  // namespace

GeneralModel::GeneralModel(QObject* parent) : QObject(parent) {
  api_ = leversApi();
  settings_ = api_->custom_settings_init("default", "Bangke::GeneralModel");
  bangkeStyle_ = api_->custom_settings_init("weasel", "Bangke::GeneralModel");
}

QVariantList GeneralModel::shiftActions() const {
  QVariantList out;
  for (const auto& a : kShiftActions) {
    QVariantMap m;
    m.insert(QStringLiteral("label"), QString::fromUtf8(a.label));
    m.insert(QStringLiteral("value"), QString::fromLatin1(a.val));
    out.append(m);
  }
  return out;
}

void GeneralModel::load() {
  api_->load_settings(settings_);
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return;
  RimeApi* rime = rime_get_api();

  int ps = 5;
  if (rime->config_get_int(&config, "menu/page_size", &ps))
    pageSize_ = ps;
  initPs_ = ps;

  // 注释显示:0=隐藏(共享默认),14=显示;未打补丁即默认隐藏
  api_->load_settings(bangkeStyle_);
  RimeConfig wcfg = {0};
  int cfp = 0;
  if (api_->settings_get_config(bangkeStyle_, &wcfg))
    rime->config_get_int(&wcfg, "style/comment_font_point", &cfp);
  commentHints_ = cfp >= 14;
  initCommentHints_ = cfp >= 14;

  // 共享 weasel.yaml 里 show_notifications 默认 false、停留 1200ms;
  // settings_get_config 只含补丁,取不到就按这两个默认显示
  Bool notify = False;
  const bool hasNotify =
      api_->settings_get_config(bangkeStyle_, &wcfg) &&
      rime->config_get_bool(&wcfg, "show_notifications", &notify);
  notifications_ = hasNotify && notify != False;
  initNotifications_ = notifications_;
  int ntime = 1200;
  if (api_->settings_get_config(bangkeStyle_, &wcfg))
    rime->config_get_int(&wcfg, "show_notifications_time", &ntime);
  notificationsTime_ = ntime;
  initNotificationsTime_ = ntime;

  // settings_get_config 只含 custom 补丁,不含合并后的共享默认;
  // 空 = 未打补丁,保持当前值(与下拉框"找不到就不动"一致)
  const char* sl =
      rime->config_get_cstring(&config, "ascii_composer/switch_key/Shift_L");
  const char* sr =
      rime->config_get_cstring(&config, "ascii_composer/switch_key/Shift_R");
  initShiftL_ = sl ? QString::fromUtf8(sl) : QString();
  initShiftR_ = sr ? QString::fromUtf8(sr) : QString();
  for (const auto& a : kShiftActions) {
    const QLatin1String val(a.val);
    if (initShiftL_ == val)
      shiftL_ = initShiftL_;
    if (initShiftR_ == val)
      shiftR_ = initShiftR_;
  }
  loaded_ = true;
  emit changed();
}

bool GeneralModel::save() {
  if (!loaded_)
    return false;
  const int ps = pageSize_;
  const QString sl = shiftL_;
  const QString sr = shiftR_;
  const bool hints = commentHints_;
  bool changed = false;

  // default 侧:有变化才写(save_settings 无变化时返回 false,不能连坐)
  if (ps != initPs_ || sl != initShiftL_ || sr != initShiftR_) {
    // 重读再写:与 SwitcherModel 共写 default.custom.yaml,不能拿旧树覆写对方
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

  // weasel 侧(注释总开关/提示):同样有变化才写;先 reload 防 StyleModel 旧树覆写
  const bool notify = notifications_;
  const int notifyTime = notificationsTime_;
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
