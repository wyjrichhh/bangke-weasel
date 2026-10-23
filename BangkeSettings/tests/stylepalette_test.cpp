#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QColor>
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
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("styleModel", &model);
    QQmlComponent mock(&engine);
    mock.setData("import QtQml; QtObject {property QtObject style: styleModel}", QUrl());
    auto* context = mock.create();
    if (!context) return 5;
    context->setParent(&engine);
    engine.rootContext()->setContextProperty("app", context);
    QQmlComponent component(&engine);
    component.loadFromModule("Bangke.Pages", "MacPreviewStrip");
    auto* root = qobject_cast<QQuickItem*>(component.create());
    if (!root) {
      std::cerr << component.errorString().toStdString();
      return 6;
    }
    QQuickWindow window;
    root->setParent(&window);
    root->setParentItem(window.contentItem());
    root->setWidth(550);
    root->setHeight(430);
    window.resize(550, 430);
    window.show();
    model.load();
    if (!paletteMatches(model)) result = 3;
    auto* theme = engine.singletonInstance<QObject*>("Bangke.Pages", "Theme");
    auto* background = root->findChild<QObject*>("candidateBackground");
    auto* highlight = root->findChild<QObject*>("candidateHighlight");
    if (!theme || !background || !highlight) return 7;
    const auto checkRenderedColors = [&](bool dark) {
      theme->setProperty("dark", dark);
      app.processEvents();
      const auto face = background->property("color").value<QColor>();
      const auto active = highlight->property("color").value<QColor>();
      std::cout << "rendered " << dark << ": " << face.name().toStdString()
                << " / " << active.name().toStdString() << '\n';
      return face == QColor(dark ? "#171c23" : "#f6f3ed") &&
             active == QColor(dark ? "#94c8bc" : "#785330");
    };
    if (!checkRenderedColors(false) || !checkRenderedColors(true)) result = 8;
    model.setColorScheme("other");
    model.setColorScheme("native");
    if (!paletteMatches(model)) result = 4;
    if (!checkRenderedColors(false) || !checkRenderedColors(true)) result = 9;

  }
  api->finalize();
  return result;
}
