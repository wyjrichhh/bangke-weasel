#pragma once
#include <BangkeIPC.h>
#include <map>
#include <Winnt.h>   // for security attributes constants
#include <aclapi.h>  // for ACL
#include <boost/thread.hpp>
#include <PipeChannel.h>

#include "SecurityAttribute.h"

namespace bangke {
class PipeServer;

typedef CWinTraits<WS_DISABLED, WS_EX_TRANSPARENT> ServerWinTraits;

class ServerImpl : public CWindowImpl<ServerImpl, CWindow, ServerWinTraits>
// class ServerImpl
{
 public:
  DECLARE_WND_CLASS(BANGKE_IPC_WINDOW)

  BEGIN_MSG_MAP(BANGKE_IPC_WINDOW)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  MESSAGE_HANDLER(WM_QUERYENDSESSION, OnQueryEndSystemSession)
  MESSAGE_HANDLER(WM_ENDSESSION, OnEndSystemSession)
  MESSAGE_HANDLER(WM_DWMCOLORIZATIONCOLORCHANGED, OnColorChange)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnColorChange)
  MESSAGE_HANDLER(WM_COMMAND, OnCommand)
  MESSAGE_HANDLER(WM_BK_RIME_EVENT, OnRimeEvent)
  MESSAGE_HANDLER(WM_TIMER, OnTimer)
  END_MSG_MAP()

  LRESULT OnColorChange(UINT uMsg,
                        WPARAM wParam,
                        LPARAM lParam,
                        BOOL& bHandled);
  LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnQueryEndSystemSession(UINT uMsg,
                                  WPARAM wParam,
                                  LPARAM lParam,
                                  BOOL& bHandled);
  LRESULT OnEndSystemSession(UINT uMsg,
                             WPARAM wParam,
                             LPARAM lParam,
                             BOOL& bHandled);
  LRESULT OnCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnRimeEvent(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  DWORD OnEcho(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnSetAscii(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnStartSession(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnEndSession(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnKeyEvent(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnShutdownServer(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnFocusIn(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnFocusOut(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnUpdateInputPosition(BANGKE_IPC_COMMAND uMsg,
                              DWORD wParam,
                              DWORD lParam);
  DWORD OnStartMaintenance(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnReloadStyle(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnEndMaintenance(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnCommitComposition(BANGKE_IPC_COMMAND uMsg,
                            DWORD wParam,
                            DWORD lParam);
  DWORD OnClearComposition(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);
  DWORD OnSelectCandidateOnCurrentPage(BANGKE_IPC_COMMAND uMsg,
                                       DWORD wParam,
                                       DWORD lParam);
  DWORD OnHighlightCandidateOnCurrentPage(BANGKE_IPC_COMMAND uMsg,
                                          DWORD wParam,
                                          DWORD lParam);
  DWORD OnChangePage(BANGKE_IPC_COMMAND uMsg, DWORD wParam, DWORD lParam);

 public:
  ServerImpl();
  ~ServerImpl();

  HWND Start();
  int Stop();
  int Run();

  void SetRequestHandler(RequestHandler* pHandler) {
    m_pRequestHandler = pHandler;
  }

 private:
  void _Finailize();
  template <typename _Resp>
  void HandlePipeMessage(PipeMessage pipe_msg, _Resp resp);

  std::unique_ptr<PipeServer> channel;
  std::unique_ptr<boost::thread> pipeThread;
  RequestHandler* m_pRequestHandler;  // reference
  HMODULE m_hUser32Module;
  SecurityAttribute sa;
  BOOL m_darkMode;
};

}  // namespace bangke
