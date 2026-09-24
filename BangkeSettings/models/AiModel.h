#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <map>

// AI 预测配置页:一页管所有接了 AI 的方案,按方案各存一份,顶部选方案。
//
// ai_predict 是插件从 *方案* 配置里读的(predict_translator.cc 用
// engine_->schema()->config()),所以它天然按方案隔离——一个方案一套值,编辑哪一个只
// 影响哪一个。默认停在当前正在用的方案,免得改了半天改的不是生效的那份。
//
// 纯推理模式(ai_only)还要动同文件的 speller/alphabet:数字与标点必须留在同一个拼音段
// 里,模型才收得到;而该键在模式关闭时要逐字节还原。记账键 alphabet_backup /
// alphabet_added 也放在 ai_predict 块内,随设置一起走。
class AiModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList schemas READ schemas NOTIFY schemasChanged)
  Q_PROPERTY(QStringList schemaNames READ schemaNames NOTIFY schemasChanged)
  Q_PROPERTY(QString currentSchema READ currentSchema WRITE setCurrentSchema NOTIFY currentSchemaChanged)
  Q_PROPERTY(QString currentSchemaName READ currentSchemaName NOTIFY currentSchemaChanged)
  // 以下都是"当前方案"的值;换当前方案前不必手动提交(写属性即落到该方案)
  Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
  Q_PROPERTY(bool filterPunctuation READ filterPunctuation WRITE setFilterPunctuation NOTIFY changed)
  Q_PROPERTY(bool aiOnly READ aiOnly WRITE setAiOnly NOTIFY changed)
  Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY changed)
  Q_PROPERTY(int targetIndex READ targetIndex WRITE setTargetIndex NOTIFY changed)
  Q_PROPERTY(int minInputLength READ minInputLength WRITE setMinInputLength NOTIFY changed)
  Q_PROPERTY(int minHanzi READ minHanzi WRITE setMinHanzi NOTIFY changed)
  Q_PROPERTY(int debounceMs READ debounceMs WRITE setDebounceMs NOTIFY changed)
  Q_PROPERTY(int maxTokens READ maxTokens WRITE setMaxTokens NOTIFY changed)
  Q_PROPERTY(int searchRange READ searchRange WRITE setSearchRange NOTIFY changed)
  Q_PROPERTY(int contextWindowSize READ contextWindowSize WRITE setContextWindowSize NOTIFY changed)
  Q_PROPERTY(int minContextPromptLength READ minContextPromptLength WRITE setMinContextPromptLength NOTIFY changed)
  // 只读:从方案的 *.custom.yaml 里读出来给用户看
  Q_PROPERTY(QString modelPath READ modelPath NOTIFY currentSchemaChanged)
  Q_PROPERTY(QVariantList deviceOptions READ deviceOptions CONSTANT)
  Q_PROPERTY(QVariantMap fieldRanges READ fieldRanges CONSTANT)

 public:
  explicit AiModel(QObject* parent = nullptr);

  QStringList schemas() const { return schemas_; }
  QStringList schemaNames() const { return schemaNames_; }
  QString currentSchema() const { return currentSchema_; }
  void setCurrentSchema(const QString& schemaId);
  QString currentSchemaName() const;

  bool enabled() const;
  void setEnabled(bool value);
  bool filterPunctuation() const;
  void setFilterPunctuation(bool value);
  bool aiOnly() const;
  void setAiOnly(bool value);
  QString device() const;
  void setDevice(const QString& value);
  int targetIndex() const;
  void setTargetIndex(int value);
  int minInputLength() const;
  void setMinInputLength(int value);
  int minHanzi() const;
  void setMinHanzi(int value);
  int debounceMs() const;
  void setDebounceMs(int value);
  int maxTokens() const;
  void setMaxTokens(int value);
  int searchRange() const;
  void setSearchRange(int value);
  int contextWindowSize() const;
  void setContextWindowSize(int value);
  int minContextPromptLength() const;
  void setMinContextPromptLength(int value);
  QString modelPath() const { return modelPath_; }

  QVariantList deviceOptions() const;
  QVariantMap fieldRanges() const;

  Q_INVOKABLE void load();
  bool save();
  // 恢复默认:所有接 AI 方案的草稿一起回出厂值(对齐 mac 的 restoreDefaults)
  Q_INVOKABLE void resetAll();

 signals:
  void changed();
  void schemasChanged();
  void currentSchemaChanged();

 private:
  // 一个方案的 AI 设置,与 ai_predict 块一一对应。默认值与插件里的同名默认值一致
  // (见 predict_translator.cc 的 LoadOptions),补丁里缺哪个键就按哪个默认走。
  struct AIPredict {
    bool enabled = true;
    bool filterPunctuation = true;
    bool aiOnly = false;
    QString device = QStringLiteral("cpu");
    int targetIndex = 1;
    int minInputLength = 12;
    int minHanzi = 2;
    int debounceMs = 200;
    int maxTokens = 64;
    int searchRange = 10;
    int contextWindowSize = 10;
    int minContextPromptLength = 2;
  };

  // 把某个方案的值铺到属性上(原 showSchema)
  void showSchema(const QString& schemaId);
  AIPredict readPredict(const QString& schemaId) const;
  AIPredict predictFor(const QString& schemaId) const;
  AIPredict* currentEdit();

  QString activeSchemaId() const;
  QStringList aiWiredSchemas() const;
  QString schemaCustomYaml(const QString& schemaId) const;
  QString schemaDisplayName(const QString& schemaId) const;
  // 方案的生效 speller/alphabet(纯推理模式扩展/还原它)
  QString schemaAlphabet(const QString& schemaId) const;

  // 顶层 speller/alphabet 该怎么改:块内只做记账,行本身由调用方在换块之后落笔
  struct AlphabetEdit {
    enum Action { None, Set, Remove } action = None;
    QString value;
  };
  AlphabetEdit planAiOnlyAlphabet(const QString& schemaId,
                                  bool aiOnly,
                                  QString& block,
                                  const QString& yaml);

  // 各方案的设置。载入时按方案铺开,保存时写回各自的 *.custom.yaml
  std::map<QString, AIPredict> predict_;
  QStringList schemas_;
  QStringList schemaNames_;
  QString currentSchema_;
  QString modelPath_;
};
