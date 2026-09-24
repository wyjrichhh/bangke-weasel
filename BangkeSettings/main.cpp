#include <string>

#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QUrl>

#include "AppController.h"
#include "Configurator.h"
#include "SystemTheme.h"
#include "WinChrome.h"
// 放在项目头之后:AppController.h 会带进 rime_api.h,先 windows.h 会撞宏重定义(4005)
#include <windows.h>

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

// 无 Widgets 之后的模态提示。CLI 模式下没法用 QML,退回 Win32 原生框
void showMessage(const QString& title, const QString& body) {
  const std::wstring t = title.toStdWString();
  const std::wstring b = body.toStdWString();
  MessageBoxW(nullptr, b.c_str(), t.c_str(), MB_OK | MB_ICONINFORMATION);
}

void showUsage() {
  showMessage(QStringLiteral(u"蚌壳拼音·设置"),
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
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral(u"蚌壳拼音·设置"));
    QGuiApplication::setOrganizationName(QStringLiteral("Bangke"));
    // 标题栏图标:QML 的 Window 没设 icon 就落回 Windows 默认应用图标
    app.setWindowIcon(QIcon(QStringLiteral(":/bangke.ico")));

    Mode mode = argc > 1 ? parseMode(QString::fromLocal8Bit(argv[1])) : Mode::Gui;

    // /cleanup /clearpending 在 MSI 序列里以 SYSTEM 令牌运行：提前返回，
    // 不构造 Configurator（其构造器/Initialize 会按 HKCU 定位用户目录，SYSTEM 下指向错误位置）
    if (mode == Mode::Cleanup)
      return Configurator::CleanupResidue();
    if (mode == Mode::ClearPending)
      return Configurator::ClearPendingDeletes();

    Configurator configurator;
    configurator.Initialize();

    // 非 GUI 模式没有 QML 承接提示,退回 Win32 框
    if (mode != Mode::Gui && mode != Mode::Dict)
      QObject::connect(&configurator, &Configurator::info, &configurator,
                       [](const QString& title, const QString& body) {
                         showMessage(title, body);
                       });

    switch (mode) {
      case Mode::Help:
        showUsage();
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
        // 屏幕上的字号与字体:QML 里没显式指定的文本跟这一份。
        // families 回退链 QML 侧给不了(font 没有 families 组属性),在这配:
        // 西文 Segoe UI,中文逐字回落微软雅黑
        QFont appFont;
        appFont.setFamilies({QStringLiteral("Segoe UI"),
                             QStringLiteral("Microsoft YaHei UI")});
        appFont.setPixelSize(13);
        app.setFont(appFont);
        // 落到 Basic:QML 靠每个实例覆写 background/indicator 拿真投影与焦点环,
        // 换 style 会把这些盖掉
        QQuickStyle::setStyle(QStringLiteral("Basic"));

        SystemTheme sysTheme;
        AppController controller(&configurator, mode == Mode::Dict);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("app"),
                                                 &controller);
        engine.rootContext()->setContextProperty(QStringLiteral("sysTheme"),
                                                 &sysTheme);
        // 入口 = 模块名 + 主组件名,与 CMakeLists 的 qt_add_qml_module 一致
        engine.loadFromModule(QStringLiteral("Bangke.Pages"),
                              QStringLiteral("Main"));
        // 兜底:qmldir 解析不到时按 qrc 路径直载(源文件在 qml/ 子目录下走的就是这条)
        if (engine.rootObjects().isEmpty())
          engine.load(QUrl(QStringLiteral(
              "qrc:/qt/qml/Bangke/Pages/qml/Main.qml")));
        if (!engine.rootObjects().isEmpty()) {
          // frameless 窗口的拖动/缩放/圆角/acrylic 在 Win32 侧,只服务 GUI 主窗
          static WinChrome winChrome;
          if (auto* window =
                  qobject_cast<QQuickWindow*>(engine.rootObjects().first()))
            winChrome.attach(window, &sysTheme);
        }
        if (engine.rootObjects().isEmpty())
          ret = 1;
        else
          ret = app.exec();
        break;
      }
    }
  }

  if (hMutex)
    CloseHandle(hMutex);
  return ret;
}
