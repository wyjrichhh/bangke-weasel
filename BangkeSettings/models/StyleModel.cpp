#include "StyleModel.h"

#include <QFontDatabase>

namespace {

// 自定义配色卡管理的颜色键(bangke_custom 补丁)
const char* kCustomColorKeys[] = {
    "back_color",
    "text_color",
    "candidate_text_color",
    "label_color",
    "comment_text_color",
    "border_color",
    "hilited_candidate_text_color",
    "hilited_candidate_back_color",
    "hilited_label_color",
};

QVariantMap option(const QString& label, const QString& value) {
  QVariantMap m;
  m.insert(QStringLiteral("label"), label);
  m.insert(QStringLiteral("value"), value);
  return m;
}

QString valueOf(const QVariantMap& opt) {
  return opt.value(QStringLiteral("value")).toString();
}

int indexOfOption(const QVariantList& options, const QString& value) {
  for (int i = 0; i < options.size(); ++i)
    if (valueOf(options.at(i).toMap()) == value)
      return i;
  return -1;
}

}  // namespace

StyleControlModel::StyleControlModel(QObject* parent)
    : QAbstractListModel(parent) {
  const auto addToggle = [this](const QString& section, const QString& label,
                                const QString& key,
                                const QString& helper = QString()) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = QStringLiteral("toggle");
    r.helper = helper;
    r.fallback = QStringLiteral("0");
    r.value = false;
    rows_.push_back(r);
  };
  const auto addSpin = [this](const QString& section, const QString& label,
                              const QString& key, int lo, int hi,
                              const QString& helper = QString()) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = QStringLiteral("spin");
    r.helper = helper;
    r.lo = lo;
    r.hi = hi;
    // 原 QSpinBox 构造后停在 0,即兜底值
    r.fallback = QStringLiteral("0");
    r.value = 0;
    rows_.push_back(r);
  };
  const auto addEdit = [this](const QString& section, const QString& label,
                              const QString& key,
                              const QString& helper = QString()) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = QStringLiteral("edit");
    r.helper = helper;
    r.value = QString();
    rows_.push_back(r);
  };
  const auto addCombo = [this](const QString& section, const QString& label,
                               const QString& key, const QVariantList& options,
                               const QString& helper = QString(),
                               bool keepUnknown = false) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = QStringLiteral("combo");
    r.helper = helper;
    r.options = options;
    r.keepUnknown = keepUnknown;
    // 原 QComboBox 构造后停在第 0 项
    r.fallback = options.isEmpty() ? QString() : valueOf(options.first().toMap());
    r.value = r.fallback;
    rows_.push_back(r);
  };

  const QString kFont = QStringLiteral(u"字体");
  // 字体名从机器上装的字体里挑。首项是空值,与配置语义一致(空 = 跟随系统)
  QVariantList fontOptions;
  fontOptions.append(option(QStringLiteral(u"跟随系统"), QString()));
  for (const QString& family : QFontDatabase::families())
    fontOptions.append(option(family, family));
  addCombo(kFont, QStringLiteral(u"字体"), QStringLiteral("font_face"),
           fontOptions, QString(), /*keepUnknown=*/true);
  addSpin(kFont, QStringLiteral(u"序号字号"), QStringLiteral("label_font_point"),
          0, 48, QStringLiteral(u"0 = 不显示序号"));
  addSpin(kFont, QStringLiteral(u"注释字号"),
          QStringLiteral("comment_font_point"), 0, 48,
          QStringLiteral(u"0 = 不显示注释"));
  addSpin(kFont, QStringLiteral(u"候选缩写长度"),
          QStringLiteral("candidate_abbreviate_length"), 0, 20,
          QStringLiteral(u"0 = 不缩写"));

  // 选项在 forceLoad 里按实际方案表刷新
  addCombo(QStringLiteral(u"深色模式"), QStringLiteral(u"深色配色方案"),
           QStringLiteral("color_scheme_dark"), QVariantList(),
           QStringLiteral(u"系统切到深色时改用它"));

  const QString kDisplay = QStringLiteral(u"显示");
  addToggle(kDisplay, QStringLiteral(u"在输入框中显示编码"),
            QStringLiteral("inline_preedit"));
  addCombo(kDisplay, QStringLiteral(u"编码显示方式"),
           QStringLiteral("preedit_type"),
           {option(QStringLiteral(u"只显示已选部分"), QStringLiteral("composition")),
            option(QStringLiteral(u"预览首个候选"), QStringLiteral("preview")),
            option(QStringLiteral(u"预览整句"), QStringLiteral("preview_all"))});
  addToggle(kDisplay, QStringLiteral(u"英文提示跟随光标"),
            QStringLiteral("ascii_tip_follow_cursor"));
  addToggle(kDisplay, QStringLiteral(u"候选窗跟随光标"),
            QStringLiteral("enhanced_position"));
  addToggle(kDisplay, QStringLiteral(u"托盘图标"),
            QStringLiteral("display_tray_icon"));
  addToggle(kDisplay, QStringLiteral(u"滚轮翻页"),
            QStringLiteral("paging_on_scroll"));
  addToggle(kDisplay, QStringLiteral(u"点击候选窗捕获"),
            QStringLiteral("click_to_capture"));

  const QString kMouse = QStringLiteral(u"鼠标与渲染");
  addCombo(kMouse, QStringLiteral(u"悬停效果"), QStringLiteral("hover_type"),
           {option(QStringLiteral(u"无"), QStringLiteral("none")),
            option(QStringLiteral(u"半高亮"), QStringLiteral("semi_hilite")),
            option(QStringLiteral(u"高亮"), QStringLiteral("hilite"))});
  addCombo(kMouse, QStringLiteral(u"字体抗锯齿"),
           QStringLiteral("antialias_mode"),
           {option(QStringLiteral(u"默认"), QStringLiteral("default")),
            option(QStringLiteral(u"灰度"), QStringLiteral("grayscale")),
            option(QStringLiteral(u"ClearType"), QStringLiteral("cleartype")),
            option(QStringLiteral(u"强制灰度"), QStringLiteral("force_dword")),
            option(QStringLiteral(u"不做抗锯齿"), QStringLiteral("aliased"))});

  const QString kVertical = QStringLiteral(u"竖排文字");
  addToggle(kVertical, QStringLiteral(u"竖排显示候选"),
            QStringLiteral("vertical_text"));
  addToggle(kVertical, QStringLiteral(u"从左到右"),
            QStringLiteral("vertical_text_left_to_right"));
  addToggle(kVertical, QStringLiteral(u"自动换行"),
            QStringLiteral("vertical_text_with_wrap"));
  addToggle(kVertical, QStringLiteral(u"上下自动反向"),
            QStringLiteral("vertical_auto_reverse"));

  const QString kFormat = QStringLiteral(u"格式");
  addEdit(kFormat, QStringLiteral(u"序号格式"), QStringLiteral("label_format"),
          QStringLiteral(u"%s 即序号"));
  addEdit(kFormat, QStringLiteral(u"高亮标记"), QStringLiteral("mark_text"));

  const QString kTuning = QStringLiteral(u"布局微调");
  const struct {
    const char* key;
    const char* label;
    int lo, hi;
  } kTuningDefs[] = {
      {"corner_radius", "面板圆角", 0, 24},
      {"round_corner", "高亮圆角", 0, 24},
      {"border_width", "边框宽度", 0, 8},
      {"margin_x", "内边距", 0, 24},
      {"candidate_spacing", "候选间距", 0, 24},
      {"hilite_padding", "高亮内边距", 0, 24},
      {"shadow_radius", "阴影范围", 0, 32},
      {"shadow_offset_y", "阴影纵向偏移", -16, 16},
      {"shadow_offset_x", "阴影横向偏移", -16, 16},
      {"margin_y", "上下内边距", 0, 24},
      {"spacing", "行间距", 0, 32},
      {"hilite_spacing", "高亮与文字间距", 0, 24},
      {"hilite_padding_x", "高亮横向内边距", 0, 24},
      {"hilite_padding_y", "高亮纵向内边距", 0, 24},
      {"linespacing", "候选行高", 0, 32},
      {"min_width", "最小宽度", 0, 600},
      {"max_width", "最大宽度", 0, 600},
  };
  for (const auto& d : kTuningDefs)
    addSpin(kTuning, QString::fromUtf8(d.label),
            QStringLiteral("layout/") + QLatin1String(d.key), d.lo, d.hi);
}

int StyleControlModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : (int)rows_.size();
}

QVariant StyleControlModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= (int)rows_.size())
    return QVariant();
  const Row& r = rows_[index.row()];
  switch (role) {
    case KeyRole:
      return r.key;
    case KindRole:
      return r.kind;
    case SectionRole:
      return r.section;
    case LabelRole:
      return r.label;
    case HelperRole:
      return r.helper;
    case ValueRole:
      return r.value;
    case MinRole:
      return r.lo;
    case MaxRole:
      return r.hi;
    case OptionsRole:
      return r.options;
    case InitRole:
      return r.init;
    default:
      return QVariant();
  }
}

bool StyleControlModel::setData(const QModelIndex& index, const QVariant& value,
                                int role) {
  if (role != ValueRole || !index.isValid() || index.row() < 0 ||
      index.row() >= (int)rows_.size())
    return false;
  Row& r = rows_[index.row()];
  if (r.kind == QStringLiteral("toggle"))
    r.value = value.toBool();
  else if (r.kind == QStringLiteral("spin"))
    r.value = value.toInt();
  else
    r.value = value.toString();
  const QModelIndex idx = this->index(index.row());
  emit dataChanged(idx, idx, {ValueRole});
  emit valueChanged(r.key);
  return true;
}

Qt::ItemFlags StyleControlModel::flags(const QModelIndex& index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> StyleControlModel::roleNames() const {
  return {
      {KeyRole, "key"},         {KindRole, "kind"},
      {SectionRole, "section"}, {LabelRole, "label"},
      {HelperRole, "helper"},   {ValueRole, "value"},
      {MinRole, "min"},         {MaxRole, "max"},
      {OptionsRole, "options"}, {InitRole, "init"},
  };
}

QString StyleControlModel::asText(const Row& row) {
  if (row.kind == QStringLiteral("toggle"))
    return row.value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
  if (row.kind == QStringLiteral("spin"))
    return QString::number(row.value.toInt());
  return row.value.toString();
}

void StyleControlModel::load(UIStyleSettings& settings) {
  for (auto& r : rows_) {
    const std::string key = r.key.toStdString();
    const std::string fallback = r.fallback.toStdString();
    if (r.kind == QStringLiteral("toggle")) {
      r.value = settings.GetStyleBool(key, r.fallback == QStringLiteral("1"));
    } else if (r.kind == QStringLiteral("spin")) {
      r.value = settings.GetStyleInt(key, r.fallback.toInt());
    } else if (r.kind == QStringLiteral("edit")) {
      r.value = QString::fromStdString(settings.GetStyleString(key, fallback));
    } else {
      const QString v =
          QString::fromStdString(settings.GetStyleString(key, fallback));
      int i = indexOfOption(r.options, v);
      if (i < 0 && r.keepUnknown && !v.isEmpty()) {
        r.options.prepend(option(v, v));
        i = 0;
      }
      if (i >= 0)
        r.value = valueOf(r.options.at(i).toMap());
      else
        r.value = r.options.isEmpty() ? QString()
                                      : valueOf(r.options.first().toMap());
    }
    r.init = asText(r);
  }
  if (!rows_.empty())
    emit dataChanged(index(0), index((int)rows_.size() - 1));
}

bool StyleControlModel::save(UIStyleSettings& settings) {
  bool changed = false;
  for (auto& r : rows_) {
    const QString now = asText(r);
    if (now == r.init)
      continue;
    const std::string key = r.key.toStdString();
    if (r.kind == QStringLiteral("toggle"))
      settings.SetStyleBool(key, r.value.toBool());
    else if (r.kind == QStringLiteral("spin"))
      settings.SetStyleInt(key, r.value.toInt());
    else
      settings.SetStyleString(key, now.toStdString());
    changed = true;
  }
  return changed;
}

int StyleControlModel::intValue(const QString& key, int fallback) const {
  for (const auto& r : rows_)
    if (r.key == key)
      return r.value.toInt();
  return fallback;
}

QString StyleControlModel::stringValue(const QString& key) const {
  for (const auto& r : rows_)
    if (r.key == key)
      return r.value.toString();
  return QString();
}

void StyleControlModel::setComboOptions(const QString& key,
                                        const QVariantList& options) {
  for (int i = 0; i < (int)rows_.size(); ++i) {
    Row& r = rows_[i];
    if (r.key != key)
      continue;
    r.options = options;
    r.fallback = options.isEmpty() ? QString() : valueOf(options.first().toMap());
    const QModelIndex idx = index(i);
    emit dataChanged(idx, idx, {OptionsRole});
    return;
  }
}

StyleModel::StyleModel(QObject* parent) : QObject(parent) {
  styleControls_ = new StyleControlModel(this);
  connect(styleControls_, &StyleControlModel::valueChanged, this,
          [this](const QString& key) {
            // 预览只吃这两处圆角与字体
            if (key == QStringLiteral("layout/corner_radius") ||
                key == QStringLiteral("layout/round_corner") ||
                key == QStringLiteral("font_face"))
              updatePreview();
          });
}

QVariantList StyleModel::layoutOptions() const {
  return {option(QStringLiteral(u"竖排"), QStringLiteral("vertical")),
          option(QStringLiteral(u"横排"), QStringLiteral("horizontal"))};
}

QVariantMap StyleModel::customColors() const {
  QVariantMap out;
  for (const auto& kv : custom_)
    out.insert(QString::fromStdString(kv.first), (uint)kv.second);
  return out;
}

QStringList StyleModel::customColorKeys() const {
  QStringList out;
  for (const char* k : kCustomColorKeys)
    out << QString::fromLatin1(k);
  return out;
}

int StyleModel::customOpacity() const {
  const auto it = custom_.find("back_color");
  if (it == custom_.end())
    return 0;
  return (int)(it->second >> 24) * 100 / 255;
}

int StyleModel::indexOfScheme(const QString& id) const {
  for (int i = 0; i < schemes_.size(); ++i)
    if (valueOf(schemes_.at(i).toMap()) == id)
      return i;
  return -1;
}

void StyleModel::setColorScheme(const QString& value) {
  if (colorScheme_ == value)
    return;
  colorScheme_ = value;
  emit changed();
  updatePreview();
}

void StyleModel::setFontSize(int value) {
  if (fontSize_ == value)
    return;
  fontSize_ = value;
  emit changed();
  updatePreview();
}

void StyleModel::setLayout(const QString& value) {
  if (layout_ == value)
    return;
  layout_ = value;
  emit changed();
  updatePreview();
}

void StyleModel::onCustomEdited() {
  customDirty_ = true;
  // 编辑即选中「自定义」;首次编辑时列表尚无该条目,须先补上,
  // 否则预览一直画旧方案(滑动不透明度预览不跟随即此因)
  if (indexOfScheme(QStringLiteral("bangke_custom")) < 0) {
    schemes_.append(
        option(QStringLiteral(u"自定义"), QStringLiteral("bangke_custom")));
    emit schemesChanged();
  }
  colorScheme_ = QStringLiteral("bangke_custom");
  emit changed();
  emit customColorsChanged();
  updatePreview();
}

void StyleModel::setCustomColor(const QString& key, uint argb) {
  const std::string k = key.toStdString();
  if (custom_[k] == argb)
    return;
  custom_[k] = argb;
  onCustomEdited();
}

void StyleModel::setCustomOpacity(int percent) {
  // 只改 alpha,RGB 保留
  const unsigned cur = custom_["back_color"];
  custom_["back_color"] =
      ((unsigned)percent * 255u / 100u << 24) | (cur & 0x00FFFFFFu);
  onCustomEdited();
}

void StyleModel::resetCustomColors() {
  custom_ = initCustom_;
  customDirty_ = false;
  emit customColorsChanged();
  updatePreview();
}

void StyleModel::load() {
  if (loaded_)
    return;
  forceLoad();
}

void StyleModel::forceLoad() {
  settings_.Load();

  std::vector<ColorSchemeInfo> schemes;
  settings_.GetPresetColorSchemes(&schemes);
  activeScheme_ = QString::fromStdString(settings_.GetActiveColorScheme());

  schemes_.clear();
  for (auto& s : schemes)
    schemes_.append(option(QString::fromStdString(s.name),
                           QString::fromStdString(s.color_scheme_id)));
  const bool hasCustom = settings_.HasCustomScheme();
  if (hasCustom)
    schemes_.append(
        option(QStringLiteral(u"自定义"), QStringLiteral("bangke_custom")));
  emit schemesChanged();

  // 深色模式下拉首项「跟随浅色」数据为空串——服务端拿空名去查方案查不到,
  // 自然落回浅色,与"没设过"同义
  QVariantList darkOptions;
  darkOptions.append(option(QStringLiteral(u"跟随浅色"), QString()));
  for (auto& s : schemes)
    darkOptions.append(option(QString::fromStdString(s.name),
                              QString::fromStdString(s.color_scheme_id)));
  styleControls_->setComboOptions(QStringLiteral("color_scheme_dark"),
                                  darkOptions);

  const int index = indexOfScheme(activeScheme_);
  colorScheme_ = index >= 0 ? activeScheme_
                            : (schemes_.isEmpty() ? QString()
                                                  : valueOf(schemes_[0].toMap()));

  activeFontSize_ = settings_.GetFontSize(14);
  fontSize_ = activeFontSize_;
  activeHorizontal_ = settings_.GetHorizontal(false);
  layout_ = activeHorizontal_ ? QStringLiteral("horizontal")
                              : QStringLiteral("vertical");

  styleControls_->load(settings_);

  // 自定义配色初值:已有 bangke_custom 用之,否则从当前方案颜色起步
  const std::string baseScheme =
      hasCustom ? std::string("bangke_light") : activeScheme_.toStdString();
  auto base = settings_.GetSchemeColors(baseScheme);
  if (hasCustom) {
    for (const char* k : kCustomColorKeys)
      custom_[k] = settings_.GetCustomColor(k, base.count(k) ? base[k] : 0);
  } else {
    auto from = settings_.GetSchemeColors(activeScheme_.toStdString());
    for (const char* k : kCustomColorKeys)
      custom_[k] = from.count(k) ? from[k] : base[k];
  }
  initCustom_ = custom_;
  customDirty_ = false;
  emit customColorsChanged();

  updatePreview();
  loaded_ = true;
  emit changed();
}

void StyleModel::updatePreview() {
  const QString id = colorScheme_;
  std::map<std::string, unsigned int> colors;
  if (id == QStringLiteral("bangke_custom")) {
    // 基线共享浅色,叠加用户补丁(编辑中的 custom_ 即最新意图)
    colors = settings_.GetSchemeColors("bangke_light");
    for (const char* k : kCustomColorKeys) {
      const auto it = custom_.find(k);
      if (it != custom_.end())
        colors[k] = it->second;
    }
  } else if (!id.isEmpty()) {
    colors = settings_.GetSchemeColors(id.toStdString());
  }
  previewColors_.clear();
  for (const auto& kv : colors)
    previewColors_.insert(QString::fromStdString(kv.first), (uint)kv.second);
  previewHorizontal_ = (layout_ == QStringLiteral("horizontal"));
  previewPanelRadius_ =
      styleControls_->intValue(QStringLiteral("layout/corner_radius"), 10);
  previewHiliteRadius_ =
      styleControls_->intValue(QStringLiteral("layout/round_corner"), 6);
  // 字体也跟着配置走(空 = 跟随系统),字号用「外观」里的候选字号
  previewFontFace_ = styleControls_->stringValue(QStringLiteral("font_face"));
  previewFontPoint_ = fontSize_;
  emit previewChanged();
}

bool StyleModel::save() {
  // 没载过值的页没有「用户改动」可言:行表还停在兜底值,而每项的 init 是空串,
  // 逐项比较会把它们全判成「改过」,把默认值写进配置。曾经发生过——在 AI 页改一个开关
  // 后保存,saveAndDeploy 连带保存本页,label_font_point 被写成 0,候选序号就此消失。
  if (!loaded_)
    return false;
  bool changed = false;
  const QString scheme = colorScheme_;
  if (!scheme.isEmpty() && scheme != activeScheme_) {
    settings_.SelectColorScheme(scheme.toStdString());
    changed = true;
  }
  if (fontSize_ != activeFontSize_) {
    settings_.SetFontSize(fontSize_);
    changed = true;
  }
  const bool horizontal = (layout_ == QStringLiteral("horizontal"));
  if (horizontal != activeHorizontal_) {
    settings_.SetHorizontal(horizontal);
    changed = true;
  }
  if (styleControls_->save(settings_))
    changed = true;
  // 自定义配色:内容有变化才写。方案归属以下拉选择为准(选「自定义」时
  // scheme 已是 bangke_custom);曾在此会话编辑过自定义不构成切回的理由
  // ——否则会把用户随后显式选择的内置方案覆盖掉
  if (customDirty_ && custom_ != initCustom_) {
    for (const char* k : kCustomColorKeys)
      settings_.SetCustomColor(k, custom_[k]);
    changed = true;
  }
  if (!changed)
    return false;
  return settings_.Save();
}
