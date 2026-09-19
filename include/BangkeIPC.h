#pragma once
#include <BangkeIPCData.h>
#include <BangkeUtility.h>
#include <windows.h>
#include <functional>
#include <memory>
#include <KeyEvent.h>

#define BANGKE_IPC_WINDOW L"BangkeIPCWindow_1.0"
#define BANGKE_IPC_PIPE_NAME L"BangkeNamedPipe"

#define BANGKE_IPC_METADATA_SIZE 1024
#define BANGKE_IPC_BUFFER_SIZE (4 * 1024)
#define BANGKE_IPC_BUFFER_LENGTH (BANGKE_IPC_BUFFER_SIZE / sizeof(WCHAR))
#define BANGKE_IPC_SHARED_MEMORY_SIZE \
  (sizeof(PipeMessage) + BANGKE_IPC_BUFFER_SIZE)

enum BANGKE_IPC_COMMAND {
  BANGKE_IPC_ECHO = (WM_APP + 1),
  BANGKE_IPC_START_SESSION,
  BANGKE_IPC_END_SESSION,
  BANGKE_IPC_PROCESS_KEY_EVENT,
  BANGKE_IPC_SHUTDOWN_SERVER,
  BANGKE_IPC_FOCUS_IN,
  BANGKE_IPC_FOCUS_OUT,
  BANGKE_IPC_UPDATE_INPUT_POS,
  BANGKE_IPC_START_MAINTENANCE,
  BANGKE_IPC_END_MAINTENANCE,
  BANGKE_IPC_COMMIT_COMPOSITION,
  BANGKE_IPC_CLEAR_COMPOSITION,
  BANGKE_IPC_SET_ASCII,
  BANGKE_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
  BANGKE_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
  BANGKE_IPC_CHANGE_PAGE,
  BANGKE_IPC_LAST_COMMAND
};

// rime 通知的"入队投递"通道：OnNotify 可能跑在 rime 内部线程（含插件 worker），
// 只允许 PostMessage 本消息后立即返回，实际工作由服务端消息循环在串行线程做
#define WM_BK_RIME_EVENT (WM_APP + 0x42)
enum BkRimeEvent {
  BK_EVENT_AI_PUMP = 1,  // 有推理在途（属性 ai_predict/pending），起泵把候选顶出来
};

namespace bangke {
struct PipeMessage {
  BANGKE_IPC_COMMAND Msg;
  DWORD wParam;
  DWORD lParam;
};

struct IPCMetadata {
  enum { WINDOW_CLASS_LENGTH = 64 };
  UINT32 server_hwnd;
  WCHAR server_window_class[WINDOW_CLASS_LENGTH];
};

// 處理請求之物件
struct RequestHandler {
  using EatLine = std::function<bool(std::wstring&)>;
  RequestHandler() {}
  virtual ~RequestHandler() {}
  virtual void Initialize() {}
  virtual void Finalize() {}
  virtual DWORD FindSession(DWORD session_id) { return 0; }
  virtual DWORD AddSession(LPWSTR buffer, EatLine eat = 0) { return 0; }
  virtual DWORD RemoveSession(DWORD session_id) { return 0; }
  virtual BOOL ProcessKeyEvent(KeyEvent keyEvent,
                               DWORD session_id,
                               EatLine eat) {
    return FALSE;
  }
  // 按键前光标处的文本,客户端随按键请求捎来。空串表示"这次读不到",消费方必须回退
  // 到别的上下文来源,而不是当成"文档到此为止"
  virtual void SetClientContext(DWORD session_id, std::wstring const& text) {}
  virtual void CommitComposition(DWORD session_id) {}
  virtual void ClearComposition(DWORD session_id) {}
  virtual void SelectCandidateOnCurrentPage(size_t index, DWORD session_id) {}
  virtual bool HighlightCandidateOnCurrentPage(size_t index,
                                               DWORD session_id,
                                               EatLine eat) {
    return false;
  }
  virtual bool ChangePage(bool backward, DWORD session_id, EatLine eat) {
    return false;
  }
  virtual void FocusIn(DWORD param, DWORD session_id) {}
  virtual void FocusOut(DWORD param, DWORD session_id) {}
  virtual void UpdateInputPosition(RECT const& rc, DWORD session_id) {}
  virtual void StartMaintenance() {}
  virtual void EndMaintenance() {}
  virtual void SetOption(DWORD session_id, const std::string& opt, bool val) {}
  virtual void UpdateColorTheme(BOOL darkMode) {}
  // 服务端窗口就绪后注入 HWND，供 OnNotify 投递 WM_BK_RIME_EVENT
  virtual void SetEventWindow(HWND wnd) {}
  // 在服务端消息循环（串行线程）上消费投递来的 rime 事件
  virtual void OnDeferredEvent(int event, uintptr_t rime_session_id) {}
  // AI 泵的两半，都只在服务端消息循环线程上调用：Arm 在按键后看有没有推理在途并起
  // 定时器，Tick 在定时器到点时重排一次。放在一个线程上是为了让 SetTimer/KillTimer
  // 与泵的状态不被跨线程穿插
  virtual void AiPumpArm(DWORD session_id) {}
  virtual void AiPumpTick() {}
};

// 處理server端回應之物件
typedef std::function<bool(LPWSTR buffer, DWORD length)> ResponseHandler;

// 啟動服務進程之物件
typedef std::function<bool()> ServerLauncher;

// IPC實現類聲明

class ClientImpl;
class ServerImpl;

// IPC接口類

class Client {
 public:
  Client();
  virtual ~Client();

