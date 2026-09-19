#include "StylePage.h"

#include <QAbstractButton>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "Ui.h"

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

QIcon colorSwatch(unsigned argb) {
  QPixmap pm(16, 16);
  pm.fill(QColor((QRgb)argb));
  return QIcon(pm);
}
}  // namespace

// 迷你候选窗预览:按方案原始颜色直接绘制,与真面板同数据源
// (共享 weasel.yaml + 用户补丁),不再依赖 preview\*.png
class SchemePreviewWidget : public QWidget {
 public:
  SchemePreviewWidget(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(380, 190);
  }

  void setScheme(const std::map<std::string, unsigned int>& colors,
                 bool horizontal, int panelRadius, int hiliteRadius) {
    colors_ = colors;
    horizontal_ = horizontal;
    panelRadius_ = panelRadius;
    hiliteRadius_ = hiliteRadius;
    update();
  }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    auto color = [this](const char* key, QRgb def) {
      auto it = colors_.find(key);
      return it == colors_.end() ? def : (QRgb)it->second;
    };
    const QColor back(color("back_color", 0xFAFFFFFF));
    const QColor border(color("border_color", 0xFFE3E3E8));
    const QColor shadow(color("shadow_color", 0x28000000));
    const QColor text(color("text_color", 0xFF1F2937));
    const QColor candText(color("candidate_text_color", 0xFF2F3437));
    const QColor label(color("label_color", 0xFF9CA3AF));
    const QColor hiliteText(
        color("hilited_candidate_text_color", 0xFF16202B));
    const QColor hiliteBack(
        color("hilited_candidate_back_color", 0xFFA8C7F0));
    const QColor hiliteLabel(color("hilited_label_color", 0xFF51709A));

    // 阴影余量 + 面板矩形(圆角随布局设置联动)
    const QRectF panel = rect().adjusted(16, 10, -16, -16);
    const double pr = panelRadius_;
    p.setPen(Qt::NoPen);
    for (int i = 3; i >= 1; --i) {
      QColor s = shadow;
      s.setAlpha(s.alpha() / (4 - i));
      p.setBrush(s);
      p.drawRoundedRect(panel.translated(0, i * 2).adjusted(-1, -1, 1, 1),
                        pr + 1 + i, pr + 1 + i);
    }
    p.setBrush(back);
    p.setPen(QPen(border, 1));
    p.drawRoundedRect(panel, pr, pr);
    p.setPen(Qt::NoPen);

    QFont f = font();
    f.setFamily(QStringLiteral("Microsoft YaHei UI"));
    f.setPointSize(9);
    p.setFont(f);

    const Cand cands[] = {
        {QStringLiteral("1."), QStringLiteral(u"蚌壳"), QStringLiteral(u"预测")},
        {QStringLiteral("2."), QStringLiteral(u"帮客"), QString()},
        {QStringLiteral("3."), QStringLiteral(u"绑客"), QString()},
    };

    p.setPen(text);
    p.drawText(QRectF(panel.left() + 12, panel.top() + 8,
                      panel.width() - 24, 20),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral(u"bang'ke pinyin"));

    if (horizontal_) {
      double x = panel.left() + 10;
      for (int i = 0; i < 3; ++i) {
        const double w = 74 + (cands[i].comment.isEmpty() ? 0 : 46);
        drawCandidate(p, QRectF(x, panel.top() + 32, w, 30), cands[i], i == 0,
                      label, candText, hiliteLabel, hiliteText, hiliteBack);
        x += w + 8;
      }
    } else {
      double y = panel.top() + 30;
      for (int i = 0; i < 3; ++i) {
        drawCandidate(p, QRectF(panel.left() + 8, y, panel.width() - 16, 30),
                      cands[i], i == 0, label, candText, hiliteLabel,
                      hiliteText, hiliteBack);
        y += 34;
      }
    }
  }

 private:
  struct Cand {
    QString label, text, comment;
  };

  void drawCandidate(QPainter& p, const QRectF& box, const Cand& c,
                     bool hilited, const QColor& label, const QColor& text,
                     const QColor& hiliteLabel, const QColor& hiliteText,
                     const QColor& hiliteBack) {
    if (hilited) {
      p.setPen(Qt::NoPen);
      p.setBrush(hiliteBack);
      p.drawRoundedRect(box, hiliteRadius_, hiliteRadius_);
    }
    p.setPen(hilited ? hiliteLabel : label);
    p.drawText(QRectF(box.left() + 8, box.top(), 22, box.height()),
               Qt::AlignLeft | Qt::AlignVCenter, c.label);
    p.setPen(hilited ? hiliteText : text);
    p.drawText(QRectF(box.left() + 30, box.top(), 50, box.height()),
               Qt::AlignLeft | Qt::AlignVCenter, c.text);
    if (!c.comment.isEmpty()) {
      p.setPen(hilited ? hiliteLabel : label);
      p.drawText(QRectF(box.left() + 82, box.top(), 60, box.height()),
                 Qt::AlignLeft | Qt::AlignVCenter, c.comment);
    }
  }

  std::map<std::string, unsigned int> colors_;
  bool horizontal_ = false;
  int panelRadius_ = 10;
  int hiliteRadius_ = 6;
};

