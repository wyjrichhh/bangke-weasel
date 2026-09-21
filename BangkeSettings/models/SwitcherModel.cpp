#include "SwitcherModel.h"

#include <cstring>
#include <set>
#include <string>

#include <BangkeUtility.h>
#include <rime_levers_api.h>

SwitcherModel::SwitcherModel(QObject* parent) : QAbstractListModel(parent) {
  api_ = leversApi();
}

SwitcherModel::~SwitcherModel() {
  if (available_.list)
    api_->schema_list_destroy(&available_);
  if (settings_)
    api_->custom_settings_destroy((RimeCustomSettings*)settings_);
}

int SwitcherModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : (int)rows_.size();
}

QVariant SwitcherModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= (int)rows_.size())
    return QVariant();
  const SchemaRowRec& r = rows_[index.row()];
  switch (role) {
    case NameRole:
      return r.name;
    case SchemaIdRole:
      return r.id;
    case EnabledRole:
      return r.enabled;
    default:
      return QVariant();
  }
}

bool SwitcherModel::setData(const QModelIndex& index, const QVariant& value,
                            int role) {
  if (role != EnabledRole || !index.isValid() || index.row() < 0 ||
      index.row() >= (int)rows_.size())
    return false;
  rows_[index.row()].enabled = value.toBool();
  modified_ = true;
  const QModelIndex idx = this->index(index.row());
  emit dataChanged(idx, idx, {EnabledRole});
  return true;
}

Qt::ItemFlags SwitcherModel::flags(const QModelIndex& index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QHash<int, QByteArray> SwitcherModel::roleNames() const {
  return {
      {NameRole, "name"},
      {SchemaIdRole, "schemaId"},
      {EnabledRole, "enabled"},
  };
}

void SwitcherModel::load() {
  if (loaded_)
    return;
  refresh();
}

void SwitcherModel::refresh() {
  if (!api_)
    return;
  if (!settings_)
    settings_ = api_->switcher_settings_init();
  loadSettings();
  loaded_ = true;
}

void SwitcherModel::loadSettings() {
  api_->load_settings((RimeCustomSettings*)settings_);
  populate();
}

void SwitcherModel::populate() {
  if (!settings_)
    return;
  if (available_.list) {
    api_->schema_list_destroy(&available_);
    available_ = {0};
  }
  RimeSchemaList selected = {0};
  api_->get_available_schema_list(settings_, &available_);
  api_->get_selected_schema_list(settings_, &selected);

  beginResetModel();
  rows_.clear();
  auto addRow = [this](RimeSchemaListItem& item, RimeSchemaInfo* info,
                       bool checked) {
    SchemaRowRec rec;
    rec.info = info;
    rec.id = QString::fromUtf8(item.schema_id);
    rec.name = QString::fromStdString(item.name);
    rec.enabled = checked;
    rows_.push_back(rec);
  };
  std::set<void*> recruited;
  for (size_t i = 0; i < selected.size; ++i) {
    const char* schema_id = selected.list[i].schema_id;
    for (size_t j = 0; j < available_.size; ++j) {
      RimeSchemaListItem& item(available_.list[j]);
      RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
      if (!strcmp(item.schema_id, schema_id) &&
          recruited.find(info) == recruited.end()) {
        recruited.insert(info);
        addRow(item, info, true);
        break;
      }
    }
  }
  for (size_t i = 0; i < available_.size; ++i) {
    RimeSchemaListItem& item(available_.list[i]);
    RimeSchemaInfo* info = (RimeSchemaInfo*)item.reserved;
    if (recruited.find(info) == recruited.end()) {
      recruited.insert(info);
      addRow(item, info, false);
    }
  }
  endResetModel();

  api_->schema_list_destroy(&selected);
  modified_ = false;
}

bool SwitcherModel::save() {
  if (!modified_ || !settings_ || rows_.empty())
    return false;
  std::vector<const char*> selection;
  for (auto& rec : rows_) {
    if (!rec.enabled)
      continue;
    selection.push_back(api_->get_schema_id(rec.info));
  }
  if (selection.empty()) {
    emit info(QStringLiteral(u"蚌壳拼音"),
              QStringLiteral(u"至少要选用一项方案。"));
    return false;
  }
  // 重读再写:与 GeneralModel 共写 default.custom.yaml,避免旧树覆写对方
  api_->load_settings((RimeCustomSettings*)settings_);
  api_->select_schemas(settings_, selection.data(), (int)selection.size());
  return api_->save_settings((RimeCustomSettings*)settings_);
}
