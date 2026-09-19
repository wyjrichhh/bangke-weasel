#pragma once
#include <QWidget>

#include <QStringList>

class QComboBox;
class QFormLayout;
class QLabel;
class QSpinBox;

class ToggleSwitch;

// AI 预测配置页:读写"当前方案"custom.yaml 的 ai_predict: 块。
// 当前方案取自 user.yaml 的 previously_selected_schema,切换方案后
// 重新进入页面即重载;保存时由 MainWindow 统一触发重新部署。
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
  QString stateSignature() const;

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
  QLabel* schemaLabel_ = nullptr;

  QString currentSchemaId() const;
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
  // 载入时的控件状态签名;保存仅在有差异时写文件
  QString initState_;
};
