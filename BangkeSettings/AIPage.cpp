#include "AIPage.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "Ui.h"
#include <BangkeUtility.h>

#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

AIPage::AIPage(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 12, 16, 12);
  layout->setSpacing(10);

  enabled_ = new ToggleSwitch;
  filterPunctuation_ = new ToggleSwitch;
  aiOnly_ = new ToggleSwitch;
  schemaLabel_ = new QLabel;
  schemaLabel_->setObjectName(QStringLiteral("hint"));

  device_ = new QComboBox;
  device_->setFixedWidth(150);
  device_->addItem(QStringLiteral(u"CPU"), QStringLiteral("cpu"));
  device_->addItem(QStringLiteral(u"CUDA (GPU)"), QStringLiteral("cuda"));
  maxTokens_ = new QSpinBox;
  maxTokens_->setRange(1, 1024);
  debounce_ = new QSpinBox;
  debounce_->setRange(0, 5000);
  debounce_->setSuffix(QStringLiteral(u" ms"));
  minInput_ = new QSpinBox;
  minInput_->setRange(1, 100);
  minHanzi_ = new QSpinBox;
  minHanzi_->setRange(1, 10);
  for (auto* spin : {maxTokens_, debounce_, minInput_, minHanzi_})
    spin->setFixedWidth(96);

  auto* basic = new QVBoxLayout();
  basic->addWidget(schemaLabel_);
  basic->addLayout(makeSettingRows({
      makeSettingRow(QStringLiteral(u"启用 AI 预测"), enabled_,
                     QStringLiteral(u"开关作用于当前方案")),
      makeSettingRow(QStringLiteral(u"过滤标点符号"), filterPunctuation_,
                     QStringLiteral(u"关掉后候选保留模型吐出的句读")),
      makeSettingRow(QStringLiteral(u"仅使用 AI 候选"), aiOnly_,
                     QStringLiteral(u"候选只来自模型:一条、不带序号")),
      makeSettingRow(QStringLiteral(u"设备"), device_),
      makeSettingRow(QStringLiteral(u"最大 token 数"), maxTokens_),
      makeSettingRow(QStringLiteral(u"防抖"), debounce_),
      makeSettingRow(QStringLiteral(u"最小输入长度"), minInput_),
      makeSettingRow(QStringLiteral(u"最少汉字数"), minHanzi_),
  }));
  layout->addWidget(makeCard(QStringLiteral(u"基本"), basic));

  targetIndex_ = new QSpinBox;
  targetIndex_->setRange(0, 20);
  searchRange_ = new QSpinBox;
  searchRange_->setRange(1, 100);
  contextWindow_ = new QSpinBox;
  contextWindow_->setRange(1, 100);
  minContextPrompt_ = new QSpinBox;
  minContextPrompt_->setRange(1, 20);
  for (auto* spin :
       {targetIndex_, searchRange_, contextWindow_, minContextPrompt_})
    spin->setFixedWidth(96);
  modelPath_ = new QLineEdit;
  modelPath_->setReadOnly(true);
  modelPath_->setFixedWidth(280);

  layout->addWidget(makeCard(
      QStringLiteral(u"高级选项"),
      makeSettingRows({
          makeSettingRow(QStringLiteral(u"AI 候选显示位置 (target_index)"),
                         targetIndex_),
          makeSettingRow(QStringLiteral(u"去重扫描范围 (search_range)"),
                         searchRange_),
          makeSettingRow(QStringLiteral(u"上下文窗口 (context_window_size)"),
                         contextWindow_),
          makeSettingRow(QStringLiteral(u"上下文最小拼音数"), minContextPrompt_),
          makeSettingRow(QStringLiteral(u"模型路径"), modelPath_),
      })));
  layout->addStretch(1);

  load();
}

// 逐行扫描 yaml:只取 ai_predict: 块下已知键的值,其余行不管。
// 这是读;写用 save() 里的"替换已知行"策略——与 fcitx5-rime config-ui 同款。
static QString findValue(const QString& yaml, const QString& key) {
  for (const auto& line : yaml.split(QLatin1Char('\n'))) {
    if (line.trimmed().startsWith(key + QLatin1Char(':')))
      return line.mid(line.indexOf(QLatin1Char(':')) + 1).trimmed();
  }
  return QString();
}

