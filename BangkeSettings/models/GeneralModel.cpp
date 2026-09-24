#include "GeneralModel.h"

#include <QMap>
#include <QStringList>
#include <QVariant>

#include "Levers.h"

#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

namespace {

// Shift 行为下拉:label 显示,val 写进 ascii_composer/switch_key
const struct {
  const char* val;
  const char* label;
} kShiftActions[] = {
    {"commit_code", "上屏编码"}, {"commit_text", "上屏候选"},
    {"inline_ascii", "内嵌英文"}, {"clear", "清除"}, {"noop", "不处理"},
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

QVariantList GeneralModel::notificationOptions() const {
  const struct {
    const char* val;
    const char* label;
  } opts[] = {{"never", "从不"}, {"appropriate", "适当时候"}, {"always", "总是"}};
  QVariantList out;
  for (const auto& o : opts) {
    QVariantMap m;
    m.insert(QStringLiteral("label"), QString::fromUtf8(o.label));
    m.insert(QStringLiteral("value"), QString::fromLatin1(o.val));
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

  // 拼音注释:0=隐藏、16=显示(共享默认已随 mac 改为显示)
  const int cfp = style_.GetStyleInt("comment_font_point", 16);
  commentHints_ = cfp >= 16;
  initCommentHints_ = commentHints_;

  // 通知:引擎只有布尔;true 显示为「适当时候」(mac 语义)
  notificationsMode_ = style_.GetStyleBool("show_notifications", false)
                           ? QStringLiteral("appropriate")
                           : QStringLiteral("never");
  initNotifications_ = notificationsMode_;

  trayIcon_ = style_.GetStyleBool("display_tray_icon", false);
  initTrayIcon_ = trayIcon_;

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

void GeneralModel::resetDefaults() {
  pageSize_ = 5;
  shiftL_ = QStringLiteral("commit_code");
  shiftR_ = QStringLiteral("commit_code");
  commentHints_ = true;
  notificationsMode_ = QStringLiteral("never");
  trayIcon_ = false;
  emit changed();
}

bool GeneralModel::save() {
  if (!loaded_)
    return false;
  const int ps = pageSize_;
  const QString sl = shiftL_;
  const QString sr = shiftR_;
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

  // weasel 侧(注释/通知/托盘):同样有变化才写;先 reload 防 StyleModel 旧树覆写
  const bool hints = commentHints_;
  const bool notify = notificationsMode_ != QStringLiteral("never");
  const bool tray = trayIcon_;
  if (hints != initCommentHints_ || notify != (initNotifications_ != "never") ||
      tray != initTrayIcon_) {
    api_->load_settings(bangkeStyle_);
    if (hints != initCommentHints_)
      api_->customize_int(bangkeStyle_, "style/comment_font_point",
                          hints ? 16 : 0);
    if (notify != (initNotifications_ != QStringLiteral("never")))
      api_->customize_bool(bangkeStyle_, "show_notifications", notify);
    if (tray != initTrayIcon_)
      api_->customize_bool(bangkeStyle_, "style/display_tray_icon", tray);
    if (!api_->save_settings(bangkeStyle_))
      return false;
    initCommentHints_ = hints;
    initNotifications_ = notificationsMode_;
    initTrayIcon_ = tray;
    changed = true;
  }
  return changed;
}
