#pragma once
#include <QWidget>

#include <QStringList>
#include <map>

class QComboBox;
class QFormLayout;
class QLabel;
class QSpinBox;

class ToggleSwitch;

// AI 预测配置页:一页管所有接了 AI 的方案,按方案各存一份,顶部选方案。
//
// ai_predict 是插件从 *方案* 配置里读的(predict_translator.cc 用
// engine_->schema()->config()),所以它天然按方案隔离——一个方案一套值,编辑哪一个只
// 影响哪一个。默认停在当前正在用的方案,免得改了半天改的不是生效的那份。
//
// 纯推理模式(ai_only)还要动同文件的 speller/alphabet:数字与标点必须留在同一个拼音段
// 里,模型才收得到;而该键在模式关闭时要逐字节还原。记账键 alphabet_backup /
// alphabet_added 也放在 ai_predict 块内,随设置一起走。
class AIPage : public QWidget {
  Q_OBJECT
 public:
  explicit AIPage(QWidget* parent = nullptr);

  void load();
  bool save();

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

  void showSchema(const QString& schemaId);
  void commitWidgets();
  AIPredict readPredict(const QString& schemaId) const;

  ToggleSwitch* enabled_ = nullptr;
  ToggleSwitch* filterPunctuation_ = nullptr;
  ToggleSwitch* aiOnly_ = nullptr;
  QComboBox* device_ = nullptr;
  QSpinBox* maxTokens_ = nullptr;
  QSpinBox* debounce_ = nullptr;
  QSpinBox* minInput_ = nullptr;
  QSpinBox* minHanzi_ = nullptr;
  QSpinBox* targetIndex_ = nullptr;
  QSpinBox* searchRange_ = nullptr;
  QSpinBox* contextWindow_ = nullptr;
  QSpinBox* minContextPrompt_ = nullptr;
  class QLineEdit* modelPath_ = nullptr;
  QComboBox* schemaCombo_ = nullptr;
  QLabel* schemaLabel_ = nullptr;

  // 各方案的设置。载入时按方案铺开,保存时只写值真的动过的那些
  std::map<QString, AIPredict> predict_;
  QString currentSchema_;

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
};
