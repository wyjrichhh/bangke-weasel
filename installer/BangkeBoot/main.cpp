// 单文件分发引导壳:把图形安装器(BangkeInstaller.exe)+ MSI + Qt 运行库
// 作为 RCDATA 资源嵌入本 exe。运行时解包到 %TEMP%\BangkeSetup\,
// 拉起安装界面,安装器退出后清理临时目录。
// 载荷布局由资源 IDR_MANIFEST(100) 描述:每行 "资源ID|相对路径"。
// 本体 requireAdministrator:双击即 UAC 一次(正规安装器惯例),子进程直启免代理。
#include <windows.h>
#include <shlwapi.h>

#include <string>
#include <vector>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static const wchar_t* kWndClass = L"BangkeBoot";
static wchar_t g_msg[128] = L"正在准备安装…";
static HWND g_wnd = NULL;

// 诊断日志:%TEMP%\bangkeboot.log(分发版出问题时一条双击即可定位)
static void Log(const wchar_t* msg) {
  wchar_t tmp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, tmp);
  const std::wstring path = std::wstring(tmp) + L"bangkeboot.log";
  HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return;
  SetFilePointer(f, 0, NULL, FILE_END);
  wchar_t line[256];
  swprintf_s(line, L"%lu %s\r\n", GetTickCount(), msg);
  DWORD w = 0;
  WriteFile(f, line, (DWORD)(wcslen(line) * sizeof(wchar_t)), &w, NULL);
  CloseHandle(f);
}

static std::wstring SlashPath(const std::wstring& p) {
  std::wstring r = p;
  for (auto& c : r)
    if (c == L'/')
      c = L'\\';
  return r;
}

static void EnsureDirFor(const std::wstring& file) {
  std::wstring dir = file.substr(0, file.find_last_of(L'\\'));
  for (size_t i = 3; i < dir.size(); ++i) {
    if (dir[i] == L'\\') {
      CreateDirectoryW(dir.substr(0, i).c_str(), NULL);
    }
  }
  CreateDirectoryW(dir.c_str(), NULL);
}

