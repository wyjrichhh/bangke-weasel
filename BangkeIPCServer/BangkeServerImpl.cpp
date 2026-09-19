#include "stdafx.h"
#include "BangkeServerImpl.h"
#include <mutex>
#include <Windows.h>
#include <resource.h>
#include <BangkeUtility.h>
#include <BangkeRime.h>

namespace bangke {
class PipeServer : public PipeChannel<DWORD, PipeMessage> {
 public:
  using ServerRunner = std::function<void()>;
  using Respond = std::function<void(Msg)>;
  using ServerHandler = std::function<void(PipeMessage, Respond)>;

  PipeServer(std::wstring&& pn_cmd, SECURITY_ATTRIBUTES* s);

 public:
  void Listen(ServerHandler const& handler);
  /* Get a server runner */
  ServerRunner GetServerRunner(ServerHandler const& handler);

 private:
  void _ProcessPipeThread(HANDLE pipe, ServerHandler const& handler);
};
}  // namespace bangke

using namespace bangke;

extern CAppModule _Module;

ServerImpl::ServerImpl()
    : m_pRequestHandler(NULL),
      m_darkMode(IsUserDarkMode()),
      channel(std::make_unique<PipeServer>(GetPipeName(), sa.get_attr())) {
  m_hUser32Module = GetModuleHandle(_T("user32.dll"));
}

ServerImpl::~ServerImpl() {
  _Finailize();
}

void ServerImpl::_Finailize() {
  if (pipeThread != nullptr) {
    pipeThread->interrupt();
    pipeThread = nullptr;
  } else {
    // avoid finalize again
    return;
  }

  if (IsWindow()) {
    DestroyWindow();
  }
}

LRESULT ServerImpl::OnColorChange(UINT uMsg,
                                  WPARAM wParam,
                                  LPARAM lParam,
                                  BOOL& bHandled) {
  if (IsUserDarkMode() != m_darkMode) {
    m_darkMode = IsUserDarkMode();
    m_pRequestHandler->UpdateColorTheme(m_darkMode);
  }
  return 0;
}

LRESULT ServerImpl::OnCreate(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL& bHandled) {
  // not neccessary...
  ::SetWindowText(m_hWnd, BANGKE_IPC_WINDOW);
  // OnNotify 可能跑在 rime 内部线程，需此窗口把事件转投回消息循环（串行线程）
  if (m_pRequestHandler)
    m_pRequestHandler->SetEventWindow(m_hWnd);
  return 0;
}

LRESULT ServerImpl::OnRimeEvent(UINT uMsg,
                                WPARAM wParam,
                                LPARAM lParam,
                                BOOL& bHandled) {
  if (m_pRequestHandler)
    m_pRequestHandler->OnDeferredEvent((int)wParam, (uintptr_t)lParam);
  return 0;
}

LRESULT ServerImpl::OnTimer(UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL& bHandled) {
  // 本窗口只有 AI 泵一个定时器（候选窗自己的定时器挂在 panel 的窗口上）
  if (m_pRequestHandler)
    m_pRequestHandler->AiPumpTick();
  return 0;
}

LRESULT ServerImpl::OnClose(UINT uMsg,
                            WPARAM wParam,
                            LPARAM lParam,
                            BOOL& bHandled) {
  Stop();
  return 0;
}

LRESULT ServerImpl::OnDestroy(UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL& bHandled) {
  bHandled = FALSE;
  return 1;
}

LRESULT ServerImpl::OnQueryEndSystemSession(UINT uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam,
                                            BOOL& bHandled) {
  return TRUE;
}

LRESULT ServerImpl::OnEndSystemSession(UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam,
                                       BOOL& bHandled) {
  if (m_pRequestHandler) {
    m_pRequestHandler->Finalize();
    m_pRequestHandler = nullptr;
  }
  return 0;
}

LRESULT ServerImpl::OnCommand(UINT uMsg,
                              WPARAM wParam,
                              LPARAM lParam,
                              BOOL& bHandled) {
  bHandled = FALSE;
  return 0;
}

DWORD ServerImpl::OnSetAscii(BANGKE_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->SetOption(lParam, "ascii_mode", wParam != 0);
  return 0;
}

HWND ServerImpl::Start() {
  std::wstring instanceName = L"(BANGKE)BangkePinyin-";
  instanceName += getUsername();
  HANDLE hMutexOneInstance = ::CreateMutex(NULL, FALSE, instanceName.c_str());
  bool areYouOK = (::GetLastError() == ERROR_ALREADY_EXISTS ||
                   ::GetLastError() == ERROR_ACCESS_DENIED);

  if (areYouOK) {
    return 0;  // assure single instance
  }

  HWND hwnd = Create(NULL);

  return hwnd;
}

int ServerImpl::Stop() {
  // DO NOT exit process or finalize here
  // Let BangkeServer handle this
  PostMessage(WM_QUIT);
  return 0;
}

