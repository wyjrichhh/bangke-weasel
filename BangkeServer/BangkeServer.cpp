// BangkeServer.cpp : main source file for BangkeServer.exe
//
//	WTL MessageLoop 封装了消息循环. 实现了 getmessage/dispatchmessage....

#include "stdafx.h"
#include "resource.h"
#include "BangkeServerApp.h"
#include <BangkeIPC.h>
#include <BangkeUI.h>
#include <BangkeRime.h>
#include <BangkeUtility.h>
#include <functional>
#include <ShellScalingApi.h>
#include <WinUser.h>
#include <memory>
#include <string>
#include <atlstr.h>
#pragma comment(lib, "Shcore.lib")
CAppModule _Module;

int WINAPI _tWinMain(HINSTANCE hInstance,
                     HINSTANCE /*hPrevInstance*/,
                     LPTSTR lpstrCmdLine,
                     int nCmdShow) {
  LANGID langId = get_language_id();
  SetThreadUILanguage(langId);
  SetThreadLocale(langId);

  if (!IsWindowsBlueOrLaterEx()) {
    CString info, cap;
    info.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING);
    cap.LoadStringW(IDS_STR_SYSTEM_VERSION_WARNING_CAPTION);
    MessageBoxExW(NULL, info, cap, MB_ICONERROR, langId);
    return 0;
  }
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  // 防止服务进程开启输入法
  ImmDisableIME(-1);

  WCHAR user_name[20] = {0};
  DWORD size = _countof(user_name);
  GetUserName(user_name, &size);
  if (!_wcsicmp(user_name, L"SYSTEM")) {
    return 1;
  }

  HRESULT hRes = ::CoInitialize(NULL);
  // If you are running on NT 4.0 or higher you can use the following call
  // instead to make the EXE free threaded. This means that calls come in on a
  // random RPC thread.
  // HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
  ATLASSERT(SUCCEEDED(hRes));

  // this resolves ATL window thunking problem when Microsoft Layer for Unicode
  // (MSLU) is used
  ::DefWindowProc(NULL, 0, 0, 0L);

  AtlInitCommonControls(
      ICC_BAR_CLASSES);  // add flags to support other controls

  hRes = _Module.Init(NULL, hInstance);
  ATLASSERT(SUCCEEDED(hRes));

  if (!wcscmp(L"/userdir", lpstrCmdLine)) {
    CreateDirectory(BangkeUserDataPath().c_str(), NULL);
    BangkeServerApp::explore(BangkeUserDataPath());
    return 0;
  }
  if (!wcscmp(L"/bangkedir", lpstrCmdLine)) {
    BangkeServerApp::explore(BangkeServerApp::install_dir());
    return 0;
  }

  // command line option /q stops the running server
  bool quit = !wcscmp(L"/q", lpstrCmdLine) || !wcscmp(L"/quit", lpstrCmdLine);
  // restart if already running
  {
    bangke::Client client;
    if (client.Connect())  // try to connect to running server
    {
      client.ShutdownServer();
      if (quit)
        return 0;
      int retry = 0;
      while (client.Connect() && retry < 10) {
        client.ShutdownServer();
        retry++;
        Sleep(50);
      }
      if (retry >= 10)
        return 0;
    } else if (quit)
      return 0;
  }

  CreateDirectory(BangkeUserDataPath().c_str(), NULL);

  int nRet = 0;
  try {
    BangkeServerApp app;
    RegisterApplicationRestart(NULL, 0);
    // The settings binary owns networking and notifications, outside the input path.
    wchar_t exePath[MAX_PATH] = {};
    const DWORD exeLength = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exeLength > 0 && exeLength < MAX_PATH) {
      std::wstring helper(exePath);
      const auto slash = helper.find_last_of(L"\\/");
      if (slash != std::wstring::npos) {
        helper = helper.substr(0, slash + 1) + L"BangkeSettings.exe";
        std::wstring command = L"\"" + helper + L"\" /updatemonitor";
        STARTUPINFOW startup = {}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION process = {};
        if (CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
          CloseHandle(process.hThread); CloseHandle(process.hProcess);
        }
      }
    }
    nRet = app.Run();
  } catch (...) {
    // bad luck...
    nRet = -1;
  }

  _Module.Term();
  ::CoUninitialize();

  return nRet;
}
