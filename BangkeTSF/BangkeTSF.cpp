#include "stdafx.h"
#include <BangkeProtocol.h>
#include <cstddef>
#include <cstring>

#include <BangkeIPCData.h>
#include <thread>
#include <shellapi.h>
#include <tlhelp32.h>
#include "BangkeTSF.h"
#include "CandidateList.h"
#include "LanguageBar.h"
#include "Compartment.h"
#include "ResponseParser.h"
#include "EditSession.h"
#include <sstream>

static void error_message(const WCHAR* msg) {
  static DWORD next_tick = 0;
  DWORD now = GetTickCount();
  if (now > next_tick) {
    next_tick = now + 10000;  // (ms)
    MessageBox(NULL, msg, get_bangke_ime_name().c_str(), MB_ICONERROR | MB_OK);
  }
}

BangkeTSF::BangkeTSF() {
  _cRef = 1;

  _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;

  _dwTextEditSinkCookie = TF_INVALID_COOKIE;
  _dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
  _dwThreadFocusSinkCookie = TF_INVALID_COOKIE;
  _fTestKeyDownPending = FALSE;
  _fTestKeyUpPending = FALSE;

  _fCUASWorkaroundTested = _fCUASWorkaroundEnabled = FALSE;

  _cand = new CCandidateList(this);

  DllAddRef();
}

BangkeTSF::~BangkeTSF() {
  _StopSnapshotListener();
  DllRelease();
}

STDMETHODIMP BangkeTSF::QueryInterface(REFIID riid, void** ppvObject) {
  if (ppvObject == NULL)
    return E_INVALIDARG;

  *ppvObject = NULL;

  if (IsEqualIID(riid, IID_IUnknown) ||
      IsEqualIID(riid, IID_ITfTextInputProcessor))
    *ppvObject = (ITfTextInputProcessor*)this;
  else if (IsEqualIID(riid, IID_ITfTextInputProcessorEx))
    *ppvObject = (ITfTextInputProcessorEx*)this;
  else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    *ppvObject = (ITfThreadMgrEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextEditSink))
    *ppvObject = (ITfTextEditSink*)this;
  else if (IsEqualIID(riid, IID_ITfTextLayoutSink))
    *ppvObject = (ITfTextLayoutSink*)this;
  else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    *ppvObject = (ITfKeyEventSink*)this;
  else if (IsEqualIID(riid, IID_ITfCompositionSink))
    *ppvObject = (ITfCompositionSink*)this;
  else if (IsEqualIID(riid, IID_ITfEditSession))
    *ppvObject = (ITfEditSession*)this;
  else if (IsEqualIID(riid, IID_ITfThreadFocusSink))
    *ppvObject = (ITfThreadFocusSink*)this;
  else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider))
    *ppvObject = (ITfDisplayAttributeProvider*)this;

  if (*ppvObject) {
    AddRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) BangkeTSF::AddRef() {
  return ++_cRef;
}

STDMETHODIMP_(ULONG) BangkeTSF::Release() {
  LONG cr = --_cRef;

  assert(_cRef >= 0);

  if (_cRef == 0)
    delete this;

  return cr;
}

STDMETHODIMP BangkeTSF::Activate(ITfThreadMgr* pThreadMgr,
                                 TfClientId tfClientId) {
  return ActivateEx(pThreadMgr, tfClientId, 0U);
}

STDMETHODIMP BangkeTSF::Deactivate() {
  _StopSnapshotListener();
  m_client.EndSession();

  _InitTextEditSink(com_ptr<ITfDocumentMgr>());

  _UninitThreadMgrEventSink();

  _UninitKeyEventSink();
  _UninitPreservedKey();

  _UninitLanguageBar();

  _UninitCompartment();

  _UninitThreadMgrEventSink();

  _pThreadMgr = NULL;

  _tfClientId = TF_CLIENTID_NULL;

  _cand->DestroyAll();

  return S_OK;
}

