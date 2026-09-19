#include <QApplication>
#include <QMessageBox>
#include <windows.h>

#include "MainWindow.h"
#include "Configurator.h"
#include "Theme.h"
#include <BangkeUtility.h>

namespace {

enum class Mode { Gui, Deploy, Sync, Install, Dict, Cleanup, ClearPending, Help };

Mode parseMode(const QString& arg) {
  QString a = arg;
  if (a.startsWith('/') || a.startsWith('-'))
    a.remove(0, 1);
  a = a.toLower();
  if (a == "deploy")
    return Mode::Deploy;
  if (a == "sync")
    return Mode::Sync;
  if (a == "install")
    return Mode::Install;
  if (a == "dict")
    return Mode::Dict;
  if (a == "cleanup")
    return Mode::Cleanup;
  if (a == "clearpending")
    return Mode::ClearPending;
  if (a == "?" || a == "help")
    return Mode::Help;
  return Mode::Gui;
}

void showUsage(QWidget* parent) {
  QMessageBox::information(
      parent, QStringLiteral(u"蚌壳拼音·设置"),
      QStringLiteral(u"用法: BangkeSettings.exe [选项]\n"
                     u"/deploy  - 重新部署\n"
                     u"/dict    - 词典管理\n"
                     u"/sync    - 同步用户数据\n"
                     u"/install - 静默首次部署（安装器调用）\n"
                     u"/cleanup - 卸载残留清理（安装器调用）\n"
                     u"/clearpending - 清除指向本产品的挂起重启删除（安装器调用）\n"
                     u"/?       - 显示本帮助"));
}

}  // namespace

int main(int argc, char* argv[]) {
  const Mode mode0 = argc > 1 ? parseMode(QString::fromLocal8Bit(argv[1])) : Mode::Gui;
  // 单实例互斥只约束 GUI;CLI(/install /deploy 等)曾被它静默挡掉
  // (退出码 1 恰好是 CA 的"成功"),种子从没真正跑过
  HANDLE hMutex = nullptr;
  if (mode0 == Mode::Gui) {
    hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerExclusiveMutex");
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
      return 1;
  }

  int ret = 0;
  {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral(u"蚌壳拼音·设置"));
    QApplication::setOrganizationName(QStringLiteral(u"Bangke"));
    Theme::Apply(app);

    Mode mode = argc > 1 ? parseMode(QString::fromLocal8Bit(argv[1])) : Mode::Gui;

    // /cleanup /clearpending 在 MSI 序列里以 SYSTEM 令牌运行：提前返回，
    // 不构造 Configurator（其构造器/Initialize 会按 HKCU 定位用户目录，SYSTEM 下指向错误位置）
    if (mode == Mode::Cleanup)
      return Configurator::CleanupResidue();
    if (mode == Mode::ClearPending)
      return Configurator::ClearPendingDeletes();

    Configurator configurator;
    configurator.Initialize();

    switch (mode) {
      case Mode::Help:
        showUsage(nullptr);
        break;
      case Mode::Deploy:
        ret = configurator.UpdateWorkspace();
        break;
      case Mode::Install:
        // 首部署:先补 AI 接线与模型,再部署使其生效
        configurator.EnsureAiDefaults();
        // 同时把它加进用户的输入法列表——注册那一步不会做,而升级是先卸后装
        configurator.EnsureInputMethodRegistered();
        ret = configurator.UpdateWorkspace();
        break;
      case Mode::Sync:
        ret = configurator.SyncUserData();
        break;
      case Mode::Dict:
      case Mode::Gui: {
        MainWindow window(&configurator, mode == Mode::Dict);
        window.show();
        ret = app.exec();
        break;
      }
    }
  }

  if (hMutex)
    CloseHandle(hMutex);
  return ret;
}
