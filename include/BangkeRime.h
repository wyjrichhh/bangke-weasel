#pragma once
#include <BangkeIPC.h>
#include <BangkeUI.h>
#include <map>
#include <string>
#include <mutex>

#include <rime_api.h>

struct CaseInsensitiveCompare {
  bool operator()(const std::string& str1, const std::string& str2) const {
    std::string str1Lower, str2Lower;
    std::transform(str1.begin(), str1.end(), std::back_inserter(str1Lower),
                   [](char c) { return std::tolower(c); });
    std::transform(str2.begin(), str2.end(), std::back_inserter(str2Lower),
                   [](char c) { return std::tolower(c); });
    return str1Lower < str2Lower;
  }
};

typedef std::map<std::string, bool> AppOptions;
typedef std::map<std::string, AppOptions, CaseInsensitiveCompare>
    AppOptionsByAppName;

struct SessionStatus {
  SessionStatus()
      : style(bangke::UIStyle()),
        __synced(false),
        session_id(0) {
    RIME_STRUCT(RimeStatus, status);
  }
  bangke::UIStyle style;
  RimeStatus status;
  bool __synced;
  RimeSessionId session_id;
  std::string client_app;  // 部署后按它恢复 ipc 会话
  // 纯 AI 推理（ai_predict/ai_only）：菜单只有模型那一条，词库不参与
  bool ai_only = false;
  // 上一帧送出去的候选数。纯推理模式下菜单空着就意味着「还没有预测可确认」，
  // 空格据此决定吞不吞（见 ProcessKeyEvent）
  size_t menu_candidates = 0;
  // 已经交给插件的上下文。值没变就不重写：set_property 每次都会触发插件重跑推理
  std::string published_context;
};
typedef std::map<DWORD, SessionStatus> SessionStatusMap;
typedef DWORD BangkeSessionId;
class BangkeRimeHandler : public bangke::RequestHandler {
 public:
  // rime API 与会话表的串行锁：管道路径(ServerImpl)与延迟事件消费
  // (OnDeferredEvent)共用。必须递归：process_key→Compose→filter 会在同一线程
  // 内同步触发属性通知。OnNotify 本身绝不取此锁——它可能跑在 rime 内部线程上，
  // 一旦等锁即可与销毁路径的 worker join 互等成死锁
  static std::recursive_mutex& ApiMutex() {
    static std::recursive_mutex m;
    return m;
  }

  BangkeRimeHandler(bangke::UI* ui);
  virtual ~BangkeRimeHandler();
  virtual void Initialize();

 private:
  // ignore_deployer:EndMaintenance 强起用——部署已完成,互斥尚未松手
  // 不构成障碍;启动路径仍要检查(真有部署器在场则等 AddSession 再试)
  void _Initialize(bool ignore_deployer);