STDMETHODIMP BangkeTSF::ActivateEx(ITfThreadMgr* pThreadMgr,
                                   TfClientId tfClientId,
                                   DWORD dwFlags) {
  com_ptr<ITfDocumentMgr> pDocMgrFocus;
  _activateFlags = dwFlags;

  _pThreadMgr = pThreadMgr;
  _tfClientId = tfClientId;

  if (!_InitThreadMgrEventSink())
    goto ExitError;

  if ((_pThreadMgr->GetFocus(&pDocMgrFocus) == S_OK) &&
      (pDocMgrFocus != NULL)) {
    _InitTextEditSink(pDocMgrFocus);
  }

  if (!_InitKeyEventSink())
    goto ExitError;

  // if (!_InitDisplayAttributeGuidAtom())
  //	goto ExitError;
  //	some app might init failed because it not provide DisplayAttributeInfo,
  // like some opengl stuff
  _InitDisplayAttributeGuidAtom();

  if (!_InitPreservedKey())
    goto ExitError;

  if (!_InitLanguageBar())
    goto ExitError;

  if (!_IsKeyboardOpen())
    _SetKeyboardOpen(TRUE);

  if (!_InitCompartment())
    goto ExitError;
  if (!_InitThreadFocusSink())
    goto ExitError;

  _EnsureServerConnected();

  return S_OK;

ExitError:
  Deactivate();
  return E_FAIL;
}

STDMETHODIMP BangkeTSF::OnSetThreadFocus() {
  std::wstring _ToggleImeOnOpenClose{};
  RegGetStringValue(HKEY_CURRENT_USER, L"Software\\Bangke",
                    L"ToggleImeOnOpenClose", _ToggleImeOnOpenClose);
  _isToOpenClose = (_ToggleImeOnOpenClose == L"yes");
  if (m_client.Echo()) {
    m_client.ProcessKeyEvent(0);
    bangke::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style(),
                                  &_last_applied_serial);
    bool ok = m_client.GetResponseData(std::ref(parser));
    if (ok)
      _UpdateLanguageBar(_status);
  }
  return S_OK;
}
STDMETHODIMP BangkeTSF::OnKillThreadFocus() {
  _AbortComposition();
  return S_OK;
}
BOOL BangkeTSF::_InitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return FALSE;
  if (FAILED(pSource->AdviseSink(IID_ITfThreadFocusSink,
                                 (ITfThreadFocusSink*)this,
                                 &_dwThreadFocusSinkCookie)))
    return FALSE;
  return TRUE;
}
void BangkeTSF::_UninitThreadFocusSink() {
  com_ptr<ITfSource> pSource;
  if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
    return;
  if (FAILED(pSource->UnadviseSink(_dwThreadFocusSinkCookie)))
    return;
}

STDMETHODIMP BangkeTSF::OnActivated(REFCLSID clsid,
                                    REFGUID guidProfile,
                                    BOOL isActivated) {
  if (!IsEqualCLSID(clsid, c_clsidTextService)) {
    return S_OK;
  }

  if (isActivated) {
    _ShowLanguageBar(TRUE);
    _UpdateLanguageBar(_status);
  } else {
    _DeleteCandidateList();
    _ShowLanguageBar(FALSE);
  }
  return S_OK;
}





