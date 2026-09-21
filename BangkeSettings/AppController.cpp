#include "AppController.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>

#include "Configurator.h"
#include <BangkeUtility.h>

namespace {
// 页头文案与侧栏条目同一份来源 —— 分开写迟早对不上
const struct {
  const char* title;
  const char* sub;
} kPages[] = {
    {"通用设置", "候选数量、Shift 键行为与切换提示"},
    {"界面样式", "配色、字体、候选窗排列与排版微调"},
    {"AI 预测", "模型、触发条件与候选插入方式"},
    {"高级", "方案选单、用户词典与快捷入口"},
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

  // 四页布局: 0=通用 1=界面样式 2=AI 3=高级(方案+词典)
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

void AppController::setPageIndex(int index) {
  if (index == pageIndex_)
    return;
  pageIndex_ = index;
  emit pageIndexChanged();
  onPageChanged(index);
}

void AppController::onPageChanged(int index) {
  dict_->setSessionActive(false);
  // 离开高级页立即结束部署会话:BeginDictSession 拿住的 BangkeDeployerMutex
  // 若握到关窗,之后点「保存并重新部署」必撞"另一项部署任务"
  configurator_->EndDictSession();
  // 进入即重读配置(含外部/部署后的变更),未保存的改动随之丢弃
  if (index == 3) {
    switcher_->load();
    if (configurator_->BeginDictSession())
      dict_->setSessionActive(true);
  } else if (index == 0) {
    general_->load();
  } else if (index == 1) {
    style_->forceLoad();
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

void AppController::saveAndDeploy() {
  // 部署独占:高级页词典会话握着维护握手,叠加部署会令服务端
  // 二次 Finalize rime(双杀),先收掉,完事在原页重建
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