StylePage::StylePage(QWidget* parent) : QWidget(parent) {
  schemeCombo_ = new QComboBox(this);
  fontSize_ = new QSpinBox(this);
  fontSize_->setRange(9, 36);

  layoutCombo_ = new QComboBox(this);
  layoutCombo_->addItem(QStringLiteral(u"竖排"), QStringLiteral("vertical"));
  layoutCombo_->addItem(QStringLiteral(u"横排"), QStringLiteral("horizontal"));

  schemeCombo_->setFixedWidth(170);
  fontSize_->setFixedWidth(96);
  layoutCombo_->setFixedWidth(110);
  auto* form = makeSettingRows({
      makeSettingRow(QStringLiteral(u"配色方案"), schemeCombo_),
      makeSettingRow(QStringLiteral(u"字体大小"), fontSize_),
      makeSettingRow(QStringLiteral(u"候选窗排列"), layoutCombo_),
  });

  preview_ = new SchemePreviewWidget(this);

  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 12, 16, 12);
  layout->setSpacing(10);
  layout->addWidget(makeCard(QStringLiteral(u"外观"), form));

  // ---- 布局微调(读 = 补丁 ⊕ 共享默认) ----
  struct SpinDef {
    const char* key;
    const char* label;
    int lo, hi, fb;
  };
  const SpinDef defs[] = {
      {"corner_radius", "面板圆角", 0, 24, 12},
      {"round_corner", "高亮圆角", 0, 24, 17},
      {"border_width", "边框宽度", 0, 8, 0},
      {"margin_x", "内边距", 0, 24, 12},
      {"candidate_spacing", "候选间距", 0, 24, 12},
      {"hilite_padding", "高亮内边距", 0, 24, 8},
      {"shadow_radius", "阴影范围", 0, 32, 10},
      {"shadow_offset_y", "阴影纵向偏移", -16, 16, 3},
      {"shadow_offset_x", "阴影横向偏移", -16, 16, 0},
      {"margin_y", "上下内边距", 0, 24, 12},
      {"spacing", "行间距", 0, 32, 12},
      {"hilite_spacing", "高亮与文字间距", 0, 24, 6},
      {"hilite_padding_x", "高亮横向内边距", 0, 24, 8},
      {"hilite_padding_y", "高亮纵向内边距", 0, 24, 8},
      {"linespacing", "候选行高", 0, 32, 0},
      {"min_width", "最小宽度", 0, 600, 0},
      {"max_width", "最大宽度", 0, 600, 0},
  };
  std::vector<QWidget*> tuningRows;
  for (const auto& d : defs) {
    auto* spin = new QSpinBox(this);
    spin->setRange(d.lo, d.hi);
    spin->setFixedWidth(96);
    layoutSpins_.push_back({spin, d.key});
    tuningRows.push_back(
        makeSettingRow(QString::fromUtf8(d.label), spin));
  }
  layout->addWidget(makeCard(QStringLiteral(u"布局微调"), makeSettingRows(tuningRows)));

  // ---- 自定义配色 ----
  backColorBtn_ = colorButton("back_color", QStringLiteral(u"背景颜色"));
  hiliteColorBtn_ =
      colorButton("hilited_candidate_back_color", QStringLiteral(u"高亮颜色"));
  textColorBtn_ = colorButton("text_color", QStringLiteral(u"文字颜色"));

  opacitySlider_ = new QSlider(Qt::Horizontal, this);
  opacitySlider_->setFixedWidth(180);
  opacitySlider_->setRange(55, 100);
  opacitySlider_->setValue(95);
  connect(opacitySlider_, &QSlider::valueChanged, this, [this](int pct) {
    // 只改 alpha,RGB 保留
    const unsigned cur = custom_["back_color"];
    custom_["back_color"] =
        ((unsigned)pct * 255u / 100u << 24) | (cur & 0x00FFFFFFu);
    backColorBtn_->setIcon(colorSwatch(custom_["back_color"]));
    onCustomEdited();
  });

  auto* customCard = new QVBoxLayout();
  customCard->setSpacing(8);
  customCard->addLayout(makeSettingRows({
      makeSettingRow(QStringLiteral(u"背景颜色"), backColorBtn_),
      makeSettingRow(QStringLiteral(u"高亮颜色"), hiliteColorBtn_),
      makeSettingRow(QStringLiteral(u"文字颜色"), textColorBtn_),
      makeSettingRow(QStringLiteral(u"背景不透明度"), opacitySlider_),
  }));
  auto* hint = new QLabel(
      QStringLiteral(u"调整任一颜色或透明度后,将以「自定义」配色生效。"), this);
  hint->setObjectName(QStringLiteral("hint"));
  customCard->addWidget(hint);
  layout->addWidget(makeCard(QStringLiteral(u"自定义配色"), customCard));

  // ---- 字体 ----
  auto* fontFace = new QLineEdit(this);
  fontFace->setFixedWidth(220);
  fontFace->setPlaceholderText(QStringLiteral(u"留空跟随系统"));
  auto* labelPoint = new QSpinBox(this);
  labelPoint->setRange(0, 48);
  auto* commentPoint = new QSpinBox(this);
  commentPoint->setRange(0, 48);
  auto* abbrevLen = new QSpinBox(this);
  abbrevLen->setRange(0, 20);
  for (auto* s : {labelPoint, commentPoint, abbrevLen})
    s->setFixedWidth(96);
  layout->insertWidget(
      1, makeCard(QStringLiteral(u"字体"),
                  makeSettingRows({
                      makeSettingRow(QStringLiteral(u"字体"), fontFace),
                      makeSettingRow(QStringLiteral(u"序号字号"), labelPoint,
                                     QStringLiteral(u"0 = 不显示序号")),
                      makeSettingRow(QStringLiteral(u"注释字号"), commentPoint,
                                     QStringLiteral(u"0 = 不显示注释")),
                      makeSettingRow(QStringLiteral(u"候选缩写长度"), abbrevLen,
                                     QStringLiteral(u"0 = 不缩写")),
                  })));
  bindText("font_face", fontFace);
  bindInt("label_font_point", labelPoint);
  bindInt("comment_font_point", commentPoint);
  bindInt("candidate_abbreviate_length", abbrevLen);

  // ---- 深色配色 ----
  darkSchemeCombo_ = new QComboBox(this);
  darkSchemeCombo_->setFixedWidth(170);
  layout->insertWidget(
      2, makeCard(QStringLiteral(u"深色模式"),
                  makeSettingRows({
                      makeSettingRow(QStringLiteral(u"深色配色方案"),
                                     darkSchemeCombo_,
                                     QStringLiteral(u"系统切到深色时改用它")),
                  })));
  bindChoice("color_scheme_dark", darkSchemeCombo_);

  // ---- 显示 ----
  auto* inlinePreedit = new ToggleSwitch;
  auto* asciiTipFollow = new ToggleSwitch;
  auto* enhancedPosition = new ToggleSwitch;
  auto* trayIcon = new ToggleSwitch;
  auto* pagingOnScroll = new ToggleSwitch;
  auto* clickToCapture = new ToggleSwitch;
  auto* preeditType = new QComboBox(this);
  preeditType->setFixedWidth(150);
  preeditType->addItem(QStringLiteral(u"只显示已选部分"),
                       QStringLiteral("composition"));
  preeditType->addItem(QStringLiteral(u"预览首个候选"), QStringLiteral("preview"));
  preeditType->addItem(QStringLiteral(u"预览整句"), QStringLiteral("preview_all"));
  layout->insertWidget(
      3, makeCard(QStringLiteral(u"显示"),
                  makeSettingRows({
                      makeSettingRow(QStringLiteral(u"在输入框中显示编码"),
                                     inlinePreedit),
                      makeSettingRow(QStringLiteral(u"编码显示方式"), preeditType),
                      makeSettingRow(QStringLiteral(u"英文提示跟随光标"),
                                     asciiTipFollow),
                      makeSettingRow(QStringLiteral(u"候选窗跟随光标"),
                                     enhancedPosition),
                      makeSettingRow(QStringLiteral(u"托盘图标"), trayIcon),
                      makeSettingRow(QStringLiteral(u"滚轮翻页"), pagingOnScroll),
                      makeSettingRow(QStringLiteral(u"点击候选窗捕获"), clickToCapture),
                  })));
  bindBool("inline_preedit", inlinePreedit);
  bindChoice("preedit_type", preeditType);
  bindBool("ascii_tip_follow_cursor", asciiTipFollow);
  bindBool("enhanced_position", enhancedPosition);
  bindBool("display_tray_icon", trayIcon);
  bindBool("paging_on_scroll", pagingOnScroll);
  bindBool("click_to_capture", clickToCapture);

  // ---- 鼠标与渲染 ----
  auto* hoverType = new QComboBox(this);
  hoverType->setFixedWidth(150);
  hoverType->addItem(QStringLiteral(u"无"), QStringLiteral("none"));
  hoverType->addItem(QStringLiteral(u"半高亮"), QStringLiteral("semi_hilite"));
  hoverType->addItem(QStringLiteral(u"高亮"), QStringLiteral("hilite"));
  auto* antialias = new QComboBox(this);
  antialias->setFixedWidth(150);
  antialias->addItem(QStringLiteral(u"默认"), QStringLiteral("default"));
  antialias->addItem(QStringLiteral(u"灰度"), QStringLiteral("grayscale"));
  antialias->addItem(QStringLiteral(u"ClearType"), QStringLiteral("cleartype"));
  antialias->addItem(QStringLiteral(u"强制灰度"), QStringLiteral("force_dword"));
  antialias->addItem(QStringLiteral(u"不做抗锯齿"), QStringLiteral("aliased"));
  layout->insertWidget(
      4, makeCard(QStringLiteral(u"鼠标与渲染"),
                  makeSettingRows({
                      makeSettingRow(QStringLiteral(u"悬停效果"), hoverType),
                      makeSettingRow(QStringLiteral(u"字体抗锯齿"), antialias),
                  })));
  bindChoice("hover_type", hoverType);
  bindChoice("antialias_mode", antialias);

  // ---- 竖排文字 ----
  auto* verticalText = new ToggleSwitch;
  auto* verticalLtr = new ToggleSwitch;
  auto* verticalWrap = new ToggleSwitch;
  auto* verticalAutoReverse = new ToggleSwitch;
  layout->insertWidget(
      5, makeCard(QStringLiteral(u"竖排文字"),
                  makeSettingRows({
                      makeSettingRow(QStringLiteral(u"竖排显示候选"),
                                     verticalText),
                      makeSettingRow(QStringLiteral(u"从左到右"), verticalLtr),
                      makeSettingRow(QStringLiteral(u"自动换行"), verticalWrap),
                      makeSettingRow(QStringLiteral(u"上下自动反向"),
                                     verticalAutoReverse),
                  })));
  bindBool("vertical_text", verticalText);
  bindBool("vertical_text_left_to_right", verticalLtr);
  bindBool("vertical_text_with_wrap", verticalWrap);
  bindBool("vertical_auto_reverse", verticalAutoReverse);

  // ---- 格式 ----
  auto* labelFormat = new QLineEdit(this);
  labelFormat->setFixedWidth(220);
  auto* markText = new QLineEdit(this);
  markText->setFixedWidth(220);
  layout->insertWidget(
      6, makeCard(QStringLiteral(u"格式"),
                  makeSettingRows({
                      makeSettingRow(QStringLiteral(u"序号格式"), labelFormat,
                                     QStringLiteral(u"%s 即序号")),
                      makeSettingRow(QStringLiteral(u"高亮标记"), markText),
                  })));
  bindText("label_format", labelFormat);
  bindText("mark_text", markText);

  auto* prevForm = new QVBoxLayout();
  prevForm->addWidget(preview_);
  layout->addWidget(makeCard(QStringLiteral(u"预览"), prevForm));
  layout->addStretch(1);

  connect(schemeCombo_, &QComboBox::currentIndexChanged, this,
          &StylePage::updatePreview);
  connect(layoutCombo_, &QComboBox::currentIndexChanged, this,
          &StylePage::updatePreview);
}