// 当前方案:rime 把最近选择的方案记在 user.yaml(previously_selected_schema)。
// 取不到时退到方案列表第一个。
QString AIPage::currentSchemaId() const {
  const QDir dir(QString::fromStdWString(BangkeUserDataPath().wstring()));
  QFile u(dir.filePath(QStringLiteral("user.yaml")));
  if (u.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString yaml = QString::fromUtf8(u.readAll());
    u.close();
    static const QRegularExpression rx(
        QStringLiteral("(?m)^\\s*previously_selected_schema:\\s*(\\S+)"));
    auto m = rx.match(yaml);
    if (m.hasMatch())
      return m.captured(1);
  }
  QFile d(dir.filePath(QStringLiteral("default.custom.yaml")));
  if (d.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString yaml = QString::fromUtf8(d.readAll());
    d.close();
    static const QRegularExpression rx(QStringLiteral("schema:\\s*(\\S+)"));
    auto m = rx.match(yaml);
    if (m.hasMatch())
      return m.captured(1);
  }
  return QStringLiteral("luna_pinyin");
}

QString AIPage::schemaCustomYaml(const QString& schemaId) const {
  const QDir dir(QString::fromStdWString(BangkeUserDataPath().wstring()));
  return dir.filePath(schemaId + QStringLiteral(".custom.yaml"));
}

// 展示名取共享数据目录 <id>.schema.yaml 的顶层 name:;取不到用 id
QString AIPage::schemaDisplayName(const QString& schemaId) const {
  const QDir dir(QString::fromStdWString(BangkeSharedDataPath().wstring()));
  QFile f(dir.filePath(schemaId + QStringLiteral(".schema.yaml")));
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString v =
        findValue(QString::fromUtf8(f.readAll()), QStringLiteral("name"));
    f.close();
    if (!v.isEmpty())
      return v;
  }
  return schemaId;
}

void AIPage::load() {
  const QString schema = currentSchemaId();
  schemaLabel_->setText(QStringLiteral(u"当前方案:%1").arg(
      schemaDisplayName(schema)));
  QFile f(schemaCustomYaml(schema));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return;
  const QString yaml = QString::fromUtf8(f.readAll());
  f.close();

  // 缺键走插件里的同名默认值(见 predict_translator.cc 的 LoadOptions)
  const auto boolOr = [&yaml](const QString& key, bool fallback) {
    const QString v = findValue(yaml, key);
    return v.isEmpty() ? fallback : v != QStringLiteral("false");
  };
  enabled_->setChecked(boolOr(QStringLiteral("enabled"), true));
  filterPunctuation_->setChecked(boolOr(QStringLiteral("filter_punctuation"), true));
  aiOnly_->setChecked(boolOr(QStringLiteral("ai_only"), false));
  const QString dev = findValue(yaml, QStringLiteral("device"));
  device_->setCurrentIndex(dev == QStringLiteral("cuda") ? 1 : 0);
  maxTokens_->setValue(findValue(yaml, QStringLiteral("max_tokens")).toInt());
  debounce_->setValue(findValue(yaml, QStringLiteral("debounce_ms")).toInt());
  minInput_->setValue(findValue(yaml, QStringLiteral("min_input_length")).toInt());
  minHanzi_->setValue(findValue(yaml, QStringLiteral("min_hanzi")).toInt());
  targetIndex_->setValue(findValue(yaml, QStringLiteral("target_index")).toInt());
  searchRange_->setValue(findValue(yaml, QStringLiteral("search_range")).toInt());
  contextWindow_->setValue(findValue(yaml, QStringLiteral("context_window_size")).toInt());
  minContextPrompt_->setValue(findValue(yaml, QStringLiteral("min_context_prompt_length")).toInt());
  modelPath_->setText(findValue(yaml, QStringLiteral("model_path")));

  initState_ = stateSignature();
}