int ServerImpl::Run() {
  // This workaround causes a VC internal error:
  // void PipeServer::Listen(ServerHandler handler);
  //
  // auto handler = boost::bind(&ServerImpl::HandlePipeMessage, this);
  // auto listener = boost::bind(&PipeServer::Listen, channel.get(), handler);
  //
  auto listener = [this](PipeMessage msg, PipeServer::Respond resp) -> void {
    std::lock_guard guard(BangkeRimeHandler::ApiMutex());
    HandlePipeMessage(msg, resp);
  };
  pipeThread = std::make_unique<boost::thread>(
      [this, &listener]() { channel->Listen(listener); });

  CMessageLoop theLoop;
  _Module.AddMessageLoop(&theLoop);
  int nRet = theLoop.Run();
  _Module.RemoveMessageLoop();
  return nRet;
}

DWORD ServerImpl::OnEcho(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->FindSession(lParam);
}

DWORD ServerImpl::OnStartSession(BANGKE_IPC_COMMAND uMsg,
                                 DWORD wParam,
                                 DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->AddSession(
      reinterpret_cast<LPWSTR>(channel->ReceiveBuffer()),
      [this](std::wstring& msg) -> bool {
        *channel << msg;
        return true;
      });
}

DWORD ServerImpl::OnEndSession(BANGKE_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  return m_pRequestHandler->RemoveSession(lParam);
}

DWORD ServerImpl::OnKeyEvent(BANGKE_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler /* || !m_pSharedMemory*/)
    return 0;

  auto eat = [this](std::wstring& msg) -> bool {
    *channel << msg;
    return true;
  };
  return m_pRequestHandler->ProcessKeyEvent(KeyEvent(wParam), lParam, eat);
}

DWORD ServerImpl::OnShutdownServer(BANGKE_IPC_COMMAND uMsg,
                                   DWORD wParam,
                                   DWORD lParam) {
  Stop();
  return 0;
}