QPushButton* StylePage::colorButton(const char* key, const QString& label) {
  auto* btn = new QPushButton(label, this);
  connect(btn, &QPushButton::clicked, this, [this, key, label] {
    const QColor current((QRgb)custom_[key]);
    const QColor picked = QColorDialog::getColor(
        current, this, label, QColorDialog::ShowAlphaChannel);
    if (!picked.isValid() || picked.rgba() == (QRgb)custom_[key])
      return;
    custom_[key] = picked.rgba();
    btnRefresh(key);
    onCustomEdited();
  });
  return btn;
}

void StylePage::btnRefresh(const char* key) {
  QPushButton* btn = key == std::string("back_color")             ? backColorBtn_
                     : key == std::string("hilited_candidate_back_color")
                         ? hiliteColorBtn_
                         : textColorBtn_;
  if (btn)
    btn->setIcon(colorSwatch(custom_[key]));
}

void StylePage::onCustomEdited() {
  customDirty_ = true;
  // 编辑即选中「自定义」;首次编辑时下拉尚无该条目,须先补上,
  // 否则预览一直画旧方案(滑动不透明度预览不跟随即此因)
  int idx = schemeCombo_->findData(QStringLiteral("bangke_custom"));
  if (idx < 0) {
    schemeCombo_->blockSignals(true);
    schemeCombo_->addItem(QStringLiteral(u"自定义"),
                           QStringLiteral("bangke_custom"));
    schemeCombo_->blockSignals(false);
    idx = schemeCombo_->count() - 1;
  }
  schemeCombo_->setCurrentIndex(idx);
  updatePreview();
}

