#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "update/updatechecker.h"
#include "models/AiModel.h"
#include "models/DictModel.h"
#include "models/GeneralModel.h"
#include "models/StyleModel.h"
#include "models/SwitcherModel.h"

class Configurator;

// 外壳:页面切换时的载入时机、保存顺序、部署互斥。
// 这些顺序本身就是行为的一部分(见 onPageChanged/saveAndDeploy),与视图无关。
class AppController : public QObject {
  Q_OBJECT
  Q_PROPERTY(QObject* updates READ updates CONSTANT)
  Q_PROPERTY(int pageIndex READ pageIndex WRITE setPageIndex NOTIFY pageIndexChanged)
  Q_PROPERTY(QStringList pageTitles READ pageTitles CONSTANT)
  Q_PROPERTY(QStringList pageSubs READ pageSubs CONSTANT)
  Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
  Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
  Q_PROPERTY(GeneralModel* general READ general CONSTANT)
  Q_PROPERTY(StyleModel* style READ style CONSTANT)
  Q_PROPERTY(AiModel* ai READ ai CONSTANT)
  Q_PROPERTY(SwitcherModel* switcher READ switcher CONSTANT)
  Q_PROPERTY(DictModel* dict READ dict CONSTANT)

 public:
  AppController(Configurator* configurator, bool openDictPage = false,
                QObject* parent = nullptr);
  ~AppController() override;

  QObject* updates() const { return updates_; }
  int pageIndex() const { return pageIndex_; }
  void setPageIndex(int index);

  QStringList pageTitles() const;
  QStringList pageSubs() const;
  bool saving() const { return saving_; }
  QString appVersion() const;

  GeneralModel* general() const { return general_; }
  StyleModel* style() const { return style_; }
  AiModel* ai() const { return ai_; }
  SwitcherModel* switcher() const { return switcher_; }
  DictModel* dict() const { return dict_; }

  Q_INVOKABLE void saveAndDeploy();
  // 恢复默认:外观/行为/智能联想的草稿回出厂值,仍需「保存并应用」落盘
  Q_INVOKABLE void restoreDefaults();
  // 关于页的文件夹入口
  Q_INVOKABLE void openUserFolder() const;
  Q_INVOKABLE void openLogFolder() const;

 signals:
  void pageIndexChanged();
  void savingChanged();
  void saved();
  // 非阻塞完成提示与模态告知,由 QML 呈现
  void toast(const QString& text);
  void info(const QString& title, const QString& body);

 private:
  void onPageChanged(int index);
  void setSaving(bool value);

  bangke::UpdateChecker* updates_ = nullptr;
  Configurator* configurator_ = nullptr;
  GeneralModel* general_ = nullptr;
  StyleModel* style_ = nullptr;
  AiModel* ai_ = nullptr;
  SwitcherModel* switcher_ = nullptr;
  DictModel* dict_ = nullptr;
  int pageIndex_ = 0;
  bool saving_ = false;
  bool pendingDeploy_ = false;
  bool pendingFullDeploy_ = false;
};