DWORD ServerImpl::OnFocusIn(BANGKE_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->FocusIn(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnFocusOut(BANGKE_IPC_COMMAND uMsg,
                             DWORD wParam,
                             DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  m_pRequestHandler->FocusOut(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnUpdateInputPosition(BANGKE_IPC_COMMAND uMsg,
                                        DWORD wParam,
                                        DWORD lParam) {
  if (!m_pRequestHandler)
    return 0;
  /*
   * 移位标志 = 1bit == 0
   * height: 0~127 = 7bit
   * top:-2048~2047 = 12bit（有符号）
   * left:-2048~2047 = 12bit（有符号）
   *
   * 高解析度下：
   * 移位标志 = 1bit == 1
   * height: 0~254 = 7bit（舍弃低1位）
   * top: -4096~4094 = 12bit（有符号，舍弃低1位）
   * left: -4096~4094 = 12bit（有符号，舍弃低1位）
   */
  RECT rc;
  int hi_res = (wParam >> 31) & 0x01;
  rc.left = ((wParam & 0x7ff) - (wParam & 0x800)) << hi_res;
  rc.top = (((wParam >> 12) & 0x7ff) - ((wParam >> 12) & 0x800)) << hi_res;
  const int width = 6;
  int height = ((wParam >> 24) & 0x7f) << hi_res;
  rc.right = rc.left + width;
  rc.bottom = rc.top + height;

  {
    using PPTLPFPMDPI = BOOL(WINAPI*)(HWND, LPPOINT);
    PPTLPFPMDPI PhysicalToLogicalPointForPerMonitorDPI =
        (PPTLPFPMDPI)::GetProcAddress(m_hUser32Module,
                                      "PhysicalToLogicalPointForPerMonitorDPI");
    POINT lt = {rc.left, rc.top};
    POINT rb = {rc.right, rc.bottom};
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &lt);
    PhysicalToLogicalPointForPerMonitorDPI(NULL, &rb);
    rc = {lt.x, lt.y, rb.x, rb.y};
  }

  m_pRequestHandler->UpdateInputPosition(rc, lParam);
  return 0;
}

DWORD ServerImpl::OnStartMaintenance(BANGKE_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->StartMaintenance();
  return 0;
}

DWORD ServerImpl::OnEndMaintenance(BANGKE_IPC_COMMAND uMsg,
                                   DWORD wParam,
                                   DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->EndMaintenance();
  return 0;
}

DWORD ServerImpl::OnCommitComposition(BANGKE_IPC_COMMAND uMsg,
                                      DWORD wParam,
                                      DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->CommitComposition(lParam);
  return 0;
}

DWORD ServerImpl::OnClearComposition(BANGKE_IPC_COMMAND uMsg,
                                     DWORD wParam,
                                     DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->ClearComposition(lParam);
  return 0;
}

DWORD ServerImpl::OnSelectCandidateOnCurrentPage(BANGKE_IPC_COMMAND uMsg,
                                                 DWORD wParam,
                                                 DWORD lParam) {
  if (m_pRequestHandler)
    m_pRequestHandler->SelectCandidateOnCurrentPage(wParam, lParam);
  return 0;
}

DWORD ServerImpl::OnHighlightCandidateOnCurrentPage(BANGKE_IPC_COMMAND uMsg,
                                                    DWORD wParam,
                                                    DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->HighlightCandidateOnCurrentPage(wParam, lParam, eat);
  }
  return 0;
}

DWORD ServerImpl::OnChangePage(BANGKE_IPC_COMMAND uMsg,
                               DWORD wParam,
                               DWORD lParam) {
  if (m_pRequestHandler) {
    auto eat = [this](std::wstring& msg) -> bool {
      *channel << msg;
      return true;
    };
    m_pRequestHandler->ChangePage(wParam, lParam, eat);
  }
  return 0;
}

#define MAP_PIPE_MSG_HANDLE(__msg, __wParam, __lParam) \
  {                                                    \
    auto lParam = __lParam;                            \
    auto wParam = __wParam;                            \
    LRESULT _result = 0;                               \
    switch (__msg) {
#define PIPE_MSG_HANDLE(__msg, __func)       \
  case __msg:                                \
    _result = __func(__msg, wParam, lParam); \
    break;

#define END_MAP_PIPE_MSG_HANDLE(__result) \
  }                                       \
  __result = _result;                     \
  }

template <typename _Resp>
void ServerImpl::HandlePipeMessage(PipeMessage pipe_msg, _Resp resp) {
  DWORD result;

  MAP_PIPE_MSG_HANDLE(pipe_msg.Msg, pipe_msg.wParam, pipe_msg.lParam)
  PIPE_MSG_HANDLE(BANGKE_IPC_ECHO, OnEcho)
  PIPE_MSG_HANDLE(BANGKE_IPC_START_SESSION, OnStartSession)
  PIPE_MSG_HANDLE(BANGKE_IPC_END_SESSION, OnEndSession)
  PIPE_MSG_HANDLE(BANGKE_IPC_PROCESS_KEY_EVENT, OnKeyEvent)
  PIPE_MSG_HANDLE(BANGKE_IPC_SHUTDOWN_SERVER, OnShutdownServer)
  PIPE_MSG_HANDLE(BANGKE_IPC_FOCUS_IN, OnFocusIn)
  PIPE_MSG_HANDLE(BANGKE_IPC_FOCUS_OUT, OnFocusOut)
  PIPE_MSG_HANDLE(BANGKE_IPC_UPDATE_INPUT_POS, OnUpdateInputPosition)
  PIPE_MSG_HANDLE(BANGKE_IPC_START_MAINTENANCE, OnStartMaintenance)
  PIPE_MSG_HANDLE(BANGKE_IPC_END_MAINTENANCE, OnEndMaintenance)
  PIPE_MSG_HANDLE(BANGKE_IPC_COMMIT_COMPOSITION, OnCommitComposition)
  PIPE_MSG_HANDLE(BANGKE_IPC_CLEAR_COMPOSITION, OnClearComposition);
  PIPE_MSG_HANDLE(BANGKE_IPC_SELECT_CANDIDATE_ON_CURRENT_PAGE,
                  OnSelectCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(BANGKE_IPC_HIGHLIGHT_CANDIDATE_ON_CURRENT_PAGE,
                  OnHighlightCandidateOnCurrentPage);
  PIPE_MSG_HANDLE(BANGKE_IPC_CHANGE_PAGE, OnChangePage);
  PIPE_MSG_HANDLE(BANGKE_IPC_SET_ASCII, OnSetAscii);
  END_MAP_PIPE_MSG_HANDLE(result);

  resp(result);
}

PipeServer::PipeServer(std::wstring&& pn_cmd, SECURITY_ATTRIBUTES* s)
    : PipeChannel(std::move(pn_cmd), s) {}

void PipeServer::Listen(ServerHandler const& handler) {
  for (;;) {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    try {
      boost::this_thread::interruption_point();
      pipe = _ConnectServerPipe(pname);
      boost::thread th(
          [&handler, pipe, this] { _ProcessPipeThread(pipe, handler); });
    } catch (DWORD ex) {
      _FinalizePipe(pipe);
    }
    boost::this_thread::interruption_point();
  }
}

PipeServer::ServerRunner PipeServer::GetServerRunner(
    ServerHandler const& handler) {
  return [&handler, this]() { Listen(handler); };
}

void PipeServer::_ProcessPipeThread(HANDLE pipe, ServerHandler const& handler) {
  try {
    for (;;) {
      Res msg;
      _Receive(pipe, &msg, sizeof(msg));
      handler(msg, [this, pipe](Msg resp) { _Send(pipe, resp); });
    }
  } catch (...) {
    _FinalizePipe(pipe);
  }
}

// bangke::Server

Server::Server() : m_pImpl(new ServerImpl) {}

Server::~Server() {
  if (m_pImpl)
    delete m_pImpl;
}

HWND Server::Start() {
  return m_pImpl->Start();
}

int Server::Stop() {
  return m_pImpl->Stop();
}

int Server::Run() {
  return m_pImpl->Run();
}

void Server::SetRequestHandler(RequestHandler* pHandler) {
  m_pImpl->SetRequestHandler(pHandler);
}

HWND Server::GetHWnd() {
  return m_pImpl->m_hWnd;
}
