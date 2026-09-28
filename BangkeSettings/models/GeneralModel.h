#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>

#include <rime_levers_api.h>

#include "../UIStyleSettings.h"

// 行为页:输入类经 rime levers 写 default.custom.yaml
// (menu/page_size、ascii_composer/switch_key),与 SwitcherModel 共写一文件;
// 托盘/通知/拼音注释是 weasel 样式键,读 = 补丁 ⊕ 共享默认,写 = 补丁。
class GeneralModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY changed)
  Q_PROPERTY(QString shiftL READ shiftL WRITE setShiftL NOTIFY changed)
  Q_PROPERTY(QString shiftR READ shiftR WRITE setShiftR NOTIFY changed)
  Q_PROPERTY(bool commentHints READ commentHints WRITE setCommentHints NOTIFY changed)
  // never / appropriate / always:写 false / [schema] / true(引擎的顶层
  // show_notifications 支持布尔与事件名列表两种形态)
  Q_PROPERTY(QString notificationsMode READ notificationsMode WRITE setNotificationsMode NOTIFY changed)
  // 两个 Shift 下拉的可选项,label 用于显示,value 是写进配置的键值
  Q_PROPERTY(QVariantList shiftActions READ shiftActions CONSTANT)
  Q_PROPERTY(QVariantList notificationOptions READ notificationOptions CONSTANT)

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

  QString notificationsMode() const { return notificationsMode_; }
  void setNotificationsMode(const QString& value) {
    notificationsMode_ = value;
    emit changed();
  }

  QVariantList shiftActions() const;
  QVariantList notificationOptions() const;

  // 首次进入才载;载入后不重读,草稿跨页保持
  Q_INVOKABLE void load();
  bool save();
  // 恢复默认:草稿回到出厂值(仍需保存才落盘)
  Q_INVOKABLE void resetDefaults();

 signals:
  void changed();

 private:
  RimeLeversApi* api_ = nullptr;
  RimeCustomSettings* settings_ = nullptr;
  // 注释显示/托盘/通知是 weasel 样式键,经 UIStyleSettings 读合并默认值
  RimeCustomSettings* bangkeStyle_ = nullptr;
  UIStyleSettings style_;

  // 下列成员的初值 = 出厂默认(与 data/weasel.yaml / default.yaml 对应)
  int pageSize_ = 5;
  QString shiftL_ = QStringLiteral("commit_code");
  QString shiftR_ = QStringLiteral("commit_code");
  bool commentHints_ = true;
  QString notificationsMode_ = QStringLiteral("never");

  // 载入时的原值(Shift 空串 = custom yaml 无此键);保存只写有变化的键
  int initPs_ = 0;
  QString initShiftL_;
  QString initShiftR_;
  bool initCommentHints_ = false;
  QString initNotifications_ = QStringLiteral("never");
  // 载过值才允许保存：init* 都是载入时记下的,没载过就与当前值不等,
  // 保存会把默认值写进配置(见 StyleModel::save 同类事故)
  bool loaded_ = false;
};
