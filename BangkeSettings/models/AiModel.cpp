#include "AiModel.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>

#include <BangkeUtility.h>

#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

namespace {

QVariantMap range(int lo, int hi, const QString& suffix) {
  QVariantMap m;
  m.insert(QStringLiteral("min"), lo);
  m.insert(QStringLiteral("max"), hi);
  m.insert(QStringLiteral("suffix"), suffix);
  return m;
}

QVariantMap option(const QString& label, const QString& value) {
  QVariantMap m;
  m.insert(QStringLiteral("label"), label);
  m.insert(QStringLiteral("value"), value);
  return m;
}

// 逐行扫描 yaml:取顶层某键的值。只用于读方案的展示名,不用于写。
QString findValue(const QString& yaml, const QString& key) {
  for (const auto& line : yaml.split(QLatin1Char('\n'))) {
    if (line.trimmed().startsWith(key + QLatin1Char(':')))
      return line.mid(line.indexOf(QLatin1Char(':')) + 1).trimmed();
  }
  return QString();
}

// ---- 逐行编辑 patch 文档 ----
// librime 的 C API 不回写磁盘,Config::SaveToFile 又会经 yaml-cpp 丢掉注释并重排键。
// 这些文件与安装器、与用户手改共享,重写必须原样留下不归本页管的行——与 Linux 端
// config-ui 同款纪律。

// ai_predict: 块的范围:缩进 2 空格的 "  ai_predict:" 起,到下一个缩进 ≤2 空格的
// 非空行为止。块内键缩进 4 空格。
bool aiBlockRange(const QString& yaml, int* start, int* end) {
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
QString blockValue(const QString& block, const QString& key) {
  const QRegularExpression rx(
      QStringLiteral("(?:^|\n)    %1:[ \t]*([^\n]*)")
          .arg(QRegularExpression::escape(key)));
  auto m = rx.match(block);
  if (!m.hasMatch())
    return QString();
  QString value = m.captured(1).trimmed();
  const int hash = value.indexOf(QLatin1Char('#'));
  if (hash >= 0 && !value.startsWith(QLatin1Char('\'')) && !value.startsWith(QLatin1Char('"')))
    value = value.left(hash).trimmed();
  return value;
}

bool hasBlockKey(const QString& block, const QString& key) {
  return block.contains(QRegularExpression(
      QStringLiteral("(?:^|\n)    %1:").arg(QRegularExpression::escape(key))));
}

void removeBlockKey(QString& block, const QString& key) {
  block.remove(QRegularExpression(QStringLiteral("(?m)^    %1:[^\n]*\n?")
                                      .arg(QRegularExpression::escape(key))));
}

void setBlockKey(QString& block, const QString& key, const QString& value) {
  const QRegularExpression rx(
      QStringLiteral("(^|\n)    %1:[^\n]*").arg(QRegularExpression::escape(key)));
  auto m = rx.match(block);
  const QString line = QStringLiteral("    %1: %2").arg(key, value);
  if (m.hasMatch()) {
    block.replace(m.capturedStart(), m.capturedLength(), m.captured(1) + line);
    return;
  }
  if (!block.endsWith(QLatin1Char('\n')))
    block += QLatin1Char('\n');
  block += line + QLatin1Char('\n');
}

bool hasTopLevel(const QString& yaml, const QString& key) {
  return yaml.contains(QRegularExpression(
      QStringLiteral("(?m)^  %1:").arg(QRegularExpression::escape(key))));
}

// YAML 里以这些字符开头的标量会被当成语法,得加引号
QString yamlScalar(const QString& value) {
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

void setTopLevel(QString& yaml, const QString& key, const QString& value) {
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

void removeTopLevel(QString& yaml, const QString& key) {
  yaml.remove(QRegularExpression(QStringLiteral("(?m)^  %1:[^\n]*\n?")
                                     .arg(QRegularExpression::escape(key))));
}

}  // namespace

AiModel::AiModel(QObject* parent) : QObject(parent) {}

QString AiModel::currentSchemaName() const {
  return currentSchema_.isEmpty() ? QString() : schemaDisplayName(currentSchema_);
}

AiModel::AIPredict AiModel::predictFor(const QString& schemaId) const {
  const auto it = predict_.find(schemaId);
  return it == predict_.end() ? AIPredict() : it->second;
}

AiModel::AIPredict* AiModel::currentEdit() {
  if (currentSchema_.isEmpty())
    return nullptr;
  return &predict_[currentSchema_];
}

bool AiModel::enabled() const { return predictFor(currentSchema_).enabled; }
void AiModel::setEnabled(bool value) {
  if (auto* p = currentEdit()) {
    p->enabled = value;
    emit changed();
  }
}

bool AiModel::filterPunctuation() const {
  return predictFor(currentSchema_).filterPunctuation;
}
void AiModel::setFilterPunctuation(bool value) {
  if (auto* p = currentEdit()) {
    p->filterPunctuation = value;
    emit changed();
  }
}

bool AiModel::aiOnly() const { return predictFor(currentSchema_).aiOnly; }
void AiModel::setAiOnly(bool value) {
  if (auto* p = currentEdit()) {
    p->aiOnly = value;
    emit changed();
  }
}

QString AiModel::device() const { return predictFor(currentSchema_).device; }
void AiModel::setDevice(const QString& value) {
  if (auto* p = currentEdit()) {
    p->device = value;
    emit changed();
  }
}

int AiModel::targetIndex() const {
  return predictFor(currentSchema_).targetIndex;
}
void AiModel::setTargetIndex(int value) {
  if (auto* p = currentEdit()) {
    p->targetIndex = value;
    emit changed();
  }
}

int AiModel::minInputLength() const {
  return predictFor(currentSchema_).minInputLength;
}
void AiModel::setMinInputLength(int value) {
  if (auto* p = currentEdit()) {
    p->minInputLength = value;
    emit changed();
  }
}

int AiModel::minHanzi() const { return predictFor(currentSchema_).minHanzi; }
void AiModel::setMinHanzi(int value) {
  if (auto* p = currentEdit()) {
    p->minHanzi = value;
    emit changed();
  }
}

int AiModel::debounceMs() const { return predictFor(currentSchema_).debounceMs; }
void AiModel::setDebounceMs(int value) {
  if (auto* p = currentEdit()) {
    p->debounceMs = value;
    emit changed();
  }
}

int AiModel::maxTokens() const { return predictFor(currentSchema_).maxTokens; }
void AiModel::setMaxTokens(int value) {
  if (auto* p = currentEdit()) {
    p->maxTokens = value;
    emit changed();
  }
}

int AiModel::searchRange() const {
  return predictFor(currentSchema_).searchRange;
}
void AiModel::setSearchRange(int value) {
  if (auto* p = currentEdit()) {
    p->searchRange = value;
    emit changed();
  }
}

int AiModel::contextWindowSize() const {
  return predictFor(currentSchema_).contextWindowSize;
}
void AiModel::setContextWindowSize(int value) {
  if (auto* p = currentEdit()) {
    p->contextWindowSize = value;
    emit changed();
  }
}

int AiModel::minContextPromptLength() const {
  return predictFor(currentSchema_).minContextPromptLength;
}
void AiModel::setMinContextPromptLength(int value) {
  if (auto* p = currentEdit()) {
    p->minContextPromptLength = value;
    emit changed();
  }
}

QVariantList AiModel::deviceOptions() const {
  return {option(QStringLiteral(u"CPU"), QStringLiteral("cpu")),
          option(QStringLiteral(u"CUDA (GPU)"), QStringLiteral("cuda"))};
}

QVariantMap AiModel::fieldRanges() const {
  QVariantMap out;
  out.insert(QStringLiteral("targetIndex"), range(0, 20, QString()));
  out.insert(QStringLiteral("minInputLength"), range(1, 100, QString()));
  out.insert(QStringLiteral("minHanzi"), range(1, 10, QString()));
  out.insert(QStringLiteral("debounceMs"),
             range(0, 5000, QStringLiteral(u" ms")));
  out.insert(QStringLiteral("maxTokens"), range(1, 1024, QString()));
  out.insert(QStringLiteral("searchRange"), range(1, 100, QString()));
  out.insert(QStringLiteral("contextWindowSize"), range(1, 100, QString()));
  out.insert(QStringLiteral("minContextPromptLength"), range(1, 20, QString()));
  return out;
}

// 当前正在用的方案:rime 把最近选择的方案记在 user.yaml(previously_selected_schema)。
// 取不到时退到方案列表第一个。
QString AiModel::activeSchemaId() const {
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

// 接了 AI 的方案:生效配置里能解析出 ai_predict/model_path 的那些——安装器把 translator
// 与 filter 接进去的就是它们。往别处写设置不会有任何效果。
QStringList AiModel::aiWiredSchemas() const {
  QStringList result;
  RimeApi* rime = rime_get_api();
  RimeSchemaList list = {0};
  if (!rime->get_schema_list(&list))
    return result;
  for (size_t i = 0; i < list.size; ++i) {
    const char* id = list.list[i].schema_id;
    if (!id)
      continue;
    RimeConfig config = {0};
    if (!rime->schema_open(id, &config))
      continue;
    char buffer[MAX_PATH] = {0};
    const bool wired = rime->config_get_string(&config, "ai_predict/model_path",
                                               buffer, sizeof(buffer));
    rime->config_close(&config);
    if (wired)
      result << QString::fromUtf8(id);
  }
  rime->free_schema_list(&list);
  return result;
}

QString AiModel::schemaCustomYaml(const QString& schemaId) const {
  const QDir dir(QString::fromStdWString(BangkeUserDataPath().wstring()));
  return dir.filePath(schemaId + QStringLiteral(".custom.yaml"));
}

// 展示名取共享数据目录 <id>.schema.yaml 的顶层 name:;取不到用 id
QString AiModel::schemaDisplayName(const QString& schemaId) const {
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

// 方案的生效 speller/alphabet:纯推理模式扩展它,关掉时还原它。
// 必须是生效值(含方案自带的),不能只看补丁——补丁里没有这一行时,要备份的是方案
// 自己的字母表,不是空串。
QString AiModel::schemaAlphabet(const QString& schemaId) const {
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

AiModel::AIPredict AiModel::readPredict(const QString& schemaId) const {
  AIPredict p;
  QFile f(schemaCustomYaml(schemaId));
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return p;
  const QString yaml = QString::fromUtf8(f.readAll());
  f.close();

  int start = 0, end = 0;
  const QString block =
      aiBlockRange(yaml, &start, &end) ? yaml.mid(start, end - start) : QString();

  const auto str = [&block](const char* key) {
    return blockValue(block, QString::fromLatin1(key));
  };
  const auto boolean = [&str](const char* key, bool fallback) {
    const QString v = str(key);
    if (v.isEmpty())
      return fallback;
    return v != QStringLiteral("false") && v != QStringLiteral("0");
  };
  const auto integer = [&str](const char* key, int fallback) {
    bool ok = false;
    const int v = str(key).toInt(&ok);
    return ok ? v : fallback;
  };

  const QString storedPath = blockValue(block, QStringLiteral("model_path"));
  if (!storedPath.isEmpty()) {
    p.modelPath = storedPath;
    if (p.modelPath.startsWith(QLatin1Char('\'')) && p.modelPath.endsWith(QLatin1Char('\'')))
      p.modelPath = p.modelPath.mid(1, p.modelPath.size() - 2).replace(QStringLiteral("''"), QStringLiteral("'"));
    else if (p.modelPath.startsWith(QLatin1Char('"')) && p.modelPath.endsWith(QLatin1Char('"')))
      p.modelPath = p.modelPath.mid(1, p.modelPath.size() - 2);
  }
  p.enabled = boolean("enabled", p.enabled);
  p.filterPunctuation = boolean("filter_punctuation", p.filterPunctuation);
  p.aiOnly = boolean("ai_only", p.aiOnly);
  const QString device = str("device");
  if (!device.isEmpty())
    p.device = device;
  p.targetIndex = integer("target_index", p.targetIndex);
  p.minInputLength = integer("min_input_length", p.minInputLength);
  p.minHanzi = integer("min_hanzi", p.minHanzi);
  p.debounceMs = integer("debounce_ms", p.debounceMs);
  p.maxTokens = integer("max_tokens", p.maxTokens);
  p.searchRange = integer("search_range", p.searchRange);
  p.contextWindowSize = integer("context_window_size", p.contextWindowSize);
  p.minContextPromptLength =
      integer("min_context_prompt_length", p.minContextPromptLength);
  return p;
}

// 把某个方案的值铺到属性上
void AiModel::showSchema(const QString& schemaId) {
  currentSchema_ = schemaId;
  // 与原 operator[] 一致:缺条目就补一份默认值
  if (!predict_.count(schemaId))
    predict_[schemaId] = AIPredict();

  emit currentSchemaChanged();
  emit changed();
}

void AiModel::setCurrentSchema(const QString& schemaId) {
  if (schemaId == currentSchema_ || !schemas_.contains(schemaId))
    return;
  showSchema(schemaId);
}

void AiModel::load() {
  if (loaded_)
    return;
  loaded_ = true;
  predict_.clear();
  schemas_.clear();
  schemaNames_.clear();

  const QStringList schemas = aiWiredSchemas();
  for (const QString& id : schemas) {
    predict_[id] = readPredict(id);
    schemas_ << id;
    schemaNames_ << schemaDisplayName(id);
  }
  emit schemasChanged();

  if (schemas.isEmpty()) {
    currentSchema_.clear();
    emit currentSchemaChanged();
    emit changed();
    return;
  }
  // 默认停在正在用的那个方案:改了半天发现改的不是生效的那份最费解
  const QString active = activeSchemaId();
  showSchema(schemas.contains(active) ? active : schemas.first());
}

void AiModel::resetAll() {
  for (const QString& id : schemas_)
    predict_[id] = AIPredict();
  if (!currentSchema_.isEmpty())
    showSchema(currentSchema_);
}

// 让数字与标点留在同一个拼音段。
//
// abc_segmentor 按 speller/alphabet 决定字符落在哪个段,而插件只看得到段内的输入——
// 字母表里没有的字符会自成一个段,永远送不到模型。但字母表不能无条件带数字:speller
// 排在 selector 前面,列进去顺带把数字选词也废了。所以进来时扩、出去时还原。
//
// 只动传入的块文本,顶层那行的改写交给调用方在换块之后做:顶层可能是新增一行,
// 那会挪动块的偏移,先改后换块会拼错位置。
AiModel::AlphabetEdit AiModel::planAiOnlyAlphabet(const QString& schemaId,
                                                  bool aiOnly,
                                                  QString& block,
                                                  const QString& yaml) {
  AlphabetEdit edit;
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
      setBlockKey(block, QStringLiteral("alphabet_backup"), yamlScalar(original));
      // 先于写行记录:回答的是"用户本来有没有",不是"现在有没有"
      if (!hasTopLevel(yaml, QStringLiteral("speller/alphabet")))
        setBlockKey(block, QStringLiteral("alphabet_added"),
                    QStringLiteral("true"));
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

bool AiModel::save() {
  saveSucceeded_ = true;
  if (currentSchema_.isEmpty())
    return false;

  bool changed = false;
  // 重新枚举而不是复用载入时的列表:窗口开着的时候方案可能被增删,对着一个已经不在的
  // 方案写会写出孤儿 .custom.yaml。载入之后才出现的方案没有 predict_ 条目,跳过——它
  // 保持出厂的样子
  for (const QString& id : aiWiredSchemas()) {
    auto it = predict_.find(id);
    if (it == predict_.end())
      continue;
    const AIPredict& p = it->second;

    QFile f(schemaCustomYaml(id));
    QString yaml;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      yaml = QString::fromUtf8(f.readAll());
      f.close();
    } else {
      saveSucceeded_ = false;
      emit info(QStringLiteral(u"保存失败"), QStringLiteral(u"无法读取 %1").arg(f.fileName()));
      continue;
    }
    int start = 0, end = 0;
    if (!aiBlockRange(yaml, &start, &end)) {
      saveSucceeded_ = false;
      emit info(QStringLiteral(u"保存失败"), QStringLiteral(u"%1 的 AI 配置块已变更，请检查文件。").arg(f.fileName()));
      continue;
    }

    const auto yn = [](bool v) {
      return v ? QStringLiteral("true") : QStringLiteral("false");
    };
    QString block = yaml.mid(start, end - start);
    setBlockKey(block, QStringLiteral("model_path"), yamlScalar(p.modelPath));
    setBlockKey(block, QStringLiteral("enabled"), yn(p.enabled));
    setBlockKey(block, QStringLiteral("filter_punctuation"),
                yn(p.filterPunctuation));
    setBlockKey(block, QStringLiteral("ai_only"), yn(p.aiOnly));
    setBlockKey(block, QStringLiteral("device"), p.device);
    setBlockKey(block, QStringLiteral("target_index"),
                QString::number(p.targetIndex));
    setBlockKey(block, QStringLiteral("min_input_length"),
                QString::number(p.minInputLength));
    setBlockKey(block, QStringLiteral("min_hanzi"), QString::number(p.minHanzi));
    setBlockKey(block, QStringLiteral("debounce_ms"),
                QString::number(p.debounceMs));
    setBlockKey(block, QStringLiteral("max_tokens"),
                QString::number(p.maxTokens));
    setBlockKey(block, QStringLiteral("search_range"),
                QString::number(p.searchRange));
    setBlockKey(block, QStringLiteral("context_window_size"),
                QString::number(p.contextWindowSize));
    setBlockKey(block, QStringLiteral("min_context_prompt_length"),
                QString::number(p.minContextPromptLength));
    // 退役的键:本页不再拥有它,留着也没有别的路径会去删
    removeBlockKey(block, QStringLiteral("quality"));

    // speller/alphabet 在 ai_predict 块外,记账却在块内,这里只能先定方案
    const AlphabetEdit alphabet = planAiOnlyAlphabet(id, p.aiOnly, block, yaml);

    const QString before = yaml;
    yaml.replace(start, end - start, block);
    // 换块之后才动顶层:新增一行会挪动块的偏移,反过来做会拼错位置
    if (alphabet.action == AlphabetEdit::Set)
      setTopLevel(yaml, QStringLiteral("speller/alphabet"), alphabet.value);
    else if (alphabet.action == AlphabetEdit::Remove)
      removeTopLevel(yaml, QStringLiteral("speller/alphabet"));

    // 只写真的动过的方案:其余方案的文件保持原样,包括它们的注释
    if (yaml == before)
      continue;
    QSaveFile output(schemaCustomYaml(id));
    const QByteArray encoded = yaml.toUtf8();
    if (!output.open(QIODevice::WriteOnly) || output.write(encoded) != encoded.size() || !output.commit()) {
      saveSucceeded_ = false;
      emit info(QStringLiteral(u"保存失败"), QStringLiteral(u"无法写入 %1").arg(output.fileName()));
      continue;
    }
    changed = true;
  }
  return changed;
}

void AiModel::chooseModel() {
  if (currentSchema_.isEmpty()) return;
  const QDir user(QString::fromStdWString(BangkeUserDataPath().wstring()));
  const QString folder = QFileDialog::getExistingDirectory(nullptr,
      QStringLiteral(u"选择 CTranslate2 模型目录"), user.absoluteFilePath(modelPath()));
  if (!folder.isEmpty()) selectModelPath(folder);
}

bool AiModel::selectModelPath(const QString& folder) {
  if (currentSchema_.isEmpty() || folder.isEmpty()) return false;
  QStringList missing;
  for (const auto& name : {QStringLiteral("model.bin"), QStringLiteral("tokenizer.json"),
                           QStringLiteral("shared_vocabulary.json")}) {
    const QFileInfo file(QDir(folder).filePath(name));
    if (!file.isFile() || !file.isReadable() || file.size() == 0) missing << name;
  }
  if (!missing.isEmpty()) {
    emit info(QStringLiteral(u"模型不可用"),
              QStringLiteral(u"目录中缺少有效文件：%1").arg(missing.join(QStringLiteral(", "))));
    return false;
  }
  if (auto* p = currentEdit()) {
    p->modelPath = QDir::fromNativeSeparators(QFileInfo(folder).absoluteFilePath());
    emit changed();
    return true;
  }
  return false;
}

void AiModel::resetModel() {
  if (auto* p = currentEdit()) {
    p->modelPath = AIPredict().modelPath;
    emit changed();
  }
}
