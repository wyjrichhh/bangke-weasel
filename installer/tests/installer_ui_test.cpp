// Exercise the production widget tree without starting any installation job.
#define main InstallerApplicationMain
#include "../BangkeInstaller/main.cpp"
#undef main
#include <iostream>

static void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setStyle("Fusion");
  const QString output = QDir::currentPath() + QStringLiteral("/ui-review");
  Require(QDir().mkpath(output), "Cannot create review directory");
  Require(QFile::exists(QStringLiteral(":/installer/bangke-appicon.png")), "Logo resource missing");
  Require(QFile::exists(QStringLiteral(":/installer/LICENSE.txt")), "License resource missing");
  MainWindow window;
  window.show();
  QEventLoop entrance;
  QTimer::singleShot(500, &entrance, &QEventLoop::quit);
  entrance.exec();
  auto* stack = window.findChild<QStackedWidget*>();
  Require(stack && stack->count() == 3, "Installer pages missing");
  // Render a fresh-install fixture; no MSI or registry is written.
  window.findChild<QFrame*>("freshInstall")->show();
  window.findChild<QFrame*>("installedActions")->hide();
  window.findChild<QLabel*>("warn")->hide();
  window.findChild<QLabel*>("facts")->setText(QStringLiteral(u"安装版本  0.4.51  ·  Windows 64 位"));
  for (bool dark : {false, true}) {
    ApplyInstallerTheme(dark);
    Require(app.palette().color(QPalette::Highlight) == QColor(dark ? "#94C8BC" : "#785330"),
            "Incorrect theme accent");
    for (const QSize size : {QSize(996, 634), QSize(936, 614)}) {
      window.resize(size);
      for (int page = 0; page < stack->count(); ++page) {
        stack->setCurrentIndex(page);
        if (page == 1) {
          auto* heading = stack->currentWidget()->findChild<QLabel*>("finishTitle");
          heading->setText(QStringLiteral(u"正在安装蚌壳拼音"));
        }
        if (page == 2) {
          stack->currentWidget()->findChild<QLabel*>("finishTitle")->setText(QStringLiteral(u"安装完成"));
          stack->currentWidget()->findChild<QLabel*>("sub")->setText(QStringLiteral(u"按 Win + 空格，切换到蚌壳拼音开始输入。"));
        }
        QCoreApplication::processEvents();
        auto* current = stack->currentWidget();
        for (auto* button : current->findChildren<QPushButton*>()) {
          if (!button->isVisible()) continue;
          const QRect bounds(button->mapTo(current, QPoint()), button->size());
          Require(current->rect().contains(bounds), "Action button clipped outside page");
          const int padding = button->objectName() == QStringLiteral("textLink") ? 8 : 36;
          if (button->fontMetrics().horizontalAdvance(button->text()) + padding > button->width()) {
            std::cerr << "Clipped button: " << button->text().toStdString()
                      << ", width=" << button->width() << '\n';
            Require(false, "Action text clipped");
          }
        }
        const QString path = output + QStringLiteral("/installer-%1-%2-%3.png")
            .arg(dark ? "dark" : "light").arg(size.width()).arg(page);
        Require(window.grab().save(path), "Screenshot save failed");
      }
    }
  }
  std::cout << "PASS: production UI, embedded assets, both themes, all pages and minimum size\n";
  return 0;
}
