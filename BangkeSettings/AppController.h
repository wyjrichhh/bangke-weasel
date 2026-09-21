#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

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
  Q_PROPERTY(int pageIndex READ pageIndex WRITE setPageIndex NOTIFY pageIndexChanged)
  Q_PROPERTY(QStringList pageTitles READ pageTitles CONSTANT)
  Q_PROPERTY(QStringList pageSubs READ pageSubs CONSTANT)
  Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
  Q_PROPERTY(GeneralModel* general READ general CONSTANT)
  Q_PROPERTY(StyleModel* style READ style CONSTANT)
  Q_PROPERTY(AiModel* ai READ ai CONSTANT)
  Q_PROPERTY(SwitcherModel* switcher READ switcher CONSTANT)
  Q_PROPERTY(DictModel* dict READ dict CONSTANT)

 public:
  AppController(Configurator* configurator, bool openDictPage = false,
                QObject* parent = nullptr);
  ~AppController() override;

  int pageIndex() const { return pageIndex_; }
  void setPageIndex(int index);

  QStringList pageTitles() const;
  QStringList pageSubs() const;
  bool saving() const { return saving_; }

  GeneralModel* general() const { return general_; }
  StyleModel* style() const { return style_; }
  AiModel* ai() const { return ai_; }
  SwitcherModel* switcher() const { return switcher_; }
  DictModel* dict() const { return dict_; }

  Q_INVOKABLE void saveAndDeploy();
  // 高级页的快捷入口
  Q_INVOKABLE void openUserFolder() const;
  Q_INVOKABLE void openLogFolder() const;

 signals:
  void pageIndexChanged();
  void savingChanged();
  // 非阻塞完成提示与模态告知,由 QML 呈现
  void toast(const QString& text);
  void info(const QString& title, const QString& body);

 private:
  void onPageChanged(int index);
  void setSaving(bool value);

  Configurator* configurator_ = nullptr;
  GeneralModel* general_ = nullptr;
  StyleModel* style_ = nullptr;
  AiModel* ai_ = nullptr;
  SwitcherModel* switcher_ = nullptr;
  DictModel* dict_ = nullptr;
  int pageIndex_ = 0;
  bool saving_ = false;
};
