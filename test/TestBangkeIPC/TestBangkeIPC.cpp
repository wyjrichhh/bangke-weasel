// TestBangkeIPC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <BangkeIPC.h>
#include <BangkeProtocol.h>
#include <BangkeRime.h>
#include <sstream>


#include <iostream>
#include <memory>

CAppModule _Module;

int console_main();
int client_main();
int server_main();
int smoke_ai();

// usage: TestBangkeIPC.exe [/start | /stop | /console | /smoke-ai]

int _tmain(int argc, _TCHAR* argv[]) {
  if (argc > 1 && !wcscmp(L"/smoke-ai", argv[1])) return smoke_ai();
  if (argc == 1)  // no args
  {
    return client_main();
  } else if (argc > 1 && !wcscmp(L"/start", argv[1])) {
    return server_main();
  } else if (argc > 1 && !wcscmp(L"/stop", argv[1])) {
    bangke::Client client;
    if (!client.Connect()) {
      std::cerr << "server not running." << std::endl;
      return 0;
    }
    client.ShutdownServer();
    return 0;
  } else if (argc > 1 && !wcscmp(L"/console", argv[1])) {
    return console_main();
  }

  return -1;
}

bool launch_server() {
  INT_PTR ret = reinterpret_cast<INT_PTR>(ShellExecute(NULL, L"open", L"TestBangkeIPC.exe", L"/start",
                              NULL, SW_NORMAL));
  if (ret <= 32) {
    std::cerr << "failed to launch server." << std::endl;
    return false;
  }
  return true;
}

bool read_buffer(LPWSTR buffer, UINT length, LPWSTR dest) {
  bangke::FrameHeader header{};
  std::wstring commit;
  bangke::Context context;
  bangke::Status status;
  bangke::Config config;
  bangke::UIStyle style;
  if (!bangke::ParseFramePrefix(reinterpret_cast<const uint8_t*>(buffer), length * sizeof(wchar_t),
      &header, &commit, &context, &status, &config, &style)) return false;
  std::wostringstream text;
  text << L"schema=" << status.schema_id << L" serial=" << header.key_serial
       << L" preedit=" << context.preedit.str << L" candidates=" << context.cinfo.candies.size();
  wcsncpy_s(dest, BANGKE_IPC_BUFFER_LENGTH, text.str().c_str(), _TRUNCATE);
  return true;
}

const char* wcstomb(const wchar_t* wcs) {
  const int buffer_len = 8192;
  static char buffer[buffer_len];
  WideCharToMultiByte(CP_OEMCP, NULL, wcs, -1, buffer, buffer_len, NULL, FALSE);
  return buffer;
}

int console_main() {
  bangke::Client client;
  if (!client.Connect()) {
    std::cerr << "failed to connect to server." << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "failed to start session." << std::endl;
    return -3;
  }

  while (std::cin.good()) {
    int ch = std::cin.get();
    if (!std::cin.good())
      break;
    bool eaten = client.ProcessKeyEvent(bangke::KeyEvent(ch, 0));
    std::cout << "server replies: " << eaten << std::endl;
    if (eaten) {
      WCHAR response[BANGKE_IPC_BUFFER_LENGTH] = {};
      bool ret = client.GetResponseData(
          std::bind<bool>(read_buffer, std::placeholders::_1,
                          std::placeholders::_2, std::ref(response)));
      std::cout << "get response data: " << ret << std::endl;
      std::cout << "buffer reads: " << std::endl
                << wcstomb(response) << std::endl;
    }
  }

  client.EndSession();

  return 0;
}

int client_main() {
  // launch_server();
  Sleep(1000);
  bangke::Client client;
  if (!client.Connect()) {
    std::cerr << "failed to connect to server." << std::endl;
    return -2;
  }
  client.StartSession();
  if (!client.Echo()) {
    std::cerr << "failed to login." << std::endl;
    return -3;
  }
  bool eaten = client.ProcessKeyEvent(bangke::KeyEvent(L'a', 0));
  std::cout << "server replies: " << eaten << std::endl;
  if (eaten) {
    WCHAR response[BANGKE_IPC_BUFFER_LENGTH] = {};
    bool ret = client.GetResponseData(
        std::bind<bool>(read_buffer, std::placeholders::_1,
                        std::placeholders::_2, std::ref(response)));
    std::cout << "get response data: " << ret << std::endl;
    std::cout << "buffer reads: " << std::endl
              << wcstomb(response) << std::endl;
  }
  client.EndSession();

  system("pause");
  return 0;
}