 public:
  virtual void Finalize();
  virtual DWORD FindSession(BangkeSessionId ipc_id);
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0);
  virtual DWORD RemoveSession(BangkeSessionId ipc_id);
  virtual BOOL ProcessKeyEvent(bangke::KeyEvent keyEvent,
                               BangkeSessionId ipc_id,
                               EatLine eat);
  virtual void CommitComposition(BangkeSessionId ipc_id);
  virtual void ClearComposition(BangkeSessionId ipc_id);
  virtual void SelectCandidateOnCurrentPage(size_t index,
                                            BangkeSessionId ipc_id);
  virtual bool HighlightCandidateOnCurrentPage(size_t index,
                                               BangkeSessionId ipc_id,
                                               EatLine eat);
  virtual bool ChangePage(bool backward, BangkeSessionId ipc_id, EatLine eat);
  virtual void FocusIn(DWORD param, BangkeSessionId ipc_id);
  virtual void FocusOut(DWORD param, BangkeSessionId ipc_id);
  virtual void UpdateInputPosition(RECT const& rc, BangkeSessionId ipc_id);
  virtual void StartMaintenance();
  virtual void EndMaintenance();
  virtual void SetOption(BangkeSessionId ipc_id,
                         const std::string& opt,
                         bool val);
  void SetClientContext(BangkeSessionId ipc_id,
                        const std::wstring& text) override;
  virtual void UpdateColorTheme(BOOL darkMode);
  void SetEventWindow(HWND wnd) override;
  void OnDeferredEvent(int event, uintptr_t rime_session_id) override;
  // AI 泵的两半，只在服务端消息循环线程上跑（见 _StopAiPump 上方注释）
  void AiPumpArm(DWORD ipc_id) override;
  void AiPumpTick() override;

  void OnUpdateUI(std::function<void()> const& cb);

 private:
  void _Setup();
  bool _IsDeployerRunning();
  void _UpdateUI(BangkeSessionId ipc_id);
  void _LoadSchemaSpecificSettings(BangkeSessionId ipc_id,
                                   const std::string& schema_id);
  void _LoadAppInlinePreeditSet(BangkeSessionId ipc_id,
                                bool ignore_app_name = false);
  bool _ShowMessage(bangke::Context& ctx, bangke::Status& status);
  bool _Respond(BangkeSessionId ipc_id, EatLine eat, bool include_commit = true);
  // consume_style_sync=false 用于快照推送：那一帧走共享内存而非管道，客户端可能因
  // 序号陈旧而丢弃它，就此退休样式同步标记会连同样式一起丢掉
  bool _RespondFrame(BangkeSessionId ipc_id,
                     EatLine eat,
                     bool include_commit,
                     bool consume_style_sync = true);
  void _ReadClientInfo(BangkeSessionId ipc_id, LPWSTR buffer);
  void _RestoreSession(BangkeSessionId ipc_id, const std::string& client_app);
  void _GetCandidateInfo(bangke::CandidateInfo& cinfo, RimeContext& ctx);
  void _GetStatus(bangke::Status& stat,
                  BangkeSessionId ipc_id,
                  bangke::Context& ctx);
  void _GetContext(bangke::Context& ctx, RimeSessionId session_id);
  void _UpdateShowNotifications(RimeConfig* config, bool initialize = false);

  void _UpdateInlinePreeditStatus(BangkeSessionId ipc_id);

  void _StopAiPump();
  bool _IsAiPending(RimeSessionId session_id);
  void _MaybeRequestAiPump(BangkeSessionId ipc_id, RimeSessionId session_id);

  RimeSessionId to_session_id(BangkeSessionId ipc_id) {
    return m_session_status_map[ipc_id].session_id;
  }
  SessionStatus& get_session_status(BangkeSessionId ipc_id) {
    return m_session_status_map[ipc_id];
  }
  SessionStatus& new_session_status(BangkeSessionId ipc_id) {
    return m_session_status_map[ipc_id] = SessionStatus();
  }

  AppOptionsByAppName m_app_options;
  bangke::UI* m_ui;  // reference
  DWORD m_active_session;
  HWND m_event_wnd = nullptr;  // OnNotify 投递 WM_BK_RIME_EVENT 用
  bool m_disabled;
  std::string m_last_schema_id;
  std::string m_last_app_name;
  bangke::UIStyle m_base_style;
  std::map<std::string, bool> m_show_notifications;
  std::map<std::string, bool> m_show_notifications_base;
  std::function<void()> _UpdateUICallback;

  static void OnNotify(void* context_object,
                       uintptr_t session_id,
                       const char* message_type,
                       const char* message_value);
  void _PushAiSnapshot(uintptr_t rime_sid);

  // AI 泵：插件把推理放在 worker 线程上跑完只填缓存，把候选顶进菜单必须由前端在自己
  // 的线程上重排——worker 直接 Compose 会和按键线程在同一个 Context 上撞车（librime 的
  // Context/Engine 不是线程安全的）。
  //
  // 泵只在服务端消息循环线程上起停与触发：SetTimer/KillTimer 和泵状态因此不需要跨线程
  // 同步。单次推理实测 p50 约 90ms，但长尾到 5s+，超时给够，否则长尾预测永远等不到。
  static constexpr UINT_PTR kAiPumpTimerId = 0x424B4149;  // 'BKAI'
  static constexpr UINT kAiPumpIntervalMs = 150;
  static constexpr DWORD kAiPumpTimeoutMs = 10000;
  BangkeSessionId m_ai_pump_ipc_id = 0;
  DWORD m_ai_pump_deadline = 0;
  static std::string m_message_type;
  static std::string m_message_value;
  static std::string m_message_label;
  static std::string m_option_name;
  static std::mutex m_notifier_mutex;
  SessionStatusMap m_session_status_map;
  // 部署窗口期的 ipc↔client_app 快照,EndMaintenance 据此重建会话
  std::map<DWORD, std::string> m_pending_restore;
  bool m_current_dark_mode;
  bool m_global_ascii_mode;
  int m_show_notifications_time;
  DWORD m_pid;
};
