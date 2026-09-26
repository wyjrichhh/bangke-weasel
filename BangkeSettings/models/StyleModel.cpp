#include "StyleModel.h"

#include <QFontDatabase>

#include <algorithm>

#include "Levers.h"
#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

namespace {

QVariantMap option(const QString& label, const QString& value) {
  QVariantMap m;
  m.insert(QStringLiteral("label"), label);
  m.insert(QStringLiteral("value"), value);
  return m;
}

// 复刻 mac(BangkeTheme) 的配色 fallback:mac 方案大多不写 label/comment 键,
// 缺失时 mac 用自己的语义兜底 —— 序号 = (候选文字×2 + 面板背景)/3
// (blendColor,背景用 back_color;引擎 weasel 用透明的 candidate_back,
// 混出来恒等于文字色,与 mac 观感不符);注释不混合,直接用
// secondaryLabel(浅色 50% 黑 / 深色 55% 白)
unsigned BlendMac(unsigned fg, unsigned bg) {
  auto mix = [&](unsigned a, unsigned b) -> unsigned {
    return (a * 2 + b) / 3;
  };
  return (mix((fg >> 24) & 0xFF, (bg >> 24) & 0xFF) << 24) |
         (mix((fg >> 16) & 0xFF, (bg >> 16) & 0xFF) << 16) |
         (mix((fg >> 8) & 0xFF, (bg >> 8) & 0xFF) << 8) |
         mix(fg & 0xFF, bg & 0xFF);
}

void FillSchemeFallbacks(std::map<std::string, unsigned int>& c, bool dark) {
  auto has = [&](const char* k) { return c.count(k) > 0; };
  auto put = [&](const char* k, unsigned v) {
    if (!has(k))
      c[k] = v;
  };
  put("text_color", 0xFF000000);
  put("back_color", 0xFFFFFFFF);
  put("candidate_text_color", c["text_color"]);
  put("hilited_text_color", c["text_color"]);
  put("hilited_back_color", c["back_color"]);
  put("hilited_candidate_text_color", c["hilited_text_color"]);
  put("hilited_candidate_back_color", c["hilited_back_color"]);
  put("border_color", c["text_color"]);
  put("label_color", BlendMac(c["candidate_text_color"], c["back_color"]));
  put("hilited_label_color",
      BlendMac(c["hilited_candidate_text_color"],
               has("hilited_back_color") ? c["hilited_back_color"]
                                         : c["back_color"]));
  const unsigned secondary = dark ? 0x8CFFFFFFu : 0x7F000000u;
  put("comment_text_color", secondary);
  put("hilited_comment_text_color", secondary);
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
                                const QString& key, const QString& factory,
                                const QString& helper = QString()) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = QStringLiteral("toggle");
    r.helper = helper;
    r.fallback = QStringLiteral("0");
    r.factory = factory;
    r.value = factory == QStringLiteral("1");
    rows_.push_back(r);
  };
  // 滑杆(kind=slider)与浮点滑杆(kind=dslider);factory 同时是兜底值
  const auto addSlider = [this](const QString& section, const QString& label,
                                const QString& key, double lo, double hi,
                                const QString& factory, double step = 1,
                                const QString& helper = QString()) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = step == (int)step ? QStringLiteral("slider")
                               : QStringLiteral("dslider");
    r.helper = helper;
    r.lo = lo;
    r.hi = hi;
    r.step = step;
    r.fallback = factory;
    r.factory = factory;
    r.value = factory;
    rows_.push_back(r);
  };
  const auto addEdit = [this](const QString& section, const QString& label,
                              const QString& key, const QString& factory,
                              const QString& helper = QString()) {
    Row r;
    r.section = section;
    r.label = label;
    r.key = key;
    r.kind = QStringLiteral("edit");
    r.helper = helper;
    r.value = factory;
    r.fallback = factory;
    r.factory = factory;
    rows_.push_back(r);
  };
  const auto addCombo = [this](const QString& section, const QString& label,
                               const QString& key, const QVariantList& options,
                               const QString& factory,
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
    r.value = factory;
    r.fallback = factory;
    r.factory = factory;
    rows_.push_back(r);
  };

  // ---- 行表与 mac AppearanceSettingsPage 一一对应;出厂值与
  // data/weasel.yaml 的 style: 段一一对应,两处改动要一起做 ----

  const QString kColor = QStringLiteral(u"颜色");
  // 选项在 load 里按实际方案表刷新;空串 = 跟随浅色
  addCombo(kColor, QStringLiteral(u"深色配色方案"),
           QStringLiteral("color_scheme_dark"), QVariantList(), QString(),
           QStringLiteral(u"系统切到深色时改用它"));
  addSlider(kColor, QStringLiteral(u"背景不透明度"),
            QStringLiteral("alpha"), 0, 1, QStringLiteral("1.00"), 0.05);

  const QString kFont = QStringLiteral(u"字体");
  // 字体名从机器上装的字体里挑。首项是空值,与配置语义一致(空 = 跟随系统)
  QVariantList fontOptions;
  fontOptions.append(option(QStringLiteral(u"跟随系统"), QString()));
  for (const QString& family : QFontDatabase::families())
    fontOptions.append(option(family, family));
  addCombo(kFont, QStringLiteral(u"字体"), QStringLiteral("font_face"),
           fontOptions, QStringLiteral("Segoe UI, Microsoft YaHei"), QString(),
           /*keepUnknown=*/true);

  const QString kLayout = QStringLiteral(u"布局");
  addCombo(kLayout, QStringLiteral(u"文字方向"), QStringLiteral("vertical_text"),
           {option(QStringLiteral(u"横排"), QStringLiteral("false")),
            option(QStringLiteral(u"竖排"), QStringLiteral("true"))},
           QStringLiteral("false"));
  addSlider(kLayout, QStringLiteral(u"圆角半径"),
            QStringLiteral("layout/corner_radius"), 0, 30,
            QStringLiteral("9"));
  addSlider(kLayout, QStringLiteral(u"高亮圆角半径"),
            QStringLiteral("layout/round_corner"), 0, 30,
            QStringLiteral("7"));
  addSlider(kLayout, QStringLiteral(u"行距"),
            QStringLiteral("layout/candidate_spacing"), 0, 30,
            QStringLiteral("6"));
  addSlider(kLayout, QStringLiteral(u"预编辑间距"),
            QStringLiteral("layout/spacing"), 0, 30, QStringLiteral("8"));
  addSlider(kLayout, QStringLiteral(u"阴影大小"),
            QStringLiteral("layout/shadow_radius"), 0, 30,
            QStringLiteral("2"));
  addSlider(kLayout, QStringLiteral(u"边框宽度"),
            QStringLiteral("layout/border_width"), 0, 20, QStringLiteral("0"));

  const QString kDisplay = QStringLiteral(u"显示");
  addToggle(kDisplay, QStringLiteral(u"在输入框内显示拼音"),
            QStringLiteral("inline_preedit"), QStringLiteral("1"));
  // 以下为 Windows 引擎支持、mac 设置页没有对应项的补充行
  // (mac 的 inline_candidate/mutual_exclusive/memorize_size/show_paging
  //  引擎不认,不设)。出厂值 = 引擎的 fallback 默认
  addToggle(kDisplay, QStringLiteral(u"候选窗跟随光标"),
            QStringLiteral("enhanced_position"), QStringLiteral("1"));
  addToggle(kDisplay, QStringLiteral(u"英文提示跟随光标"),
            QStringLiteral("ascii_tip_follow_cursor"), QStringLiteral("0"));
  addToggle(kDisplay, QStringLiteral(u"滚轮翻页"),
            QStringLiteral("paging_on_scroll"), QStringLiteral("0"));
  addToggle(kDisplay, QStringLiteral(u"点击候选窗捕获"),
            QStringLiteral("click_to_capture"), QStringLiteral("0"));
  addCombo(kDisplay, QStringLiteral(u"编码显示方式"),
           QStringLiteral("preedit_type"),
           {option(QStringLiteral(u"只显示已选部分"), QStringLiteral("composition")),
            option(QStringLiteral(u"预览首个候选"), QStringLiteral("preview")),
            option(QStringLiteral(u"预览整句"), QStringLiteral("preview_all"))},
           QStringLiteral("composition"));

  const QString kFormat = QStringLiteral(u"格式");
  addEdit(kFormat, QStringLiteral(u"候选格式"), QStringLiteral("label_format"),
          QStringLiteral("%s."), QStringLiteral(u"%s 即序号"));
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
    case StepRole:
      return r.step;
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
  else if (r.kind == QStringLiteral("dslider"))
    r.value = value.toDouble();
  else if (r.kind == QStringLiteral("slider"))
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
      {StepRole, "step"},       {OptionsRole, "options"},
      {InitRole, "init"},
  };
}

