#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <filesystem>
#include <iostream>

#include "models/StyleModel.h"
#include "Levers.h"

namespace {
QString fixture;

bool writeFixture(const QString& name, const QByteArray& contents) {
  QFile file(QDir(fixture).filePath(name));
  return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

bool paletteMatches(const StyleModel& model) {
  const auto light = model.previewLight();
  const auto dark = model.previewDark();
  std::cout << std::hex << "light=" << light.value("back_color").toUInt()
            << " dark=" << dark.value("back_color").toUInt()
            << " highlight=" << dark.value("hilited_candidate_back_color").toUInt()
            << '\n';
  return light.value("back_color").toUInt() == 0xFFF6F3EDu &&
         dark.value("back_color").toUInt() == 0xFF171C23u &&
         light.value("hilited_candidate_back_color").toUInt() == 0xFF785330u &&
         dark.value("hilited_candidate_back_color").toUInt() == 0xFF94C8BCu;
}
}  // namespace

std::filesystem::path BangkeUserDataPath() { return fixture.toStdWString(); }
std::filesystem::path BangkeSharedDataPath() { return fixture.toStdWString(); }

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QTemporaryDir directory;
  if (!directory.isValid()) return 1;
  fixture = directory.path();
  if (!writeFixture("weasel.yaml",
                    "style:\n  color_scheme: native\n  font_point: 16\n"
                    "preset_color_schemes:\n  native:\n    name: System\n"
                    "  other:\n    name: Other\n    back_color: 0x112233\n") ||
      !writeFixture("default.yaml", "menu:\n  page_size: 5\n"))
    return 2;
  const auto path = fixture.toUtf8();
  RIME_STRUCT(RimeTraits, traits);
  traits.shared_data_dir = path.constData();
  traits.user_data_dir = path.constData();
  traits.prebuilt_data_dir = path.constData();
  traits.log_dir = path.constData();
  auto* api = rime_get_api();
  api->setup(&traits);
  api->initialize(&traits);
  int result = 0;
  {
    StyleModel model;
    model.load();
    if (!paletteMatches(model)) result = 3;
    model.setColorScheme("other");
    model.setColorScheme("native");
    if (!paletteMatches(model)) result = 4;
  }
  api->finalize();
  return result;
}
