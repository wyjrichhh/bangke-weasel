#pragma once

class Configurator {
 public:
  Configurator();
  ~Configurator();

  void Initialize();
  int UpdateWorkspace(bool report_errors = false);
  int SyncUserData();
  // 全新安装的用户目录 AI 初始化(/install):写接线 custom yaml、铺模型。
  // 幂等:只补缺,不覆盖用户已有文件
  int EnsureAiDefaults();
  // 把本输入法加进当前用户的输入法列表(/install)。幂等。
  // 机器层面的注册由 MSI 的 RegisterTSF(regsvr32) 做,但那一步不会把它加进用户的列表 ——
  // 而大版本升级是先卸后装,卸载会把用户列表里那条一起带走,于是装完 Win+Space 里没有它。
  int EnsureInputMethodRegistered();
  // 卸载残留清理：以 MSI SYSTEM 令牌运行，不触碰用户数据目录，故为静态
  static int CleanupResidue();
  // 安装前清掉指向本产品文件的挂起重启删除：否则卸载后未重启就重装，
  // 下次重启会删掉新装好的文件
  static int ClearPendingDeletes();

  // 词典管理以 maintenance 会话包住（与旧 Deployer 的 DictManagement 等价），
  // 窗口切走时必须调 EndDictSession 恢复服务
  bool BeginDictSession();
  void EndDictSession();

 private:
  void* m_hDictMutex = nullptr;
};
