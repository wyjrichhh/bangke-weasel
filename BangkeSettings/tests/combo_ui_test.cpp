#include <QtQuickTest/quicktest.h>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

class Setup : public QObject {
  Q_OBJECT
 public slots:
  void qmlEngineAvailable(QQmlEngine* engine) {
    QQmlComponent component(engine);
    component.setData(R"(import QtQml
      QtObject {
        property var pageTitles: ["Appearance", "Behavior"]
        property int pageIndex: 0
      })", QUrl());
    auto* app = component.create();
    if (!app)
      qFatal("Could not create settings test context");
    app->setParent(engine);
    engine->rootContext()->setContextProperty("app", app);
  }
};

QUICK_TEST_MAIN_WITH_SETUP(combo_ui, Setup)
#include "combo_ui_test.moc"
