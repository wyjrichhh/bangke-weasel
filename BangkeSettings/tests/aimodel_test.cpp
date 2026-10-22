#include <windows.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include "models/AiModel.h"
#include "models/SwitcherModel.h"
#include <rime_api.h>

static QString fixture;
std::filesystem::path BangkeUserDataPath() { return fixture.toStdWString(); }
std::filesystem::path BangkeSharedDataPath() { return fixture.toStdWString(); }
#define REQUIRE(c) do { if (!(c)) { std::cerr << "FAIL " << __LINE__ << ": " << #c << '\n'; return 1; } } while (0)
static void write(const QString& relative, const QByteArray& content) {
  QFile f(QDir(fixture).filePath(relative));
  if (!f.open(QIODevice::WriteOnly) || f.write(content) != content.size()) std::abort();
}
static QByteArray read(const QString& relative) {
  QFile f(QDir(fixture).filePath(relative)); f.open(QIODevice::ReadOnly); return f.readAll();
}
int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  fixture = dir.path();
  QDir(fixture).mkdir("build");
  const QByteArray defaults = "schema_list:\n  - schema: rime_ice\n  - schema: luna_pinyin\n";
  write("default.yaml", defaults); write("build/default.yaml", defaults);
  for (const QString& id : {QString("rime_ice"), QString("luna_pinyin")}) {
    const auto schema = ("schema:\n  schema_id: " + id + "\n  name: test\nai_predict:\n  model_path: predict_models/zh-base-ct2-int8\nspeller:\n  alphabet: abcdefghijklmnopqrstuvwxyz\n").toUtf8();
    write(id + ".schema.yaml", schema); write("build/" + id + ".schema.yaml", schema);
    write(id + ".custom.yaml", "patch:\n  ai_predict:\n    model_path: predict_models/zh-base-ct2-int8\n    enabled: true\n");
  }
  write("default.custom.yaml", "patch:\n  schema_list:\n    - schema: rime_ice\n");
  RIME_STRUCT(RimeTraits, traits);
  const auto path = fixture.toUtf8();
  const auto build = (fixture + "/build").toUtf8();
  traits.shared_data_dir = path.constData(); traits.user_data_dir = path.constData();
  traits.prebuilt_data_dir = build.constData(); traits.log_dir = path.constData();
  traits.app_name = "rime.bangke.aimodel_test";
  auto* api = rime_get_api(); api->setup(&traits); api->initialize(&traits);
  {
    AiModel model;
    model.load(); REQUIRE(model.schemas().size() == 2);
    model.setCurrentSchema("rime_ice"); model.setEnabled(false);
    model.load(); REQUIRE(!model.enabled()); // Page changes must preserve drafts.
    REQUIRE(!model.selectModelPath(fixture + "/missing"));
    const auto folder = QString("model # owner's"); QDir(fixture).mkdir(folder);
    for (const auto& name : {"model.bin", "tokenizer.json", "shared_vocabulary.json"})
      write(folder + "/" + name, "fixture");
    const auto selected = fixture + "/" + folder;
    REQUIRE(model.selectModelPath(selected));
    model.setCurrentSchema("luna_pinyin"); REQUIRE(model.enabled());
    REQUIRE(model.modelPath() == "predict_models/zh-base-ct2-int8");
    model.setCurrentSchema("rime_ice"); REQUIRE(model.modelPath() == selected);
    model.setAiOnly(true); REQUIRE(model.save());
    const auto saved = read("rime_ice.custom.yaml");
    REQUIRE(saved.contains("owner''s"));
    REQUIRE(saved.contains("speller/alphabet:"));
    model.setAiOnly(false); REQUIRE(model.save());
    REQUIRE(!read("rime_ice.custom.yaml").contains("speller/alphabet:"));
    REQUIRE(!read("rime_ice.custom.yaml").contains("alphabet_backup:"));
    AiModel reopened; reopened.load(); reopened.setCurrentSchema("rime_ice");
    REQUIRE(!reopened.enabled() && reopened.modelPath() == selected);
    reopened.resetModel(); REQUIRE(reopened.modelPath() == "predict_models/zh-base-ct2-int8");
    REQUIRE(reopened.save()); REQUIRE(reopened.saveSucceeded());
    REQUIRE(!reopened.save()); REQUIRE(reopened.saveSucceeded());
    const auto custom = QDir(fixture).filePath("rime_ice.custom.yaml");
    const auto original = read("rime_ice.custom.yaml");
    REQUIRE(QFile::remove(custom)); REQUIRE(QDir().mkdir(custom));
    reopened.setEnabled(true);
    reopened.save(); REQUIRE(!reopened.saveSucceeded());
    REQUIRE(reopened.enabled()); // Failure must keep the draft available for retry.
    REQUIRE(QDir().rmdir(custom)); write("rime_ice.custom.yaml", original);
    REQUIRE(reopened.save()); REQUIRE(reopened.saveSucceeded());
  }
  {
    SwitcherModel switcher;
    switcher.load(); REQUIRE(switcher.rowCount() == 2);
    const auto before = read("default.custom.yaml");
    std::vector<bool> enabled;
    for (int i = 0; i < switcher.rowCount(); ++i) {
      enabled.push_back(switcher.data(switcher.index(i), SwitcherModel::EnabledRole).toBool());
      switcher.setData(switcher.index(i), false, SwitcherModel::EnabledRole);
    }
    REQUIRE(!switcher.save()); REQUIRE(!switcher.saveSucceeded());
    REQUIRE(read("default.custom.yaml") == before);
    for (int i = 0; i < switcher.rowCount(); ++i)
      switcher.setData(switcher.index(i), static_cast<bool>(enabled[i]), SwitcherModel::EnabledRole);
    REQUIRE(!switcher.save()); REQUIRE(switcher.saveSucceeded());
    REQUIRE(read("default.custom.yaml") == before);
  }
  api->finalize();
  // Reloading the plugin must not tear down or reinitialize host-owned glog.
  api->initialize(&traits); api->finalize();
  std::cout << "PASS model validation, schema isolation, draft retention, atomic save, quoted paths and AI-only alphabet restore\n";
}