  // 连接到服务，必要时启动服务进程
  bool Connect(ServerLauncher launcher = 0);
  // 断开连接
  void Disconnect();
  // 终止服务
  void ShutdownServer();
  // 發起會話
  void StartSession();
  // 結束會話
  void EndSession();
  // 進入維護模式
  void StartMaintenance();
  // 退出維護模式
  void EndMaintenance();
  // 测试连接
  bool Echo();
  // 请求服务处理按键消息
  bool ProcessKeyEvent(KeyEvent const& keyEvent);
  // 同上,并捎上按键前光标处的文本,供 AI 插件当上下文用
  bool ProcessKeyEvent(KeyEvent const& keyEvent, std::wstring const& context);
  // 上屏正在編輯的文字
  bool CommitComposition();
  // 清除正在編輯的文字
  bool ClearComposition();
  // 选择当前页面编号为index的候选
  bool SelectCandidateOnCurrentPage(size_t index);
  // 高亮当前页面编号为index的候选
  bool HighlightCandidateOnCurrentPage(size_t index);
  // 翻页，backward = true 向前翻，false向后翻
  bool ChangePage(bool backward);
  // 更新输入位置
  void UpdateInputPosition(RECT const& rc);
  // 输入窗口获得焦点
  void FocusIn();
  // 输入窗口失去焦点
  void FocusOut();
  // 切换中/英文模式（原托盘命令专道，现仅剩这一个用途，索性正名）
  void SetAsciiMode(bool ascii);
  // 本连接的会话号（推送槽命名用）
  DWORD SessionId() const;
  // 读取server返回的数据
  bool GetResponseData(ResponseHandler handler);

 private:
  ClientImpl* m_pImpl;
};

class Server {
 public:
  Server();
  virtual ~Server();

  // 初始化服务
  HWND Start();
  // 结束服务
  int Stop();
  // 消息循环
  int Run();

  void SetRequestHandler(RequestHandler* pHandler);
  HWND GetHWnd();

 private:
  ServerImpl* m_pImpl;
};

inline std::wstring GetPipeName() {
  std::wstring pipe_name;
  pipe_name += L"\\\\.\\pipe\\";
  pipe_name += getUsername();
  pipe_name += L"\\";
  pipe_name += BANGKE_IPC_PIPE_NAME;
  return pipe_name;
}
}  // namespace bangke
