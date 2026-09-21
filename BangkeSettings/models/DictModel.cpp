#include "DictModel.h"

#include <QDir>
#include <QProcess>

#include <string>

#include <BangkeUtility.h>
#include <windows.h>

namespace {

void RevealInExplorer(const QString& path) {
  QProcess::startDetached("explorer",
                          {"/select,", QDir::toNativeSeparators(path)});
}

std::string userSyncDir() {
  char dir[MAX_PATH] = {0};
  rime_get_api()->get_user_data_sync_dir(dir, _countof(dir));
  return std::string(dir);
}

}  // namespace

DictModel::DictModel(QObject* parent) : QObject(parent) {
  api_ = leversApi();
}

QString DictModel::currentDictName() const {
  if (currentIndex_ < 0 || currentIndex_ >= dicts_.size())
    return QString();
  return dicts_.at(currentIndex_);
}

void DictModel::setCurrentIndex(int value) {
  if (currentIndex_ == value)
    return;
  currentIndex_ = value;
  emit currentIndexChanged();
}

void DictModel::load() {
  populate();
}

void DictModel::setSessionActive(bool active) {
  sessionActive_ = active;
  if (active)
    populate();
}

void DictModel::populate() {
  dicts_.clear();
  if (sessionActive_ && api_) {
    RimeUserDictIterator iter = {0};
    api_->user_dict_iterator_init(&iter);
    while (const char* dict = api_->next_user_dict(&iter))
      dicts_ << QString::fromStdString(dict);
    api_->user_dict_iterator_destroy(&iter);
  }
  // 原 QListWidget::clear 后 currentRow 回到 -1
  currentIndex_ = -1;
  emit dictsChanged();
  emit currentIndexChanged();
}

void DictModel::backup() {
  const QString dict = currentDictName();
  if (dict.isEmpty())
    return;
  const std::string dir = userSyncDir();
  if (dir.empty()) {
    emit info(QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"无法定位同步目录。"));
    return;
  }
  const QString path = QDir::toNativeSeparators(
      QString::fromStdString(dir) + "\\" + dict + ".userdb.txt");
  if (!api_->backup_user_dict(dict.toStdString().c_str())) {
    emit info(QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"备份失败。"));
    return;
  }
  RevealInExplorer(path);
}

void DictModel::chooseAndRestore() {
  emit restoreRequested(QString());
}

void DictModel::applyRestore(const QString& path) {
  if (path.isEmpty())
    return;
  if (!api_->restore_user_dict(path.toStdString().c_str()))
    emit info(QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"恢复失败。"));
  else
    emit info(QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"恢复完成。"));
}

void DictModel::chooseAndExport() {
  const QString dict = currentDictName();
  if (dict.isEmpty())
    return;
  emit exportRequested(dict + QStringLiteral("_export.txt"));
}

void DictModel::applyExport(const QString& path) {
  const QString dict = currentDictName();
  if (dict.isEmpty() || path.isEmpty())
    return;
  const int result = api_->export_user_dict(dict.toStdString().c_str(),
                                            path.toStdString().c_str());
  if (result < 0) {
    emit info(QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"导出失败。"));
    return;
  }
  emit info(QStringLiteral(u"蚌壳拼音"),
            QStringLiteral(u"导出 %1 条记录。").arg(result));
  RevealInExplorer(path);
}

void DictModel::chooseAndImport() {
  const QString dict = currentDictName();
  if (dict.isEmpty())
    return;
  emit importRequested(dict + QStringLiteral("_export.txt"));
}

void DictModel::applyImport(const QString& path) {
  const QString dict = currentDictName();
  if (dict.isEmpty() || path.isEmpty())
    return;
  const int result = api_->import_user_dict(dict.toStdString().c_str(),
                                            path.toStdString().c_str());
  if (result < 0) {
    emit info(QStringLiteral(u"蚌壳拼音"), QStringLiteral(u"导入失败。"));
    return;
  }
  emit info(QStringLiteral(u"蚌壳拼音"),
            QStringLiteral(u"导入 %1 条记录。").arg(result));
}
