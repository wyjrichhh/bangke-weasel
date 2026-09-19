#pragma once
#include <BangkeIPC.h>
#include <PipeChannel.h>

namespace bangke {

class ClientImpl {
 public:
  ClientImpl();
  ~ClientImpl();

  bool Connect(ServerLauncher const& launcher);
  void Disconnect();
  void ShutdownServer();
  void StartSession();
  void EndSession();
  void StartMaintenance();
  void EndMaintenance();
  bool Echo();
  bool ProcessKeyEvent(KeyEvent const& keyEvent);
  // context 是按键前光标处的文本,随请求捎给服务端(见 Client::ProcessKeyEvent)
  bool ProcessKeyEvent(KeyEvent const& keyEvent, std::wstring const& context);
  bool CommitComposition();
  bool ClearComposition();
  bool SelectCandidateOnCurrentPage(size_t index);
  bool HighlightCandidateOnCurrentPage(size_t index);
  bool ChangePage(bool backward);
  void UpdateInputPosition(RECT const& rc);
  void FocusIn();
  void FocusOut();
  void SetAsciiMode(bool ascii);
  bool GetResponseData(ResponseHandler const& handler);
  // 本连接的会话号(推送槽命名用)
  DWORD SessionId() const { return session_id; }

 protected:
  void _InitializeClientInfo();
  bool _WriteClientInfo();

  LRESULT _SendMessage(BANGKE_IPC_COMMAND Msg, DWORD wParam, DWORD lParam);

  bool _Connected() const { return channel.Connected(); }
  bool _Active() const { return channel.Connected() && session_id != 0; }

 private:
  UINT session_id;
  std::wstring app_name;
  bool is_ime;

  PipeChannel<PipeMessage> channel;
};

}  // namespace bangke