QString AIPage::stateSignature() const {
  return QStringList{
             enabled_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"),
             filterPunctuation_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"),
             aiOnly_->isChecked() ? QStringLiteral("1") : QStringLiteral("0"),
             device_->currentData().toString(),
             QString::number(maxTokens_->value()),
             QString::number(debounce_->value()),
             QString::number(minInput_->value()),
             QString::number(minHanzi_->value()),
             QString::number(targetIndex_->value()),
             QString::number(searchRange_->value()),
             QString::number(contextWindow_->value()),
             QString::number(minContextPrompt_->value()),
             modelPath_->text(),
         }
      .join(QLatin1Char('|'));
}

// ---- 逐行编辑 patch 文档 ----
// librime 的 C API 不回写磁盘,Config::SaveToFile 又会经 yaml-cpp 丢掉注释并重排键。
// 这些文件与安装器、与用户手改共享,重写必须原样留下不归本页管的行——与 Linux 端
// config-ui 同款纪律。

// ai_predict: 块的范围:缩进 2 空格的 "  ai_predict:" 起,到下一个缩进 ≤2 空格的
// 非空行为止。块内键缩进 4 空格。
static bool aiBlockRange(const QString& yaml, int* start, int* end) {
  const int apStart = yaml.indexOf(QStringLiteral("  ai_predict:"));
  if (apStart < 0)
    return false;
  int apEnd = yaml.length();
  int pos = yaml.indexOf(QLatin1Char('\n'), apStart);
  while (pos >= 0) {
    const int nextNL = yaml.indexOf(QLatin1Char('\n'), pos + 1);
    if (nextNL < 0)
      break;
    const QString line = yaml.mid(pos + 1, nextNL - pos - 1);
    if (!line.isEmpty() && !line.startsWith(QStringLiteral("    "))) {
      apEnd = pos + 1;
      break;
    }
    pos = nextNL;
  }
  *start = apStart;
  *end = apEnd;
  return true;
}

// 块内某个键的原始值(剥掉行尾注释);块外同名键不算
static QString blockValue(const QString& block, const QString& key) {
  const QRegularExpression rx(
      QStringLiteral("(?:^|\n)    %1:[ \t]*([^\n]*)")
          .arg(QRegularExpression::escape(key)));
  auto m = rx.match(block);
  if (!m.hasMatch())
    return QString();
  QString value = m.captured(1).trimmed();
  const int hash = value.indexOf(QLatin1Char('#'));
  if (hash >= 0)
    value = value.left(hash).trimmed();
  return value;
}

static bool hasBlockKey(const QString& block, const QString& key) {
  return block.contains(QRegularExpression(
      QStringLiteral("(?:^|\n)    %1:").arg(QRegularExpression::escape(key))));
}

static void removeBlockKey(QString& block, const QString& key) {
  block.remove(QRegularExpression(QStringLiteral("(?m)^    %1:[^\n]*\n?")
                                      .arg(QRegularExpression::escape(key))));
}

static bool hasTopLevel(const QString& yaml, const QString& key) {
  return yaml.contains(QRegularExpression(
      QStringLiteral("(?m)^  %1:").arg(QRegularExpression::escape(key))));
}

// YAML 里以这些字符开头的标量会被当成语法,得加引号
static QString yamlScalar(const QString& value) {
  static const QString kIndicators = QStringLiteral("-?:,[]{}#&*!|>'\"%@`");
  const bool needs =
      value.isEmpty() || value.contains(QLatin1Char('#')) ||
      value.contains(QLatin1Char('\'')) ||
      value.contains(QStringLiteral(": ")) ||
      (!value.isEmpty() && kIndicators.contains(value.at(0)));
  if (!needs)
    return value;
  return QStringLiteral("'") +
         QString(value).replace(QLatin1Char('\''), QStringLiteral("''")) +
         QStringLiteral("'");
}