void BangkeTSF::_AsyncRefresh(UINT_PTR seq) {
  wchar_t map_name[64];
  swprintf_s(map_name, L"Local\\BangkeSnap_%u", m_client.SessionId());
  HANDLE map = OpenFileMappingW(FILE_MAP_READ, FALSE, map_name);
  if (!map)
    return;
  auto* view = (BYTE*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
  if (!view) {
    CloseHandle(map);
    return;
  }
  // 槽内容即协议帧:头 magic 自证,长度自描述
  uint32_t magic = 0;
  memcpy(&magic, view, sizeof(magic));
  bool ok = magic == bangke::kFrameMagic;
  std::wstring text;
  if (ok) {
    // 帧自描述长度(payload_len 位于帧头第 16 字节,勿与 ipc_sid@8 混淆)
    uint32_t payload_len = 0;
    memcpy(&payload_len, view + offsetof(bangke::FrameHeader, payload_len),
           sizeof(payload_len));
    const size_t frame_bytes = sizeof(bangke::FrameHeader) + payload_len;
    if (frame_bytes > 128 * 1024 || frame_bytes % 2)
      ok = false;
    else
      text.assign((const wchar_t*)view, frame_bytes / 2);
  }
  UnmapViewOfFile(view);
  CloseHandle(map);
  if (!ok)
    return;

  // 序数判定:推送帧只做纯视觉更新(_UpdateUI 不触 TSF edit session,
  // commit 只经管道响应由 DoEditSession 投递,推送动不了它),
  // 唯一要挡的是乱序回放——比已应用序号旧的帧直接丢弃
  std::wstring commit;
  bangke::Config config;
  auto context = std::make_shared<bangke::Context>();
  bangke::Status status;
  bangke::FrameHeader hdr;
  if (!bangke::ParseFramePrefix(reinterpret_cast<const uint8_t*>(text.c_str()),
                                text.size() * sizeof(wchar_t), &hdr, &commit,
                                context.get(), &status, &config,
                                &_cand->style()))
    return;
  {  // TEMP-TRACE: 推送应用判定
    wchar_t p[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(L"%TEMP%\\bk_push_trace.log", p, MAX_PATH);
    HANDLE f = CreateFileW(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE) {
      char line[160];
      snprintf(line, sizeof(line),
               "[%u] push serial=%u floor=%u candies=%d %s\r\n",
               GetCurrentProcessId(), hdr.key_serial, _last_applied_serial,
               (int)context->cinfo.candies.size(),
               hdr.key_serial < _last_applied_serial ? "DROP-STALE" : "APPLY");
      DWORD w;
      WriteFile(f, line, (DWORD)strlen(line), &w, NULL);
      CloseHandle(f);
    }
  }
  if (hdr.key_serial < _last_applied_serial)
    return;
  _UpdateUI(*context, status);
}

void BangkeTSF::_Reconnect() {
  m_client.Disconnect();
  m_client.Connect(NULL);
  m_client.StartSession();
  _StartSnapshotListener();
  bangke::ResponseParser parser(NULL, NULL, &_status, NULL, &_cand->style(),
                                &_last_applied_serial);
  bool ok = m_client.GetResponseData(std::ref(parser));
  if (ok) {
    _UpdateLanguageBar(_status);
  }
}

// 快照就绪监听:server 在会话建立时创建 Local\BangkeSnapEvt_<sid>(auto-reset)。
// 本线程等事件,醒来后向候选窗投递进程内私有消息——不经 UIPI,提权应用可达。
void BangkeTSF::_StartSnapshotListener() {
  _StopSnapshotListener();
  const DWORD sid = m_client.SessionId();
  if (!sid)
    return;
  _snap_stop = false;
  // 生命周期:Deactivate 与析构都会 join,线程窗口内 this 有效
  _snap_thread = std::thread([sid, this]() {
    wchar_t evt_name[64];
    swprintf_s(evt_name, L"Local\\BangkeSnapEvt_%u", sid);
    // server 竞态下可能尚未建事件:有限重试
    HANDLE evt = NULL;
    for (int i = 0; i < 10 && !evt && !_snap_stop; ++i) {
      evt = OpenEventW(SYNCHRONIZE, FALSE, evt_name);
      if (!evt)
        Sleep(100);
    }
    while (!_snap_stop && evt) {
      if (WaitForSingleObject(evt, 500) != WAIT_OBJECT_0)
        continue;
      if (_snap_stop)
        break;
      if (_cand)
        _cand->PostSnapshotReady();
    }
    if (evt)
      CloseHandle(evt);
  });
}

void BangkeTSF::_StopSnapshotListener() {
  if (_snap_thread.joinable()) {
    _snap_stop = true;
    // 事件可能已被关掉,唤醒不了就靠 500ms 超时兜底
    wchar_t evt_name[64];
    swprintf_s(evt_name, L"Local\\BangkeSnapEvt_%u", m_client.SessionId());
    if (HANDLE evt = OpenEventW(EVENT_MODIFY_STATE, FALSE, evt_name)) {
      SetEvent(evt);
      CloseHandle(evt);
    }
    _snap_thread.join();
  }
}

static unsigned int retry = 0;

bool BangkeTSF::_EnsureServerConnected() {
  if (!m_client.Echo()) {
    _Reconnect();
    retry++;
    if (retry >= 6) {
      HANDLE hMutex = CreateMutex(NULL, TRUE, L"BangkeDeployerExclusiveMutex");
      const auto count_server_process = []() -> int {
        int count = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE)
          return 0;
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
          do {
            if (_wcsicmp(pe.szExeFile, L"BangkeServer.exe") == 0)
              count++;
          } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return count;
      };
      if (!m_client.Echo() && GetLastError() != ERROR_ALREADY_EXISTS &&
          !count_server_process()) {
        std::wstring dir = _GetRootDir();
        std::thread th([dir, this]() {
          ShellExecuteW(NULL, L"open", (dir + L"\\start_service.bat").c_str(),
                        NULL, dir.c_str(), SW_HIDE);
          // wait 500ms, then reconnect
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          _Reconnect();
        });
        th.detach();
      }
      if (hMutex) {
        CloseHandle(hMutex);
      }
      retry = 0;
    }
    return (m_client.Echo() != 0);
  } else {
    return true;
  }
}