void StylePage::bindBool(const char* key, QAbstractButton* w) {
  styleControls_.push_back({StyleControl::Bool, key, w, {}});
}

void StylePage::bindInt(const char* key, QSpinBox* w) {
  styleControls_.push_back({StyleControl::Int, key, w, {}});
}

void StylePage::bindText(const char* key, QLineEdit* w) {
  styleControls_.push_back({StyleControl::Text, key, w, {}});
}

void StylePage::bindChoice(const char* key, QComboBox* w) {
  styleControls_.push_back({StyleControl::Choice, key, w, {}});
}

void StylePage::load() {
  if (loaded_)
    return;
  forceLoad();
}

void StylePage::forceLoad() {
  settings_.Load();

  std::vector<ColorSchemeInfo> schemes;
  settings_.GetPresetColorSchemes(&schemes);
  activeScheme_ = settings_.GetActiveColorScheme();

  schemeCombo_->blockSignals(true);
  schemeCombo_->clear();
  for (auto& s : schemes)
    schemeCombo_->addItem(QString::fromStdString(s.name),
                          QString::fromStdString(s.color_scheme_id));
  const bool hasCustom = settings_.HasCustomScheme();
  if (hasCustom)
    schemeCombo_->addItem(QStringLiteral(u"自定义"),
                           QStringLiteral("bangke_custom"));
  schemeCombo_->blockSignals(false);

  int index = schemeCombo_->findData(QString::fromStdString(activeScheme_));
  schemeCombo_->setCurrentIndex(index >= 0 ? index : 0);

  darkSchemeCombo_->blockSignals(true);
  darkSchemeCombo_->clear();
  darkSchemeCombo_->addItem(QStringLiteral(u"跟随浅色"), QStringLiteral(""));
  for (auto& s : schemes)
    darkSchemeCombo_->addItem(QString::fromStdString(s.name),
                              QString::fromStdString(s.color_scheme_id));
  darkSchemeCombo_->blockSignals(false);

  activeFontSize_ = settings_.GetFontSize(14);
  fontSize_->setValue(activeFontSize_);
  activeHorizontal_ = settings_.GetHorizontal(false);
  layoutCombo_->setCurrentIndex(activeHorizontal_ ? 1 : 0);

  for (auto& ls : layoutSpins_) {
    const int v = settings_.GetLayoutInt(ls.key, 0);
    ls.spin->setValue(v);
    initLayout_[ls.key] = v;
  }

  // 通用绑定:读 = 补丁 ⊕ 共享默认,补丁与共享都没有就用控件的构造默认值
  for (auto& c : styleControls_) {
    switch (c.kind) {
      case StyleControl::Bool: {
        auto* box = static_cast<QAbstractButton*>(c.widget);
        const bool v = settings_.GetStyleBool(c.key, box->isChecked());
        box->setChecked(v);
        c.init = v ? "1" : "0";
        break;
      }
      case StyleControl::Int: {
        auto* spin = static_cast<QSpinBox*>(c.widget);
        const int v = settings_.GetStyleInt(c.key, spin->value());
        spin->setValue(v);
        c.init = std::to_string(v);
        break;
      }
      case StyleControl::Text: {
        auto* edit = static_cast<QLineEdit*>(c.widget);
        edit->setText(QString::fromStdString(settings_.GetStyleString(
            c.key, edit->text().toStdString())));
        c.init = edit->text().toStdString();
        break;
      }
      case StyleControl::Choice: {
        auto* combo = static_cast<QComboBox*>(c.widget);
        const QString v = QString::fromStdString(settings_.GetStyleString(
            c.key, combo->currentData().toString().toStdString()));
        const int i = combo->findData(v);
        combo->setCurrentIndex(i >= 0 ? i : 0);
        c.init = combo->currentData().toString().toStdString();
        break;
      }
    }
  }

  // 自定义配色初值:已有 bangke_custom 用之,否则从当前方案颜色起步
  auto base = settings_.GetSchemeColors(
      (hasCustom ? "bangke_light" : activeScheme_));
  if (hasCustom) {
    for (const char* k : kCustomColorKeys)
      custom_[k] = settings_.GetCustomColor(k, base.count(k) ? base[k] : 0);
  } else {
    auto from = settings_.GetSchemeColors(activeScheme_);
    for (const char* k : kCustomColorKeys)
      custom_[k] = from.count(k) ? from[k] : base[k];
  }
  initCustom_ = custom_;
  customDirty_ = false;
  for (const char* k : {"back_color", "hilited_candidate_back_color",
                        "text_color"})
    btnRefresh(k);
  opacitySlider_->blockSignals(true);
  opacitySlider_->setValue((int)(custom_["back_color"] >> 24) * 100 / 255);
  opacitySlider_->blockSignals(false);

  updatePreview();
  loaded_ = true;
}

