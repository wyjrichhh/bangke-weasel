#pragma once

#include <QObject>
#include <QString>

// 提示（原先直接弹 QMessageBox）改走信号：GUI 侧由 AppController 转给 QML，
// CLI 侧（/deploy /sync /install）由 main 退回 Win32 对话框。
class Configurator : public QObject {
  Q_OBJECT

 public:
  Configurator();
  ~Configurator();

  void Initialize();
  int UpdateWorkspace(bool report_errors = false);
  // 轻量生效:仅维护握手让服务端重读配置(秒级);样式/行为类改动够用,
  // 词典与 schema 不重编。影响 schema 编译的改动(方案列表/AI 配置)仍需
  // UpdateWorkspace
  int ReloadService();
  int SyncUserData();
  // 全新安装的用户目录 AI 初始化(/install):写接线 custom yaml、铺模型。
  // 幂等:只补缺,不覆盖用户已有文件
  int EnsureAiDefaults();
  // 把本输入法加进当前用户的输入法列表(/install)。幂等。
  // 机器层面的注册由 MSI 的 RegisterTSF(regsvr32) 做,但那一步不会把它加进用户的列表 ——
  // 而大版本升级是先卸后装,卸载会把用户列表里那条一起带走,于是装完 Win+Space 里没有它。
  int EnsureInputMethodRegistered(bool setDefault = false);
  // 卸载残留清理：以 MSI SYSTEM 令牌运行，不触碰用户数据目录，故为静态
  static int CleanupResidue();
  // 安装前清掉指向本产品文件的挂起重启删除：否则卸载后未重启就重装，
  // 下次重启会删掉新装好的文件
  static int ClearPendingDeletes();

  // 词典管理以 maintenance 会话包住（与旧 Deployer 的 DictManagement 等价），
  // 窗口切走时必须调 EndDictSession 恢复服务
  bool BeginDictSession();
  void EndDictSession();

 signals:
  void info(const QString& title, const QString& body);

 private:
  void* m_hDictMutex = nullptr;
};
