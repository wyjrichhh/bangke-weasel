#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "Levers.h"

// 用户词典页:备份/恢复/导出/导入,四个动作都走 levers。
// 维护会话(互斥量 + server maintenance)由 AppController 切页时驱动;
// 没有会话就列不出词典。
class DictModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList dicts READ dicts NOTIFY dictsChanged)
  Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
  // 有选中项才能备份/导出/导入(与原三个按钮的 enable 一致)
  Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY currentIndexChanged)

 public:
  explicit DictModel(QObject* parent = nullptr);

  QStringList dicts() const { return dicts_; }
  int currentIndex() const { return currentIndex_; }
  void setCurrentIndex(int value);
  bool hasSelection() const { return currentIndex_ >= 0; }

  Q_INVOKABLE void load();
  Q_INVOKABLE void setSessionActive(bool active);

  // 备份固定落在同步目录,不用选路
  Q_INVOKABLE void backup();
  // 另外三个都要先选路,而选路在 QML 的文件对话框里:choose* 只发请求
  // (附上建议的文件名),apply* 拿到路径才落笔 —— 本模型里没有文件对话框。
  Q_INVOKABLE void chooseAndRestore();
  Q_INVOKABLE void applyRestore(const QString& path);
  Q_INVOKABLE void chooseAndExport();
  Q_INVOKABLE void applyExport(const QString& path);
  Q_INVOKABLE void chooseAndImport();
  Q_INVOKABLE void applyImport(const QString& path);

 signals:
  void dictsChanged();
  void currentIndexChanged();
  void restoreRequested(const QString& path);
  void exportRequested(const QString& path);
  void importRequested(const QString& path);
  void info(const QString& title, const QString& body);

 private:
  void populate();
  QString currentDictName() const;

  RimeLeversApi* api_ = nullptr;
  QStringList dicts_;
  int currentIndex_ = -1;
  bool sessionActive_ = false;
};