QString StyleControlModel::asText(const Row& row) {
  if (row.kind == QStringLiteral("toggle"))
    return row.value.toBool() ? QStringLiteral("1") : QStringLiteral("0");
  if (row.kind == QStringLiteral("dslider"))
    return QString::number(row.value.toDouble(), 'f', 2);
  if (row.kind == QStringLiteral("slider"))
    return QString::number(row.value.toInt());
  return row.value.toString();
}

void StyleControlModel::load(UIStyleSettings& settings) {
  for (auto& r : rows_) {
    const std::string key = r.key.toStdString();
    const std::string fallback = r.fallback.toStdString();
    if (r.kind == QStringLiteral("toggle")) {
      r.value = settings.GetStyleBool(key, r.fallback == QStringLiteral("1"));
    } else if (r.kind == QStringLiteral("dslider")) {
      r.value = settings.GetStyleDouble(key, r.fallback.toDouble());
    } else if (r.kind == QStringLiteral("slider")) {
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
    else if (r.kind == QStringLiteral("dslider"))
      settings.SetStyleDouble(key, r.value.toDouble());
    else if (r.kind == QStringLiteral("slider"))
      settings.SetStyleInt(key, r.value.toInt());
    else
      settings.SetStyleString(key, now.toStdString());
    changed = true;
  }
  return changed;
}

void StyleControlModel::resetDefaults() {
  for (size_t i = 0; i < rows_.size(); ++i) {
    Row& r = rows_[i];
    if (r.kind == QStringLiteral("toggle"))
      r.value = r.factory == QStringLiteral("1");
    else if (r.kind == QStringLiteral("dslider"))
      r.value = r.factory.toDouble();
    else if (r.kind == QStringLiteral("slider"))
      r.value = r.factory.toInt();
    else
      r.value = r.factory;
  }
  if (!rows_.empty())
    emit dataChanged(index(0), index((int)rows_.size() - 1));
}

int StyleControlModel::intValue(const QString& key, int fallback) const {
  for (const auto& r : rows_)
    if (r.key == key)
      return r.value.toInt();
  return fallback;
}

double StyleControlModel::doubleValue(const QString& key,
                                      double fallback) const {
  for (const auto& r : rows_)
    if (r.key == key)
      return r.value.toDouble();
  return fallback;
}

bool StyleControlModel::boolValue(const QString& key, bool fallback) const {
  for (const auto& r : rows_)
    if (r.key == key)
      return r.value.toBool();
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
  // 任何行变动都可能影响预览(圆角/间距/透明度/深色方案/内联拼音),统一重算
  connect(styleControls_, &StyleControlModel::valueChanged, this,
          &StyleModel::updatePreview);
}

QVariantList StyleModel::layoutOptions() const {
  return {option(QStringLiteral(u"多行"), QStringLiteral("vertical")),
          option(QStringLiteral(u"单行"), QStringLiteral("horizontal"))};
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

void StyleModel::load() {
  if (loaded_)
    return;
  settings_.Load();
  // 每页候选数决定预览条数(default.custom.yaml,与 GeneralModel 同源)
  pageSize_ = 5;
  if (RimeLeversApi* api = leversApi()) {
    RimeCustomSettings* def =
        api->custom_settings_init("default", "Bangke::StyleModel");
    if (api->load_settings(def)) {
      RimeConfig config = {0};
      if (api->settings_get_config(def, &config)) {
        int ps = 5;
        if (rime_get_api()->config_get_int(&config, "menu/page_size", &ps) &&
            ps > 0 && ps <= 9)
          pageSize_ = ps;
      }
    }
  }

  std::vector<ColorSchemeInfo> schemes;
  settings_.GetPresetColorSchemes(&schemes);
  activeScheme_ = QString::fromStdString(settings_.GetActiveColorScheme());

  schemes_.clear();
  for (auto& s : schemes)
    schemes_.append(option(QString::fromStdString(s.name),
                           QString::fromStdString(s.color_scheme_id)));
  // 存量 bangke_custom 仍可选用(引擎与迁移都认),只是不再提供编辑器
  if (settings_.HasCustomScheme())
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

  activeFontSize_ = settings_.GetFontSize(16);
  fontSize_ = activeFontSize_;
  activeHorizontal_ = settings_.GetHorizontal(false);
  layout_ = activeHorizontal_ ? QStringLiteral("horizontal")
                              : QStringLiteral("vertical");

  styleControls_->load(settings_);

  // 先置 loaded_ 再刷新预览:updatePreview 有「未载入即返回」守卫,
  // 顺序反了会让首次预览拿到空色表(QML 侧全走 fallback 色)
  loaded_ = true;
  updatePreview();
  emit changed();
}

void StyleModel::updatePreview() {
  if (!loaded_)
    return;
  // 浅色块 = 当前方案;深色块 = 深色配色方案,空 = 跟随浅色。
  // native 按外观各有一套,其余方案两块同色(方案本身就是单一色板)
  const QString lightId = colorScheme_;
  QString darkId = styleControls_->stringValue(
      QStringLiteral("color_scheme_dark"));
  if (darkId.isEmpty())
    darkId = lightId;
  previewLight_.clear();
  auto light = settings_.GetSchemeColors(lightId.toStdString(), false);
  FillSchemeFallbacks(light, /*dark=*/false);
  for (const auto& kv : light)
    previewLight_.insert(QString::fromStdString(kv.first), (uint)kv.second);
  previewDark_.clear();
  auto dark = settings_.GetSchemeColors(darkId.toStdString(), true);
  FillSchemeFallbacks(dark, /*dark=*/true);
  for (const auto& kv : dark)
    previewDark_.insert(QString::fromStdString(kv.first), (uint)kv.second);

  const bool horizontal = layout_ == QStringLiteral("horizontal");
  previewHorizontal_ = horizontal;
  // margin 经引擎 fixup = max(|margin|, hilite_padding),预览按同式
  const int marginX = std::max(
      styleControls_->intValue(QStringLiteral("layout/margin_x"), 0),
      styleControls_->intValue(QStringLiteral("layout/hilite_padding_x"), 0));
  const int marginY = std::max(
      styleControls_->intValue(QStringLiteral("layout/margin_y"), 0),
      styleControls_->intValue(QStringLiteral("layout/hilite_padding_y"), 0));
  // 候选窗字体:取 font_face 第一族;权重后缀与 DirectWriteResources 同解,
  // QML 侧把名字映到 Font.weight
  const QString faceRaw =
      styleControls_->stringValue(QStringLiteral("font_face"));
  QString family = faceRaw.section(QLatin1Char(','), 0, 0).trimmed();
  QString weight;
  const int colon = family.indexOf(QLatin1Char(':'));
  if (colon >= 0) {
    weight = family.mid(colon + 1).toLower();
    family = family.left(colon);
  }
  // 序号字号:方案自带(简约白/冷漠/浮尘/沙漠夜)优先,否则全局值;
  // 注释字号不再有 UI 行,读合并值
  int labelPoint = settings_.GetStyleInt("label_font_point", 16);
  const int schemeLabel = settings_.GetSchemeLabelFontPoint(
      lightId.toStdString());
  if (schemeLabel > 0)
    labelPoint = schemeLabel;
  previewMetrics_ = QVariantMap{
      {QStringLiteral("cornerRadius"),
       styleControls_->intValue(QStringLiteral("layout/corner_radius"), 9)},
      {QStringLiteral("hiliteRadius"),
       styleControls_->intValue(QStringLiteral("layout/round_corner"), 0)},
      {QStringLiteral("marginX"), marginX},
      {QStringLiteral("marginY"), marginY},
      // 高亮纵向外扩与边框:引擎只有 border_width>0 才画边框,
      // 预览与引擎同判,不额外加描边
      {QStringLiteral("hilitePaddingY"),
       settings_.GetStyleInt("layout/hilite_padding_y", 3)},
      {QStringLiteral("borderWidth"),
       settings_.GetStyleInt("layout/border_width", 0)},
      {QStringLiteral("pageSize"), pageSize_},
      {QStringLiteral("candidateSpacing"),
       styleControls_->intValue(QStringLiteral("layout/candidate_spacing"), 5)},
      {QStringLiteral("spacing"),
       styleControls_->intValue(QStringLiteral("layout/spacing"), 8)},
      {QStringLiteral("hiliteSpacing"),
       settings_.GetStyleInt("layout/hilite_spacing", 5)},
      {QStringLiteral("fontPoint"), fontSize_},
      {QStringLiteral("labelPoint"), labelPoint},
      {QStringLiteral("commentPoint"),
       settings_.GetStyleInt("comment_font_point", 16)},
      {QStringLiteral("fontFamily"), family},
      {QStringLiteral("fontWeight"), weight},
      {QStringLiteral("horizontal"), horizontal},
      {QStringLiteral("verticalText"),
       styleControls_->stringValue(QStringLiteral("vertical_text")) ==
           QStringLiteral("true")},
      {QStringLiteral("inlinePreedit"),
       styleControls_->boolValue(QStringLiteral("inline_preedit"), true)},
      {QStringLiteral("alpha"),
       styleControls_->doubleValue(QStringLiteral("alpha"), 1.0)},
  };
  emit previewChanged();
}

void StyleModel::resetDefaults() {
  colorScheme_ = QStringLiteral("native");
  fontSize_ = 16;
  layout_ = QStringLiteral("vertical");
  styleControls_->resetDefaults();
  emit changed();
  updatePreview();
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
  if (!changed)
    return false;
  if (!settings_.Save())
    return false;
  // 保存成功后同步快照,否则再次保存会把同一改动再写一遍
  activeScheme_ = scheme;
  activeFontSize_ = fontSize_;
  activeHorizontal_ = horizontal;
  return true;
}