static void setTopLevel(QString& yaml, const QString& key, const QString& value) {
  const QRegularExpression rx(
      QStringLiteral("(?m)^  %1:[^\n]*").arg(QRegularExpression::escape(key)));
  auto m = rx.match(yaml);
  const QString line = QStringLiteral("  %1: %2").arg(key, yamlScalar(value));
  if (m.hasMatch()) {
    yaml.replace(m.capturedStart(), m.capturedLength(), line);
    return;
  }
  const int patch = yaml.indexOf(QStringLiteral("patch:"));
  if (patch < 0)
    return;
  const int nl = yaml.indexOf(QLatin1Char('\n'), patch);
  if (nl < 0)
    return;
  yaml.insert(nl + 1, line + QLatin1Char('\n'));
}

static void removeTopLevel(QString& yaml, const QString& key) {
  yaml.remove(QRegularExpression(QStringLiteral("(?m)^  %1:[^\n]*\n?")
                                     .arg(QRegularExpression::escape(key))));
}

// 方案的生效 speller/alphabet:纯推理模式扩展它,关掉时还原它
QString AIPage::schemaAlphabet(const QString& schemaId) const {
  RimeApi* rime = rime_get_api();
  RimeConfig config = {0};
  if (!rime->schema_open(schemaId.toUtf8().constData(), &config))
    return QString();
  // rime_ice 的字母表上百字符,给小了会静默截断,写回去就是个残缺配置
  char buffer[1024] = {0};
  QString result;
  if (rime->config_get_string(&config, "speller/alphabet", buffer,
                              sizeof(buffer)))
    result = QString::fromUtf8(buffer);
  rime->config_close(&config);
  return result;
}

// 让数字与标点留在同一个拼音段。
//
// abc_segmentor 按 speller/alphabet 决定字符落在哪个段,而插件只看得到段内的输入——
// 字母表里没有的字符会自成一个段,永远送不到模型。但字母表不能无条件带数字:speller
// 排在 selector 前面,列进去顺带把数字选词也废了。所以进来时扩、出去时还原。
//
// 只动传入的块文本,顶层那行的改写交给调用方在换块之后做:顶层可能是新增一行,
// 那会挪动块的偏移,先改后换块会拼错位置。
AIPage::AlphabetEdit AIPage::planAiOnlyAlphabet(const QString& schemaId,
                                                bool aiOnly,
                                                QString& block,
                                                const QString& yaml) {
  AlphabetEdit edit;
  auto setBlock = [&block](const QString& key, const QString& value) {
    const QRegularExpression rx(
        QStringLiteral("(^|\n)    %1:[^\n]*")
            .arg(QRegularExpression::escape(key)));
    auto m = rx.match(block);
    const QString line = QStringLiteral("    %1: %2").arg(key, value);
    if (m.hasMatch()) {
      block.replace(m.capturedStart(), m.capturedLength(), m.captured(1) + line);
    } else {
      if (!block.endsWith(QLatin1Char('\n')))
        block += QLatin1Char('\n');
      block += line + QLatin1Char('\n');
    }
  };
  const QString extra = QString::fromLatin1(kAiOnlyExtraInput);
  const QString backup = blockValue(block, QStringLiteral("alphabet_backup"));

  if (aiOnly) {
    QString original = backup;
    if (original.isEmpty()) {
      // 原始值只捕获一次:以后每次保存都不能把"我们自己扩过的值"当原始值备份下来,
      // 否则那些数字会变成永久成员。
      // 扩展本身则每次都重写——安装器会用出厂文件重建这些 custom.yaml,只搬它认识的
      // 键,掉一次就得手动保存一次才恢复。
      original = schemaAlphabet(schemaId);
      if (original.isEmpty())
        return edit;  // 取不到生效字母表就别动,免得写出一个只剩数字的字母表
      setBlock(QStringLiteral("alphabet_backup"), yamlScalar(original));
      // 先于写行记录:回答的是"用户本来有没有",不是"现在有没有"
      if (!hasTopLevel(yaml, QStringLiteral("speller/alphabet")))
        setBlock(QStringLiteral("alphabet_added"), QStringLiteral("true"));
    }
    edit.action = AlphabetEdit::Set;
    edit.value = original + extra;
    return edit;
  }

  if (backup.isEmpty())
    return edit;  // 没开过,没什么要还原的
  // 我们创建的整行删掉;用户原有的还原其值——把旧值写回去会留下一行已经不做事的配置
  if (hasBlockKey(block, QStringLiteral("alphabet_added"))) {
    edit.action = AlphabetEdit::Remove;
  } else {
    edit.action = AlphabetEdit::Set;
    edit.value = backup;
  }
  removeBlockKey(block, QStringLiteral("alphabet_added"));
  removeBlockKey(block, QStringLiteral("alphabet_backup"));
  return edit;
}

