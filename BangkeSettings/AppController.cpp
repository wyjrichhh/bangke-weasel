#include "AppController.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>

#include "Configurator.h"
#include <BangkeUtility.h>

namespace {
// 页面与 mac 设置窗一致:外观/行为/智能联想照抄,「方案与词典」是 Windows
// 特有(mac 上在菜单栏),「关于」收版本与文件夹入口。
const struct {
  const char* title;
  const char* sub;
} kPages[] = {
    {"外观", ""},
    {"行为", "菜单栏与输入行为。"},
    {"智能联想", "各输入方案的模型联想。"},
    {"方案与词典", "输入方案选单与用户词典管理。"},
    {"关于", ""},
};
}  // namespace

AppController::AppController(Configurator* configurator, bool openDictPage,
                             QObject* parent)
    : QObject(parent), configurator_(configurator) {
  general_ = new GeneralModel(this);
  style_ = new StyleModel(this);
  ai_ = new AiModel(this);
  switcher_ = new SwitcherModel(this);
  dict_ = new DictModel(this);

  // Configurator 是纯 C++ 的,提示只能靠信号转到 QML
  connect(configurator_, &Configurator::info, this, &AppController::info);
  connect(switcher_, &SwitcherModel::info, this, &AppController::info);
  connect(dict_, &DictModel::info, this, &AppController::info);

  // 五页布局: 0=外观 1=行为 2=智能联想 3=方案与词典 4=关于
  pageIndex_ = openDictPage ? 3 : 0;
  // 首屏载入推到事件循环里:本对象先于 QQmlApplicationEngine 构造,构造期
  // 载入若撞上部署互斥,BeginDictSession 发的提示还没有 QML 接收者,会丢
  QMetaObject::invokeMethod(
      this, [this] { onPageChanged(pageIndex_); }, Qt::QueuedConnection);
}

AppController::~AppController() {
  dict_->setSessionActive(false);
}

QStringList AppController::pageTitles() const {
  QStringList out;
  for (const auto& p : kPages)
    out << QString::fromUtf8(p.title);
  return out;
}

QStringList AppController::pageSubs() const {
  QStringList out;
  for (const auto& p : kPages)
    out << QString::fromUtf8(p.sub);
  return out;
}

QString AppController::appVersion() const {
  return QStringLiteral("%1.%2.%3 (%4)")
      .arg(VERSION_MAJOR)
      .arg(VERSION_MINOR)
      .arg(VERSION_PATCH)
      .arg(VERSION_BUILD);
}

void AppController::setPageIndex(int index) {
  if (index == pageIndex_)
    return;
  pageIndex_ = index;
  emit pageIndexChanged();
  onPageChanged(index);
}

void AppController::onPageChanged(int index) {
  dict_->setSessionActive(false);
  // 离开方案与词典页立即结束部署会话:BeginDictSession 拿住的
  // BangkeDeployerMutex 若握到关窗,之后点「保存并应用」必撞"另一项部署任务"
  configurator_->EndDictSession();
  // 各页首次进入才载入,此后不重读——重读会冲掉跨页的未保存草稿
  // (mac 同语义:Restore Defaults 只改草稿,保存前切页不丢)。词典会话例外:
  // 每次进页都要重建,方案表与词典列表可能在部署后变了
  if (index == 3) {
    switcher_->load();
    if (configurator_->BeginDictSession())
      dict_->setSessionActive(true);
  } else if (index == 0) {
    style_->load();
  } else if (index == 1) {
    general_->load();
  } else if (index == 2) {
    ai_->load();
  }
}

void AppController::setSaving(bool value) {
  if (saving_ == value)
    return;
  saving_ = value;
  emit savingChanged();
}

void AppController::restoreDefaults() {
  // 先确保三个模型已载入:save 的 loaded_ 守卫会把「没载入的页」判成无改动
  // 而静默跳过,恢复默认就白做了(通用/外观/智能联想都会被保存连坐)
  style_->load();
  general_->load();
  ai_->load();
  style_->resetDefaults();
  general_->resetDefaults();
  ai_->resetAll();
  emit toast(QStringLiteral(u"已恢复默认,保存后生效"));
}

void AppController::saveAndDeploy() {
  // 部署独占:词典会话握着维护握手,叠加部署会令服务端二次 Finalize
  // rime(双杀),先收掉,完事在原页重建
  dict_->setSessionActive(false);
  configurator_->EndDictSession();

  bool modified = switcher_->save();
  modified = style_->save() || modified;
  modified = general_->save() || modified;
  modified = ai_->save() || modified;

  if (!modified) {
    emit toast(QStringLiteral(u"没有需要保存的修改"));
    return;
  }

  // 部署期间禁用保存,防连点撞部署互斥
  setSaving(true);
  QCoreApplication::processEvents();

  // UpdateWorkspace(true) 内部对'已有部署在跑'等场景已发提示,
  // 这里只对 >1 的意外失败码追加告警(1=mutex 冲突,信息已展示)
  const int ret = configurator_->UpdateWorkspace(true);
  setSaving(false);
  if (ret > 1) {
    emit info(QStringLiteral(u"蚌壳拼音"),
              QStringLiteral(u"保存失败(错误码 %1)。\n"
                             u"配置可能写入有误,请检查用户文件夹中的 yaml 文件。")
                  .arg(ret));
    return;
  }
  if (ret == 0) {
    emit toast(QStringLiteral(u"已保存,输入法即刻生效"));
  }

  if (pageIndex_ == 3 && configurator_->BeginDictSession())
    dict_->setSessionActive(true);
}

void AppController::openUserFolder() const {
  QDesktopServices::openUrl(QUrl::fromLocalFile(
      QString::fromStdWString(BangkeUserDataPath().wstring())));
}

void AppController::openLogFolder() const {
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdWString(BangkeLogPath().wstring())));
}
