#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <map>
#include <string>
#include <vector>

#include "../UIStyleSettings.h"

// style/ 下的通用控件表。键、标签、取值范围只有这一份,QML 按 kind 渲染对应控件,
// 载入/保存的逐项比较也都在这里,不必逐个控件写成员和分支。
// 出厂值(factory)与 data/weasel.yaml 的 style: 段一一对应,恢复默认用。
class StyleControlModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    KeyRole = Qt::UserRole + 1,
    KindRole,
    SectionRole,
    LabelRole,
    HelperRole,
    ValueRole,
    MinRole,
    MaxRole,
    StepRole,
    OptionsRole,
    InitRole,
  };
  Q_ENUM(Role)

  explicit StyleControlModel(QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;

  // 读 = 补丁 ⊕ 共享默认,补丁与共享都没有就用表里的 fallback。
  // horizontal 决定间距类的下界(引擎按排列方向钳不同的 padding)
  void load(UIStyleSettings& settings, bool horizontal);
  // 间距滑杆下界:引擎的 fixup 会把间距抬到 hilite_padding 的两倍,比这更小
  // 的值拖了没反应。下界随排列方向变(单行钳 padding_x、多行钳 padding_y)
  void setSpacingFloors(bool horizontal);
  int spacingFloor(const QString& key, bool horizontal) const;
  // 只写变了的,返回是否有改动
  bool save(UIStyleSettings& settings);
  void markSaved();
  // 草稿回到出厂值(仍需 save 才落盘;init 快照不动)
  void resetDefaults();

  int intValue(const QString& key, int fallback) const;
  double doubleValue(const QString& key, double fallback) const;
  bool boolValue(const QString& key, bool fallback) const;
  QString stringValue(const QString& key) const;
  // 深色配色下拉的选项:载入时才拿得到方案表
  void setComboOptions(const QString& key, const QVariantList& options);

 signals:
  void valueChanged(const QString& key);

 private:
  struct Row {
    QString key;
    QString kind;  // toggle / slider / dslider / edit / combo
    QString section;
    QString label;
    QString helper;
    QVariant value;
    QVariantList options;  // kind == combo
    // 允许存着列表里没有的值(字体选择器:机器上可能没装配置里写的那款)。
    // 载入时这样的值会被插回列表,否则会显示成列表首项——看着像没设,存回去却还是旧值
    bool keepUnknown = false;
    double lo = 0;
    double hi = 0;
    double step = 1;
    // 界面显示值与配置值相反(alpha 存的是不透明度,界面显示透明度):
    // 载入取反、保存取反,fallback/factory 也各按各自空间存
    bool invert = false;
    // 载入取不到值时用的兜底;出厂值单独一份,恢复默认用
    QString fallback;
    QString factory;
    QString init;
  };
  static QString asText(const Row& row);

  std::vector<Row> rows_;
  // 间距滑杆的下界入参(layout/hilite_padding_*),load 时读一次
  int spacingPadX_ = 0;
  int spacingPadY_ = 0;
  // 最近一次的排列方向:resetDefaults 也要按下界收紧出厂值
  bool spacingHorizontal_ = false;
};

// 外观页(对齐 mac AppearanceSettingsPage):颜色(配色/深色配色/不透明度)
// + 字体 + 布局(排列/方向/圆角/间距/阴影/边框) + 显示 + 格式(候选格式)。
// 配色方案/字号/候选排列是本模型直属属性,其余走 styleControls 表。
// 读 = 补丁 ⊕ 共享默认;写 = levers 补丁。
class StyleModel : public QObject {
  Q_OBJECT
  // 通用控件表,按 section 切片就是各张卡片
  Q_PROPERTY(StyleControlModel* styleControls READ styleControls CONSTANT)
  // 配色方案下拉的条目 [{label, value}],value 就是写进 style/color_scheme 的 id
  Q_PROPERTY(QVariantList schemes READ schemes NOTIFY schemesChanged)
  Q_PROPERTY(QString colorScheme READ colorScheme WRITE setColorScheme NOTIFY changed)
  Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY changed)
  // "vertical"(多行) / "horizontal"(单行)
  Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY changed)
  Q_PROPERTY(QVariantList layoutOptions READ layoutOptions CONSTANT)
  // 浅/深两块预览面板的配色(0xAARRGGBB);深色块 = 深色配色方案,空则跟随浅色
  Q_PROPERTY(QVariantMap previewLight READ previewLight NOTIFY previewChanged)
  Q_PROPERTY(QVariantMap previewDark READ previewDark NOTIFY previewChanged)
  // 预览面板的度量入参:真实圆角/间距/字号/横竖排/内联拼音/不透明度
  Q_PROPERTY(QVariantMap previewMetrics READ previewMetrics NOTIFY previewChanged)
  // 单行排列的独立 bool:QML 对 QVariantMap 下标的绑定(previewMetrics.
  // horizontal)在首载信号时不重求值,首屏预览方向曾滞留多行 → 竖线
  Q_PROPERTY(bool previewHorizontal READ previewHorizontal NOTIFY previewChanged)

 public:
  explicit StyleModel(QObject* parent = nullptr);

  StyleControlModel* styleControls() const { return styleControls_; }
  QVariantList schemes() const { return schemes_; }
  QString colorScheme() const { return colorScheme_; }
  void setColorScheme(const QString& value);
  int fontSize() const { return fontSize_; }
  void setFontSize(int value);
  QString layout() const { return layout_; }
  void setLayout(const QString& value);
  QVariantList layoutOptions() const;

  QVariantMap previewLight() const { return previewLight_; }
  QVariantMap previewDark() const { return previewDark_; }
  QVariantMap previewMetrics() const { return previewMetrics_; }
  bool previewHorizontal() const { return previewHorizontal_; }

  // 首次进入才载;载入后不重读,草稿跨页保持
  Q_INVOKABLE void load();
  bool save();
  bool saveSucceeded() const { return saveSucceeded_; }
  // 恢复默认:草稿回到出厂值(仍需保存才落盘)
  Q_INVOKABLE void resetDefaults();

 signals:
  void changed();
  void schemesChanged();
  void previewChanged();

 private:
  bool saveSucceeded_ = true;
  void updatePreview();
  int indexOfScheme(const QString& id) const;

  UIStyleSettings settings_;
  StyleControlModel* styleControls_ = nullptr;
  // 配色方案下拉;除方案表外还可能有存量 bangke_custom
  QVariantList schemes_;
  QString colorScheme_;
  int fontSize_ = 16;
  QString layout_ = QStringLiteral("vertical");

  QVariantMap previewLight_;
  QVariantMap previewDark_;
  QVariantMap previewMetrics_;
  bool previewHorizontal_ = false;
  // 每页候选数(default.custom.yaml),预览条数跟随实际
  int pageSize_ = 5;

  QString activeScheme_;
  int activeFontSize_ = 0;
  bool activeHorizontal_ = false;
  bool loaded_ = false;
};
