#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QObject>
#include <QString>

#include <vector>

#include "Levers.h"

// 方案选单:可选方案列表 + 每项的启用状态。
// 本类自己就是 QML 的列表模型(model: app.switcher),角色见 roleNames;
// 勾选只落在 modified_ 上,保存时才重读并写回。
class SwitcherModel : public QAbstractListModel {
  Q_OBJECT

 public:
  enum Role {
    NameRole = Qt::UserRole + 1,
    SchemaIdRole,
    EnabledRole,
  };
  Q_ENUM(Role)

  explicit SwitcherModel(QObject* parent = nullptr);
  ~SwitcherModel() override;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role) override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;
  QHash<int, QByteArray> roleNames() const override;

  // 首次进入页面时加载;切页往返不重置未保存的勾选
  Q_INVOKABLE void load();
  // 丢弃当前列表强制重读(刷新 / 部署后同步)
  Q_INVOKABLE void refresh();
  // 返回是否有改动被写入
  Q_INVOKABLE bool save();
  bool saveSucceeded() const { return saveSucceeded_; }

 signals:
  void info(const QString& title, const QString& body);

 private:
  bool saveSucceeded_ = true;
  void loadSettings();
  void populate();

  RimeLeversApi* api_ = nullptr;
  RimeSwitcherSettings* settings_ = nullptr;
  RimeSchemaList available_ = {0};

  struct SchemaRowRec {
    RimeSchemaInfo* info = nullptr;
    QString id;
    QString name;
    bool enabled = false;
    bool savedEnabled = false;
  };
  std::vector<SchemaRowRec> rows_;
  bool loaded_ = false;
  bool modified_ = false;
};