void StylePage::updatePreview() {
  const QString id = schemeCombo_->currentData().toString();
  std::map<std::string, unsigned int> colors;
  if (id == QStringLiteral("bangke_custom")) {
    // 基线共享浅色,叠加用户补丁(编辑中的 custom_ 即最新意图)
    colors = settings_.GetSchemeColors("bangke_light");
    for (const char* k : kCustomColorKeys)
      colors[k] = custom_[k];
  } else if (!id.isEmpty()) {
    colors = settings_.GetSchemeColors(id.toStdString());
  }
  const bool horizontal =
      layoutCombo_->currentData().toString() == QStringLiteral("horizontal");
  const int pr = layoutSpins_.empty() ? 10 : layoutSpins_[0].spin->value();
  const int hr = layoutSpins_.size() > 1 ? layoutSpins_[1].spin->value() : 6;
  preview_->setScheme(colors, horizontal, pr, hr);
}

bool StylePage::save() {
  bool changed = false;
  std::string scheme = schemeCombo_->currentData().toString().toStdString();
  if (!scheme.empty() && scheme != activeScheme_) {
    settings_.SelectColorScheme(scheme);
    changed = true;
  }
  if (fontSize_->value() != activeFontSize_) {
    settings_.SetFontSize(fontSize_->value());
    changed = true;
  }
  const bool horizontal =
      layoutCombo_->currentData().toString() == QStringLiteral("horizontal");
  if (horizontal != activeHorizontal_) {
    settings_.SetHorizontal(horizontal);
    changed = true;
  }
  for (auto& ls : layoutSpins_) {
    if (ls.spin->value() != initLayout_[ls.key]) {
      settings_.SetLayoutInt(ls.key, ls.spin->value());
      changed = true;
    }
  }
  for (auto& c : styleControls_) {
    switch (c.kind) {
      case StyleControl::Bool: {
        auto* box = static_cast<QAbstractButton*>(c.widget);
        const std::string now = box->isChecked() ? "1" : "0";
        if (now == c.init)
          break;
        settings_.SetStyleBool(c.key, box->isChecked());
        changed = true;
        break;
      }
      case StyleControl::Int: {
        auto* spin = static_cast<QSpinBox*>(c.widget);
        const std::string now = std::to_string(spin->value());
        if (now == c.init)
          break;
        settings_.SetStyleInt(c.key, spin->value());
        changed = true;
        break;
      }
      case StyleControl::Text: {
        auto* edit = static_cast<QLineEdit*>(c.widget);
        const std::string now = edit->text().toStdString();
        if (now == c.init)
          break;
        settings_.SetStyleString(c.key, now);
        changed = true;
        break;
      }
      case StyleControl::Choice: {
        auto* combo = static_cast<QComboBox*>(c.widget);
        const std::string now = combo->currentData().toString().toStdString();
        if (now == c.init)
          break;
        settings_.SetStyleString(c.key, now);
        changed = true;
        break;
      }
    }
  }
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
