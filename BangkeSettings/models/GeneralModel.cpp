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

// 通知三态。引擎的闸门是顶层键 show_notifications,两种形态:布尔(全开/
// 全关),或事件名列表(只提示列表内的事件,见 BangkeRime::_ShowMessage)。
// mac 的 never/appropriate/always 正好落成 false / [schema] / true。
QString ReadNotifications(RimeLeversApi* api, RimeCustomSettings* settings) {
  RimeConfig config = {0};
  if (!api->settings_get_config(settings, &config))
    return QStringLiteral("never");
  RimeApi* rime = rime_get_api();
  Bool on = False;
  if (rime->config_get_bool(&config, "show_notifications", &on))
    return on != False ? QStringLiteral("always") : QStringLiteral("never");
  RimeConfigIterator iter = {0};
  if (rime->config_begin_list(&iter, &config, "show_notifications")) {
    rime->config_end(&iter);
    return QStringLiteral("appropriate");
  }
  return QStringLiteral("never");
}

// 列表形态只能经 customize_item 打补丁(levers 的 "patch a list or a map"):
// 造一个根为列表的 config 交给它。写不进去就退回布尔 —— 开关本身不能丢
void WriteNotifications(RimeLeversApi* api,
                        RimeCustomSettings* settings,
                        const QString& mode) {
  if (mode == QStringLiteral("appropriate") && api->customize_item) {
    RimeConfig list = {0};
    RimeApi* rime = rime_get_api();
    const bool ok = rime->config_init(&list) &&
                    rime->config_load_string(&list, "- schema") &&
                    api->customize_item(settings, "show_notifications", &list);
    if (list.ptr)
      rime->config_close(&list);
    if (ok)
      return;
  }
  api->customize_bool(settings, "show_notifications",
                      mode == QStringLiteral("always") ? True : False);
}

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

  notificationsMode_ = ReadNotifications(api_, bangkeStyle_);
  initNotifications_ = notificationsMode_;

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

  // weasel 侧(注释/通知):同样有变化才写;先 reload 防 StyleModel 旧树覆写
  const bool hints = commentHints_;
  const bool notify = notificationsMode_ != initNotifications_;
  if (hints != initCommentHints_ || notify) {
    api_->load_settings(bangkeStyle_);
    if (hints != initCommentHints_)
      api_->customize_int(bangkeStyle_, "style/comment_font_point",
                          hints ? 16 : 0);
    if (notify)
      WriteNotifications(api_, bangkeStyle_, notificationsMode_);
    if (!api_->save_settings(bangkeStyle_))
      return false;
    initCommentHints_ = hints;
    initNotifications_ = notificationsMode_;
    changed = true;
  }
  return changed;
}