static void Pump() {
  MSG m;
  while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(h, &ps);
      SetBkColor(dc, RGB(13, 20, 29));
      SetTextColor(dc, RGB(210, 222, 235));
      RECT rc;
      GetClientRect(h, &rc);
      DrawTextW(dc, g_msg, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
      EndPaint(h, &ps);
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(h, msg, w, l);
}

static bool ExtractPayload(const std::wstring& dir,
                            std::wstring* msiNameOut = nullptr) {
  HRSRC mres = FindResourceW(NULL, MAKEINTRESOURCEW(100), RT_RCDATA);
  if (!mres)
    return false;
  HGLOBAL mg = LoadResource(NULL, mres);
  const char* manifest = (const char*)LockResource(mg);
  const size_t mlen = SizeofResource(NULL, mres);

  std::vector<std::pair<int, std::wstring>> items;
  std::string line;
  for (size_t i = 0; i <= mlen; ++i) {
    if (i == mlen || manifest[i] == '\n') {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();  // CRLF 清单,\r 混进路径即建文件失败
      const size_t bar = line.find('|');
      if (bar != std::string::npos) {
        items.push_back({atoi(line.substr(0, bar).c_str()),
                         SlashPath(std::wstring(line.begin() + bar + 1,
                                                line.begin() + line.size()))});
      }
      line.clear();
    } else {
      line.push_back(manifest[i]);
    }
  }

  int done = 0;
  for (const auto& [id, rel] : items) {
    if (msiNameOut && msiNameOut->empty() &&
        rel.rfind(L"BangkeSetup-") == 0 && rel.size() > 4 &&
        rel.compare(rel.size() - 4, 4, L".msi") == 0)
      *msiNameOut = rel;  // 清单里的 MSI 名:传给安装器,别让它按目录挑
    swprintf_s(g_msg, L"正在解包安装文件… %d/%d", ++done, (int)items.size());
    InvalidateRect(g_wnd, NULL, FALSE);  // 让小窗文案跟上
    Pump();

    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r)
      return false;
    HGLOBAL g = LoadResource(NULL, r);
    const void* data = LockResource(g);
    const DWORD size = SizeofResource(NULL, r);
    const std::wstring dst = dir + L"\\" + rel;
    EnsureDirFor(dst);
    HANDLE f = CreateFileW(dst.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
      return false;
    // 264MB 级载荷,分块落盘,进度条不卡
    const BYTE* p = (const BYTE*)data;
    for (DWORD off = 0; off < size;) {
      const DWORD chunk = min(1u << 20, size - off);
      DWORD wrote = 0;
      if (!WriteFile(f, p + off, chunk, &wrote, NULL) || wrote != chunk) {
        CloseHandle(f);
        return false;
      }
      off += chunk;
      Pump();
    }
    CloseHandle(f);
  }
  return true;
}

static void DeleteTree(const std::wstring& dir) {
  std::wstring dbl = dir;
  for (auto& c : dbl)
    if (c == L'\\')
      c = L'\0';
  dbl.push_back(L'\0');
  dbl.push_back(L'\0');
  SHFILEOPSTRUCTW op{};
  op.wFunc = FO_DELETE;
  op.pFrom = dbl.c_str();
  op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
  SHFileOperationW(&op);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
  // 极简进度小窗:安装界面解包就绪后由它接管
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(NULL, IDC_APPSTARTING);
  wc.hbrBackground = CreateSolidBrush(RGB(13, 20, 29));
  wc.lpszClassName = kWndClass;
  RegisterClassExW(&wc);
  const int w = 420, h = 120;
  const int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
  const int sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
  g_wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kWndClass,
                             L"蚌壳拼音", WS_POPUP | WS_VISIBLE, sx, sy, w, h,
                             NULL, NULL, inst, NULL);
  Pump();

  wchar_t tmp[MAX_PATH] = {0};
  GetTempPathW(MAX_PATH, tmp);
  const std::wstring dir = std::wstring(tmp) + L"BangkeSetup";
  DeleteTree(dir);  // 上次残留
  CreateDirectoryW(dir.c_str(), NULL);

  std::wstring msiName;
  bool ok = ExtractPayload(dir, &msiName);
  Log(ok ? L"extract ok" : L"extract FAILED");
  if (ok) {
    wcscpy_s(g_msg, L"");
    DestroyWindow(g_wnd);
    const std::wstring exe = dir + L"\\BangkeInstaller.exe";
    // 本壳已 requireAdministrator(双击时 UAC 提权一次,与正规安装器一致),
    // 提权父进程可直接 CreateProcessW 子安装器:真句柄,确定性等待。
    // 此前 ShellExecuteEx 交提权代理不回句柄,壳随即清理临时目录,
    // 把刚被代理拉起的安装器连文件带目录拆了。
    // MSI 文件名作为参数传下去:临时目录若有历史残留 MSI,安装器按
    // 文件名挑会挑错(字典序 0.4.9 > 0.4.13),参数是唯一真相
    std::wstring cmd = L"\"" + exe + L"\"" +
                       (msiName.empty() ? L"" : (L" \"" + msiName + L"\""));
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(exe.c_str(), cmdBuf.data(), NULL, NULL, FALSE, 0, NULL,
                       dir.c_str(), &si, &pi)) {
      AllowSetForegroundWindow(pi.dwProcessId);  // 前台权交接,否则孩子窗口沉底
      wchar_t buf[128];
      swprintf_s(buf, L"created pid=%lu", pi.dwProcessId);
      Log(buf);
      WaitForSingleObject(pi.hProcess, INFINITE);
      DWORD code = 0;
      GetExitCodeProcess(pi.hProcess, &code);
      swprintf_s(buf, L"exitcode=%lu", code);
      Log(buf);
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    } else {
      wchar_t buf[128];
      swprintf_s(buf, L"CreateProcess failed, err=%lu", GetLastError());
      Log(buf);
      ok = false;
    }
  }
  if (!ok) {
    wchar_t buf[256];
    swprintf_s(buf, L"蚌壳安装引导失败\n错误码 %lu\n详情见 %%TEMP%%\\bangkeboot.log",
               GetLastError());
    MessageBoxW(NULL, buf, L"蚌壳拼音", MB_ICONERROR | MB_OK);
  }
  DeleteTree(dir);
  return 0;
}