class TestRequestHandler : public bangke::RequestHandler {
 public:
  TestRequestHandler() : m_counter(0) {
    std::cerr << "handler ctor." << std::endl;
  }
  virtual ~TestRequestHandler() {
    std::cerr << "handler dtor: " << m_counter << std::endl;
  }
  virtual UINT FindSession(UINT session_id) {
    std::cerr << "FindSession: " << session_id << std::endl;
    return (session_id <= m_counter ? session_id : 0);
  }
  DWORD AddSession(LPWSTR buffer, EatLine eat = 0) override {
    std::cerr << "AddSession: " << m_counter + 1 << std::endl;
    return ++m_counter;
  }
  virtual UINT RemoveSession(UINT session_id) {
    std::cerr << "RemoveClient: " << session_id << std::endl;
    return 0;
  }
  virtual BOOL ProcessKeyEvent(bangke::KeyEvent keyEvent,
                               UINT session_id,
                               EatLine eat) {
    std::cerr << "ProcessKeyEvent: " << session_id
              << " keycode: " << keyEvent.keycode << " mask: " << keyEvent.mask
              << std::endl;
    bangke::Context context;
    context.preedit.str.assign(1, wchar_t(keyEvent.keycode));
    bangke::Status status;
    status.composing = true;
    const auto frame = bangke::BuildFrame(session_id, 1, nullptr, &context, &status, nullptr, nullptr);
    std::wstring wire(reinterpret_cast<const wchar_t*>(frame.data()), frame.size() / sizeof(wchar_t));
    eat(wire);
    return TRUE;
  }

 private:
  unsigned int m_counter;
};

int server_main() {
  HRESULT hRes = _Module.Init(NULL, GetModuleHandle(NULL));
  ATLASSERT(SUCCEEDED(hRes));

  bangke::Server server;
  // bangke::UI ui;
  // const std::unique_ptr<bangke::RequestHandler> handler(new
  // BangkeRimeHandler(&ui));
  const std::unique_ptr<bangke::RequestHandler> handler(new TestRequestHandler);

  server.SetRequestHandler(handler.get());
  if (!server.Start())
    return -4;
  std::cerr << "server running." << std::endl;
  int ret = server.Run();
  std::cerr << "server quitting." << std::endl;
  return ret;
}

// Runs against the installed server in the interactive session. It owns its
// IPC composition and never types into another application's document.
int smoke_ai() {
  bangke::Client client;
  if (!client.Connect()) { std::cerr << "FAIL server connection\n"; return 1; }
  client.StartSession();
  if (!client.SessionId()) { std::cerr << "FAIL session creation\n"; return 2; }
  client.SetAsciiMode(false);
  client.FocusIn();
  for (const char ch : std::string("jintiantianqizhenhaowomenyiqichuquwan"))
    client.ProcessKeyEvent(bangke::KeyEvent(ch, 0));
  wchar_t map_name[64], event_name[64];
  swprintf_s(map_name, L"Local\\BangkeSnap_%u", client.SessionId());
  swprintf_s(event_name, L"Local\\BangkeSnapEvt_%u", client.SessionId());
  HANDLE map = OpenFileMappingW(FILE_MAP_READ, FALSE, map_name);
  HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, event_name);
  bool found = false;
  const ULONGLONG deadline = GetTickCount64() + 15000;
  while (map && event && GetTickCount64() < deadline && !found) {
    if (WaitForSingleObject(event, 500) != WAIT_OBJECT_0) continue;
    auto* bytes = static_cast<const uint8_t*>(MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0));
    if (!bytes) break;
    bangke::FrameHeader header{};
    std::wstring commit;
    bangke::Context context;
    bangke::Status status;
    bangke::Config config;
    bangke::UIStyle style;
    const bool valid = bangke::ParseFramePrefix(bytes, 128 * 1024, &header,
        &commit, &context, &status, &config, &style);
    UnmapViewOfFile(bytes);
    if (!valid || header.ipc_sid != client.SessionId() || (header.flags & bangke::SNAP_HAS_COMMIT)) continue;
    for (const auto& comment : context.cinfo.comments)
      if (comment.str == L"AI") found = true;
    if (found) std::cout << "PASS AI candidate arrived via async snapshot without another key; serial="
                         << header.key_serial << "\n";
  }
  if (event) CloseHandle(event);
  if (map) CloseHandle(map);
  client.ClearComposition(); client.FocusOut(); client.EndSession();
  if (!found) std::cerr << "FAIL AI snapshot missing (check active schema/model/enabled settings)\n";
  return found ? 0 : 3;
}
