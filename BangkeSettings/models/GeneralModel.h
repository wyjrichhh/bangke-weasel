#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

#include <rime_levers_api.h>

// 通用配置页:经 rime levers 写 default.custom.yaml
// (menu/page_size、ascii_composer/switch_key),与 SwitcherModel 共写一文件。
class GeneralModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY changed)
  Q_PROPERTY(QString shiftL READ shiftL WRITE setShiftL NOTIFY changed)
  Q_PROPERTY(QString shiftR READ shiftR WRITE setShiftR NOTIFY changed)
  Q_PROPERTY(bool commentHints READ commentHints WRITE setCommentHints NOTIFY changed)
  Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY changed)
  Q_PROPERTY(int notificationsTime READ notificationsTime WRITE setNotificationsTime NOTIFY changed)
  // 两个 Shift 下拉的可选项,label 用于显示,value 是写进配置的键值
  Q_PROPERTY(QVariantList shiftActions READ shiftActions CONSTANT)

 public:
  explicit GeneralModel(QObject* parent = nullptr);

  int pageSize() const { return pageSize_; }
  void setPageSize(int value) { pageSize_ = value; emit changed(); }

  QString shiftL() const { return shiftL_; }
  void setShiftL(const QString& value) { shiftL_ = value; emit changed(); }

  QString shiftR() const { return shiftR_; }
  void setShiftR(const QString& value) { shiftR_ = value; emit changed(); }

  bool commentHints() const { return commentHints_; }
  void setCommentHints(bool value) { commentHints_ = value; emit changed(); }

  bool notifications() const { return notifications_; }
  void setNotifications(bool value) { notifications_ = value; emit changed(); }

  int notificationsTime() const { return notificationsTime_; }
  void setNotificationsTime(int value) {
    notificationsTime_ = value;
    emit changed();
  }

  QVariantList shiftActions() const;

  // 进入页面即重读(含外部/部署后的变更),未保存的改动随之丢弃
  Q_INVOKABLE void load();
  bool save();

 signals:
  void changed();

 private:
  RimeLeversApi* api_ = nullptr;
  RimeCustomSettings* settings_ = nullptr;
  // 注释显示是 weasel 样式键,写 weasel.custom.yaml(与 default 分开)
  RimeCustomSettings* bangkeStyle_ = nullptr;

  // 下列成员的初值 = 原控件构造后的默认值(下拉停在第 0 项)
  int pageSize_ = 5;
  QString shiftL_ = QStringLiteral("commit_code");
  QString shiftR_ = QStringLiteral("commit_code");
  bool commentHints_ = false;
  bool notifications_ = false;
  int notificationsTime_ = 0;

  // 载入时的原值(Shift 空串 = custom yaml 无此键);保存只写有变化的键
  int initPs_ = 0;
  QString initShiftL_;
  QString initShiftR_;
  bool initCommentHints_ = false;
  bool initNotifications_ = false;
  int initNotificationsTime_ = 0;
  // 载过值才允许保存：init* 都是载入时记下的,没载过就还是 0/空,与当前值不等,
  // 保存会把默认值写进配置(见 StyleModel::save 同类事故)
  bool loaded_ = false;
};
