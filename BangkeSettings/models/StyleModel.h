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

#include "UIStyleSettings.h"

// style/ 下的通用控件表。键、标签、取值范围只有这一份,QML 按 kind 渲染对应控件,
// 载入/保存的逐项比较也都在这里,不必逐个控件写成员和分支。
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

  // 读 = 补丁 ⊕ 共享默认,补丁与共享都没有就用表里的 fallback
  void load(UIStyleSettings& settings);
  // 只写变了的,返回是否有改动
  bool save(UIStyleSettings& settings);

  int intValue(const QString& key, int fallback) const;
  QString stringValue(const QString& key) const;
  // 深色配色下拉的选项:载入时才拿得到方案表
  void setComboOptions(const QString& key, const QVariantList& options);

 signals:
  void valueChanged(const QString& key);

 private:
  struct Row {
    QString key;
    QString kind;  // toggle / spin / edit / combo
    QString section;
    QString label;
    QString helper;
    QVariant value;
    QVariantList options;  // kind == combo
    // 允许存着列表里没有的值(字体选择器:机器上可能没装配置里写的那款)。
    // 载入时这样的值会被插回列表,否则会显示成列表首项——看着像没设,存回去却还是旧值
    bool keepUnknown = false;
    int lo = 0;
    int hi = 0;
    // 载入取不到值时用的兜底,等于原控件构造后的默认值
    QString fallback;
    QString init;
  };
  static QString asText(const Row& row);

  std::vector<Row> rows_;
};

// 界面样式页:外观(配色/字号/横竖排)+ 布局微调(圆角/边框/间距/阴影)
// + 自定义配色(写入 preset_color_schemes/bangke_custom 补丁)。
// 读 = 补丁 ⊕ 共享默认;写 = levers 补丁;进入页面即重读,双向同步。
class StyleModel : public QObject {
  Q_OBJECT
  // 通用控件表,按 section 切片就是各张卡片
  Q_PROPERTY(StyleControlModel* styleControls READ styleControls CONSTANT)
  // 配色方案下拉的条目 [{label, value}],value 就是写进 style/color_scheme 的 id
  Q_PROPERTY(QVariantList schemes READ schemes NOTIFY schemesChanged)
  Q_PROPERTY(QString colorScheme READ colorScheme WRITE setColorScheme NOTIFY changed)
  Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY changed)
  // "vertical" / "horizontal"
  Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY changed)
  Q_PROPERTY(QVariantList layoutOptions READ layoutOptions CONSTANT)
  // 自定义配色的九个键 → 0xAARRGGBB
  Q_PROPERTY(QVariantMap customColors READ customColors NOTIFY customColorsChanged)
  Q_PROPERTY(QStringList customColorKeys READ customColorKeys CONSTANT)
  Q_PROPERTY(int customOpacity READ customOpacity NOTIFY customColorsChanged)
  // 候选窗预览的入参:真实配色 + 真实圆角 + 真实字体,由本模型算好给 QML 画
  Q_PROPERTY(QVariantMap previewColors READ previewColors NOTIFY previewChanged)
  Q_PROPERTY(bool previewHorizontal READ previewHorizontal NOTIFY previewChanged)
  Q_PROPERTY(int previewPanelRadius READ previewPanelRadius NOTIFY previewChanged)
  Q_PROPERTY(int previewHiliteRadius READ previewHiliteRadius NOTIFY previewChanged)
  Q_PROPERTY(QString previewFontFace READ previewFontFace NOTIFY previewChanged)
  Q_PROPERTY(int previewFontPoint READ previewFontPoint NOTIFY previewChanged)

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
  QVariantMap customColors() const;
  QStringList customColorKeys() const;
  int customOpacity() const;

  QVariantMap previewColors() const { return previewColors_; }
  bool previewHorizontal() const { return previewHorizontal_; }
  int previewPanelRadius() const { return previewPanelRadius_; }
  int previewHiliteRadius() const { return previewHiliteRadius_; }
  QString previewFontFace() const { return previewFontFace_; }
  int previewFontPoint() const { return previewFontPoint_; }

  // 首次进入才载
  Q_INVOKABLE void load();
  // 部署后/每次进入强制重读(未保存的改动随之丢弃)
  Q_INVOKABLE void forceLoad();
  bool save();

  Q_INVOKABLE void setCustomColor(const QString& key, uint argb);
  Q_INVOKABLE void setCustomOpacity(int percent);
  // 丢弃本会话的自定义配色编辑,回到载入时的值
  Q_INVOKABLE void resetCustomColors();

 signals:
  void changed();
  void schemesChanged();
  void customColorsChanged();
  void previewChanged();

 private:
  void onCustomEdited();
  void updatePreview();
  int indexOfScheme(const QString& id) const;

  UIStyleSettings settings_;
  StyleControlModel* styleControls_ = nullptr;
  // 配色方案下拉;除方案表外还可能多一个「自定义」(有补丁时)
  QVariantList schemes_;
  QString colorScheme_;
  int fontSize_ = 0;
  QString layout_ = QStringLiteral("vertical");

  // 自定义配色
  std::map<std::string, unsigned int> custom_;  // key → 0xAARRGGBB
  std::map<std::string, unsigned int> initCustom_;
  bool customDirty_ = false;

  QVariantMap previewColors_;
  bool previewHorizontal_ = false;
  int previewPanelRadius_ = 10;
  int previewHiliteRadius_ = 6;
  QString previewFontFace_;
  int previewFontPoint_ = 9;

  QString activeScheme_;
  int activeFontSize_ = 0;
  bool activeHorizontal_ = false;
  bool loaded_ = false;
};