// 保存:读全文,逐行替换已知键的值;块内缺的键追加。
// 未管理的行(含注释、engine 配置等)原样保留。
// 只写当前方案的 custom yaml。
bool AIPage::save() {
  const QString sig = stateSignature();
  if (sig == initState_)
    return false;

  const QString schema = currentSchemaId();
  QFile f(schemaCustomYaml(schema));
  QString yaml;
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    yaml = QString::fromUtf8(f.readAll());
    f.close();
  }

  int apStart = 0, apEnd = 0;
  if (!aiBlockRange(yaml, &apStart, &apEnd))
    return false;  // 无 ai_predict 块则不写(避免在错误位置追加)

  const auto yn = [](bool v) {
    return v ? QStringLiteral("true") : QStringLiteral("false");
  };
  // 键值对;写入时必须在 ai_predict: 块内(4 空格缩进,patch:→ai_predict:→键)
  struct KV { const char* key; QString val; };
  const KV kvs[] = {
      {"enabled", yn(enabled_->isChecked())},
      {"filter_punctuation", yn(filterPunctuation_->isChecked())},
      {"ai_only", yn(aiOnly_->isChecked())},
      {"device", device_->currentData().toString()},
      {"max_tokens", QString::number(maxTokens_->value())},
      {"debounce_ms", QString::number(debounce_->value())},
      {"min_input_length", QString::number(minInput_->value())},
      {"min_hanzi", QString::number(minHanzi_->value())},
      {"target_index", QString::number(targetIndex_->value())},
      {"search_range", QString::number(searchRange_->value())},
      {"context_window_size", QString::number(contextWindow_->value())},
      {"min_context_prompt_length", QString::number(minContextPrompt_->value())},
      {"model_path", yamlScalar(modelPath_->text())},
  };

  QString newBlock = yaml.mid(apStart, apEnd - apStart);
  for (const auto& kv : kvs) {
    const QString key = QString::fromLatin1(kv.key);
    const QRegularExpression rx(
        QStringLiteral("(^|\n)    %1:\s*[^\n]*").arg(
            QRegularExpression::escape(key)));
    auto m2 = rx.match(newBlock);
    if (m2.hasMatch()) {
      newBlock.replace(m2.capturedStart(), m2.capturedLength(),
                       m2.captured(1) + QStringLiteral("    ") + key +
                           QStringLiteral(": ") + kv.val);
    } else {
      // 追加到块内最后一行后
      if (!newBlock.endsWith(QLatin1Char('\n')))
        newBlock += QLatin1Char('\n');
      newBlock += QStringLiteral("    %1: %2\n").arg(key, kv.val);
    }
  }

  // 退役的键:本页不再拥有它,留着也没有别的路径会去删
  removeBlockKey(newBlock, QStringLiteral("quality"));

  // speller/alphabet 在 ai_predict 块外,记账却在块内,这里只能先定方案
  const AlphabetEdit alphabet =
      planAiOnlyAlphabet(schema, aiOnly_->isChecked(), newBlock, yaml);

  yaml.replace(apStart, apEnd - apStart, newBlock);

  // 换块之后才动顶层:新增一行会挪动块的偏移,反过来做会拼错位置
  if (alphabet.action == AlphabetEdit::Set)
    setTopLevel(yaml, QStringLiteral("speller/alphabet"), alphabet.value);
  else if (alphabet.action == AlphabetEdit::Remove)
    removeTopLevel(yaml, QStringLiteral("speller/alphabet"));

  if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    return false;
  f.write(yaml.toUtf8());
  f.close();
  initState_ = sig;
  return true;
}
