#pragma once

#include <QByteArray>
#include <QMainWindow>

class Configurator;
class QLabel;
class QListWidget;
class QStackedWidget;
class AIPage;
class GeneralPage;
class SwitcherPage;
class StylePage;
class DictPage;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(Configurator* configurator, bool openDictPage = false,
             QWidget* parent = nullptr);
  ~MainWindow() override;

 protected:
  void closeEvent(QCloseEvent* event) override;
  bool nativeEvent(const QByteArray& eventType, void* message,
                   qintptr* result) override;

 private slots:
  void onPageChanged(int index);
  void saveAndDeploy();

 private:
  Configurator* configurator_ = nullptr;
  QListWidget* nav_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  QLabel* pageTitle_ = nullptr;
  QLabel* pageSub_ = nullptr;
  AIPage* aiPage_ = nullptr;
  GeneralPage* generalPage_ = nullptr;
  SwitcherPage* switcherPage_ = nullptr;
  StylePage* stylePage_ = nullptr;
  DictPage* dictPage_ = nullptr;
};
