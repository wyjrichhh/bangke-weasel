#include "stdafx.h"
#include <logging.h>
#include <BangkeProtocol.h>
#include <BangkeRime.h>
#include <StringAlgorithm.hpp>
#include <BangkeConstants.h>
#include <BangkeUtility.h>
#include <BangkeNativeTheme.h>
#include <BangkeColorOpacity.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <array>
#include <vector>
#include <regex>
#include <atomic>
#include <rime_api.h>
#include <Sddl.h>

#define TRANSPARENT_COLOR 0x00000000
#define ARGB2ABGR(value)                                 \
  ((value & 0xff000000) | ((value & 0x000000ff) << 16) | \
   (value & 0x0000ff00) | ((value & 0x00ff0000) >> 16))
#define RGBA2ABGR(value)                                   \
  (((value & 0xff) << 24) | ((value & 0xff000000) >> 24) | \
   ((value & 0x00ff0000) >> 8) | ((value & 0x0000ff00) << 8))
typedef enum { COLOR_ABGR = 0, COLOR_ARGB, COLOR_RGBA } ColorFormat;

using namespace bangke;

static RimeApi* rime_api;

// AI 快照推送槽:每会话一块共享内存 + 一个 auto-reset 事件。
// 槽内容即 BangkeProtocol 帧(自描述长度,覆写旧值无需清零)。
// 命名事件让前端在自身监听线程内被唤醒、进程内 PostMessage 触达候选窗
// ——旧方案 FindWindowEx 广播会被 UIPI 拦掉(提权应用收不到),全局单槽
// 也无会话隔离。DACL 放开 Everyone 与 ALL APP PACKAGES(UWP 可开)。
static const size_t kSnapSlotBytes = 128 * 1024;

static void _SnapSlotNames(BangkeSessionId ipc_id, wchar_t* map_name,
                           size_t map_n, wchar_t* evt_name, size_t evt_n) {
  swprintf_s(map_name, map_n, L"Local\\BangkeSnap_%u", (DWORD)ipc_id);
  swprintf_s(evt_name, evt_n, L"Local\\BangkeSnapEvt_%u", (DWORD)ipc_id);
}

// 句柄必须持有:命名内核对象在最后一个句柄关闭时即销毁,
// "创建后立刻 CloseHandle"曾令槽/事件名存实亡(推送与客户端都 Open 不到)
static std::mutex s_snap_handles_mutex;
static std::map<DWORD, std::pair<HANDLE, HANDLE>> s_snap_handles;

// 帧全序:每次 ProcessKeyEvent 递增;响应帧与推送帧携带当前值,
// 客户端仅应用 key_serial >= last_applied 的帧,推送乱序到达即丢弃
static std::atomic<uint32_t> s_key_serial{0};

static SECURITY_ATTRIBUTES* _SnapSlotSA() {
  // 对 Section/Event 对象授 GENERIC_ALL:SY/Everyone/AllAppPackages
  static SECURITY_ATTRIBUTES sa = [] {
    SECURITY_ATTRIBUTES s{};
    s.nLength = sizeof(s);
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;GA;;;SY)(A;;GA;;;WD)(A;;GA;;;AC)", SDDL_REVISION_1,
        &s.lpSecurityDescriptor, NULL);
    return s;
  }();
  return &sa;
}

BangkeSessionId _GenerateNewBangkeSessionId(SessionStatusMap sm, DWORD pid) {
  if (sm.empty())
    return (BangkeSessionId)(pid + 1);
  return (BangkeSessionId)(sm.rbegin()->first + 1);
}

int expand_ibus_modifier(int m) {
  return (m & 0xff) | ((m & 0xff00) << 16);
}

BangkeRimeHandler::BangkeRimeHandler(UI* ui)
    : m_ui(ui),
      m_active_session(0),
      m_disabled(true),
      m_current_dark_mode(false),
      m_global_ascii_mode(false),
      m_show_notifications_time(1200),
      _UpdateUICallback(NULL) {
  m_ui->InServer() = true;
  rime_api = rime_get_api();
  assert(rime_api);
  m_pid = GetCurrentProcessId();
  uint16_t msbit = 0;
  for (auto i = 31; i >= 0; i--) {
    if (m_pid & (1 << i)) {
      msbit = i;
      break;
    }
  }
  m_pid = (m_pid << (31 - msbit));
  _Setup();
}

BangkeRimeHandler::~BangkeRimeHandler() {
  m_show_notifications.clear();
  m_session_status_map.clear();
  m_app_options.clear();
}

bool add_session = false;
void _UpdateUIStyle(RimeConfig* config, UI* ui, bool initialize);
bool _UpdateUIStyleColor(RimeConfig* config,
                         UIStyle& style,
                         const std::string& color = std::string(),
                         bool dark_mode = false);
void _LoadAppOptions(RimeConfig* config, AppOptionsByAppName& app_options);

void _RefreshTrayIcon(const RimeSessionId session_id,
                      const std::function<void()> _UpdateUICallback) {
  // Dangerous, don't touch
  static char app_name[256] = {0};
  auto ret = rime_api->get_property(session_id, "client_app", app_name,
                                    sizeof(app_name) - 1);
  if (!ret || u8tow(app_name) == std::wstring(L"explorer.exe"))
    boost::thread th([=]() {
      ::Sleep(100);
      if (_UpdateUICallback)
        _UpdateUICallback();
    });
  else if (_UpdateUICallback)
    _UpdateUICallback();
}

void BangkeRimeHandler::_Setup() {
  RIME_STRUCT(RimeTraits, bangke_traits);
  std::string shared_dir = wtou8(BangkeSharedDataPath().wstring());
  std::string user_dir = wtou8(BangkeUserDataPath().wstring());
  bangke_traits.shared_data_dir = shared_dir.c_str();
  bangke_traits.user_data_dir = user_dir.c_str();
  bangke_traits.prebuilt_data_dir = bangke_traits.shared_data_dir;
  std::string distribution_name = wtou8(get_bangke_ime_name());
  bangke_traits.distribution_name = distribution_name.c_str();
  bangke_traits.distribution_code_name = BANGKE_CODE_NAME;
  bangke_traits.distribution_version = BANGKE_VERSION;
  bangke_traits.app_name = "rime.bangke";
  std::string log_dir = BangkeLogPath().u8string();
  bangke_traits.log_dir = log_dir.c_str();
  rime_api->setup(&bangke_traits);
  rime_api->set_notification_handler(&BangkeRimeHandler::OnNotify, this);
}

void BangkeRimeHandler::Initialize() {
  _Initialize(false);
}

void BangkeRimeHandler::_Initialize(bool ignore_deployer) {
  m_disabled = !ignore_deployer && _IsDeployerRunning();
  if (m_disabled) {
    return;
  }

  // 首启兜底:安装器的 FirstDeploy 若被互斥/上下文问题跳过,
  // 种子在这里补上(幂等,存在性检查开销可忽略),部署才能带上雾凇与 AI
  BangkeEnsureUserSeeds();

  LOG(INFO) << "Initializing la rime.";
  rime_api->initialize(NULL);
  if (rime_api->start_maintenance(/*full_check = */ False)) {
    m_disabled = true;
    rime_api->join_maintenance_thread();
  }

  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    if (m_ui) {
      _UpdateUIStyle(&config, m_ui, true);
      _UpdateShowNotifications(&config, true);
      m_current_dark_mode = IsUserDarkMode();
      if (m_current_dark_mode) {
        const int BUF_SIZE = 255;
        char buffer[BUF_SIZE + 1] = {0};
        if (rime_api->config_get_string(&config, "style/color_scheme_dark",
                                        buffer, BUF_SIZE)) {
          std::string color_name(buffer);
          _UpdateUIStyleColor(&config, m_ui->style(), color_name, true);
        } else if (rime_api->config_get_string(&config, "style/color_scheme",
                                               buffer, BUF_SIZE) &&
                   std::string(buffer) == "native") {
          // native 自身按外观解析,无需 color_scheme_dark(mac 同语义)
          _UpdateUIStyleColor(&config, m_ui->style(), "native", true);
        }
      }
      m_base_style = m_ui->style();
    }
    Bool global_ascii = false;
    if (rime_api->config_get_bool(&config, "global_ascii", &global_ascii))
      m_global_ascii_mode = !!global_ascii;
    if (!rime_api->config_get_int(&config, "show_notifications_time",
                                  &m_show_notifications_time))
      m_show_notifications_time = 1200;
    _LoadAppOptions(&config, m_app_options);
    rime_api->config_close(&config);
  }
  m_last_schema_id.clear();
}

void BangkeRimeHandler::Finalize() {
  _RequestStopAiPump(0);
  m_active_session = 0;
  m_disabled = true;
  m_session_status_map.clear();
  LOG(INFO) << "Finalizing la rime.";
  rime_api->finalize();
}

DWORD BangkeRimeHandler::FindSession(BangkeSessionId ipc_id) {
  if (m_disabled)
    return 0;
  Bool found = rime_api->find_session(to_session_id(ipc_id));
  DLOG(INFO) << "Find session: session_id = " << to_session_id(ipc_id)
             << ", found = " << found;
  return found ? (ipc_id) : 0;
}

DWORD BangkeRimeHandler::AddSession(LPWSTR buffer, EatLine eat) {
  if (m_disabled) {
    DLOG(INFO) << "Trying to resume service.";
    EndMaintenance();
    if (m_disabled)
      return 0;
  }
  RimeSessionId session_id = (RimeSessionId)rime_api->create_session();
  if (m_global_ascii_mode) {
    for (const auto& pair : m_session_status_map) {
      if (pair.first) {
        rime_api->set_option(session_id, "ascii_mode",
                             !!pair.second.status.is_ascii_mode);
        break;
      }
    }
  }

  BangkeSessionId ipc_id =
      _GenerateNewBangkeSessionId(m_session_status_map, m_pid);
  DLOG(INFO) << "Add session: created session_id = " << session_id
             << ", ipc_id = " << ipc_id;
  {
    // 会话建立即建槽与事件,前端可确定性打开(推送前已存在)
    wchar_t map_name[64], evt_name[64];
    _SnapSlotNames(ipc_id, map_name, 64, evt_name, 64);
    HANDLE map = CreateFileMappingW(INVALID_HANDLE_VALUE, _SnapSlotSA(),
                                    PAGE_READWRITE, 0, (DWORD)kSnapSlotBytes,
                                    map_name);
    HANDLE evt = CreateEventW(_SnapSlotSA(), FALSE, FALSE, evt_name);
    std::lock_guard<std::mutex> lock(s_snap_handles_mutex);
    s_snap_handles[(DWORD)ipc_id] = {map, evt};
  }
  SessionStatus& session_status = new_session_status(ipc_id);
  session_status.style = m_base_style;
  session_status.session_id = session_id;
  _ReadClientInfo(ipc_id, buffer);
  // 同一应用出现第二条会话(部署恢复的旧 ipc + 客户端重连的新 ipc)只留新的,
  // 旧的 rime 会话当场销毁,防泄漏
  if (!session_status.client_app.empty()) {
    for (auto it = m_session_status_map.begin();
         it != m_session_status_map.end();) {
      if (it->first != ipc_id &&
          it->second.client_app == session_status.client_app) {
        rime_api->destroy_session(it->second.session_id);
        it = m_session_status_map.erase(it);
      } else
        ++it;
    }
  }

  RIME_STRUCT(RimeStatus, status);
  if (rime_api->get_status(session_id, &status)) {
    std::string schema_id = status.schema_id;
    m_last_schema_id = schema_id;
    _LoadSchemaSpecificSettings(ipc_id, schema_id);
    _LoadAppInlinePreeditSet(ipc_id, true);
    _UpdateInlinePreeditStatus(ipc_id);
    _RefreshTrayIcon(session_id, _UpdateUICallback);
    session_status.status = status;
    session_status.__synced = false;
    rime_api->free_status(&status);
  }
  m_ui->style() = session_status.style;
  // show session's welcome message :-) if any
  if (eat) {
    _Respond(ipc_id, eat);
  }
  add_session = true;
  _UpdateUI(ipc_id);
  add_session = false;
  m_active_session = ipc_id;
  return ipc_id;
}

DWORD BangkeRimeHandler::RemoveSession(BangkeSessionId ipc_id) {
  _RequestStopAiPump(ipc_id);
  {
    std::lock_guard<std::mutex> lock(s_snap_handles_mutex);
    auto it = s_snap_handles.find((DWORD)ipc_id);
    if (it != s_snap_handles.end()) {
      if (it->second.first)
        CloseHandle(it->second.first);
      if (it->second.second)
        CloseHandle(it->second.second);
      s_snap_handles.erase(it);
    }
  }
  if (m_ui)
    m_ui->Hide();
  if (m_disabled)
    return 0;
  DLOG(INFO) << "Remove session: session_id = " << to_session_id(ipc_id);
  // TODO: force committing? otherwise current composition would be lost
  rime_api->destroy_session(to_session_id(ipc_id));
  m_session_status_map.erase(ipc_id);
  m_pending_restore.erase((DWORD)ipc_id);
  m_active_session = 0;
  return 0;
}

void BangkeRimeHandler::UpdateColorTheme(BOOL darkMode) {
  RimeConfig config = {NULL};
  if (rime_api->config_open("weasel", &config)) {
    if (m_ui) {
      _UpdateUIStyle(&config, m_ui, true);
      m_current_dark_mode = darkMode;
      if (darkMode) {
        const int BUF_SIZE = 255;
        char buffer[BUF_SIZE + 1] = {0};
        if (rime_api->config_get_string(&config, "style/color_scheme_dark",
                                        buffer, BUF_SIZE)) {
          std::string color_name(buffer);
          _UpdateUIStyleColor(&config, m_ui->style(), color_name, true);
        } else if (rime_api->config_get_string(&config, "style/color_scheme",
                                               buffer, BUF_SIZE) &&
                   std::string(buffer) == "native") {
          _UpdateUIStyleColor(&config, m_ui->style(), "native", true);
        }
      }
      m_base_style = m_ui->style();
    }
    rime_api->config_close(&config);
  }

  for (auto& pair : m_session_status_map) {
    RIME_STRUCT(RimeStatus, status);
    if (rime_api->get_status(to_session_id(pair.first), &status)) {
      _LoadSchemaSpecificSettings(pair.first, std::string(status.schema_id));
      _LoadAppInlinePreeditSet(pair.first, true);
      _UpdateInlinePreeditStatus(pair.first);
      pair.second.status = status;
      pair.second.__synced = false;
      rime_api->free_status(&status);
    }
  }
  m_ui->style() = get_session_status(m_active_session).style;
}

namespace {
// 供纯推理模式判断"这一下是不是普通字符键"。Shift 不算修饰：! ? : 本来就要 Shift 打
bool IsPlainKeyDown(const bangke::KeyEvent& keyEvent) {
  constexpr UINT kDisqualifying =
      ibus::RELEASE_MASK | ibus::CONTROL_MASK | ibus::ALT_MASK |
      ibus::SUPER_MASK | ibus::META_MASK;
  return (keyEvent.mask & kDisqualifying) == 0;
}

bool IsAiOnlyExtraInput(UINT keycode) {
  return keycode > 0 && keycode < 0x80 &&
         strchr(kAiOnlyExtraInput, (char)keycode) != nullptr;
}

bool HasRimeInput(RimeApi* api, RimeSessionId session_id) {
  const char* input = api->get_input(session_id);
  return input && *input;
}
}  // namespace

BOOL BangkeRimeHandler::ProcessKeyEvent(KeyEvent keyEvent,
                                            BangkeSessionId ipc_id,
                                            EatLine eat) {
  DLOG(INFO) << "Process key event: keycode = " << keyEvent.keycode
             << ", mask = " << keyEvent.mask << ", ipc_id = " << ipc_id;
  if (m_disabled)
    return FALSE;
  ++s_key_serial;  // 本次按键的响应帧与之后的推送帧按此排序
  RimeSessionId session_id = to_session_id(ipc_id);
  SessionStatus& session_status = get_session_status(ipc_id);

  // 下列两条只在纯 AI 推理模式下改道，普通模式一个字符都不碰。
  //
  // 空格：该模式下菜单只有模型那一条，组字期间没有候选可确认。照常交给 rime，它会
  // 把原始拼音直接上屏——而那些拼音还会进 commit_history，污染下一次推理的上下文。
  // 菜单空着就没有可确认的东西，吞掉即可。要求正在组字：没在组字时菜单同样是空的，
  // 那时吞空格会破坏普通打字。
  //
  // 数字与标点：菜单活着时它们各有本职——selector 拿数字选词、punctuator 把标点直接
  // 上屏、key_binder 先一步用 , . [ ] 翻页。把字符直接追加进 composition 就绕开了整条
  // 链，那三处都不必改配置。同样要求正在组字，好让这些字符在没组字时仍是它们自己。
  if (session_status.ai_only && IsPlainKeyDown(keyEvent)) {
    if (keyEvent.keycode == ibus::space && session_status.menu_candidates == 0 &&
        HasRimeInput(rime_api, session_id)) {
      _Respond(ipc_id, eat);
      _UpdateUI(ipc_id);
      m_active_session = ipc_id;
      return TRUE;
    }
    if (IsAiOnlyExtraInput(keyEvent.keycode) &&
        HasRimeInput(rime_api, session_id)) {
      // get_input 的指针指进 Context 内部的字符串，set_input 会把它换掉，先拷出来
      std::string next = rime_api->get_input(session_id);
      next += (char)keyEvent.keycode;
      // set_input 自己会触发 update_notifier → Compose，候选栏随之更新，不必再排一趟
      rime_api->set_input(session_id, next.c_str());
      _Respond(ipc_id, eat);
      _UpdateUI(ipc_id);
      m_active_session = ipc_id;
      return TRUE;
    }
  }

  Bool handled = rime_api->process_key(session_id, keyEvent.keycode,
                                       expand_ibus_modifier(keyEvent.mask));
  // vim_mode when keydown only
  if (!handled && !(keyEvent.mask & ibus::Modifier::RELEASE_MASK)) {
    bool isVimBackInCommandMode =
        (keyEvent.keycode == ibus::Keycode::Escape) ||
        ((keyEvent.mask & (1 << 2)) &&
         (keyEvent.keycode == ibus::Keycode::XK_c ||
          keyEvent.keycode == ibus::Keycode::XK_C ||
          keyEvent.keycode == ibus::Keycode::XK_bracketleft));
    if (isVimBackInCommandMode &&
        rime_api->get_option(session_id, "vim_mode") &&
        !rime_api->get_option(session_id, "ascii_mode")) {
      rime_api->set_option(session_id, "ascii_mode", True);
    }
  }
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
  return (BOOL)handled;
}

void BangkeRimeHandler::CommitComposition(BangkeSessionId ipc_id) {
  DLOG(INFO) << "Commit composition: ipc_id = " << ipc_id;
  if (m_disabled)
    return;
  rime_api->commit_composition(to_session_id(ipc_id));
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
}

void BangkeRimeHandler::ClearComposition(BangkeSessionId ipc_id) {
  DLOG(INFO) << "Clear composition: ipc_id = " << ipc_id;
  if (m_disabled)
    return;
  rime_api->clear_composition(to_session_id(ipc_id));
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
}


void BangkeRimeHandler::SelectCandidateOnCurrentPage(
    size_t index,
    BangkeSessionId ipc_id) {
  DLOG(INFO) << "select candidate on current page, ipc_id = " << ipc_id
             << ", index = " << index;
  if (m_disabled)
    return;
  rime_api->select_candidate_on_current_page(to_session_id(ipc_id), index);
}

bool BangkeRimeHandler::HighlightCandidateOnCurrentPage(
    size_t index,
    BangkeSessionId ipc_id,
    EatLine eat) {
  DLOG(INFO) << "highlight candidate on current page, ipc_id = " << ipc_id
             << ", index = " << index;
  bool res = rime_api->highlight_candidate_on_current_page(
      to_session_id(ipc_id), index);
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  return res;
}

bool BangkeRimeHandler::ChangePage(bool backward,
                                       BangkeSessionId ipc_id,
                                       EatLine eat) {
  DLOG(INFO) << "change page, ipc_id = " << ipc_id
             << (backward ? "backward" : "foreward");
  bool res = rime_api->change_page(to_session_id(ipc_id), backward);
  _Respond(ipc_id, eat);
  _UpdateUI(ipc_id);
  return res;
}

void BangkeRimeHandler::FocusIn(DWORD client_caps, BangkeSessionId ipc_id) {
  DLOG(INFO) << "Focus in: ipc_id = " << ipc_id
             << ", client_caps = " << client_caps;
  if (m_disabled)
    return;
  _UpdateUI(ipc_id);
  m_active_session = ipc_id;
}

void BangkeRimeHandler::FocusOut(DWORD param, BangkeSessionId ipc_id) {
  DLOG(INFO) << "Focus out: ipc_id = " << ipc_id;
  if (m_ui)
    m_ui->Hide();
  // 焦点走了就没有要顶的候选栏了，别让泵继续空转
  _RequestStopAiPump(ipc_id);
  m_active_session = 0;
}

void BangkeRimeHandler::UpdateInputPosition(RECT const& rc,
                                                BangkeSessionId ipc_id) {
  DLOG(INFO) << "Update input position: (" << rc.left << ", " << rc.top
             << "), ipc_id = " << ipc_id
             << ", m_active_session = " << m_active_session;
  if (m_ui)
    m_ui->UpdateInputPosition(rc);
  if (m_disabled)
    return;
  if (m_active_session != ipc_id) {
    _UpdateUI(ipc_id);
    m_active_session = ipc_id;
  }
}

std::string BangkeRimeHandler::m_message_type;
std::string BangkeRimeHandler::m_message_value;
std::string BangkeRimeHandler::m_message_label;
std::string BangkeRimeHandler::m_option_name;
std::mutex BangkeRimeHandler::m_notifier_mutex;


void BangkeRimeHandler::_PushAiSnapshot(uintptr_t rime_sid) {
  // rime session -> ipc session
  BangkeSessionId ipc_id = 0;
  for (auto& pair : m_session_status_map) {
    if (pair.second.session_id == rime_sid) {
      ipc_id = pair.first;
      break;
    }
  }
  if (!ipc_id)
    return;

  // 复用会话协商后的帧渲染(include_commit=false:get_commit 读一次即清空,
  // 推送帧绝不携带 commit,否则会偷走按键响应的上屏文本)。
  // consume_style_sync=false:推送走共享内存,客户端可能因序号陈旧整帧丢弃,
  // 就此退休样式同步标记会连样式一起丢掉
  std::wstring wire;
  if (!_RespondFrame(ipc_id, [&wire](std::wstring& s) -> bool {
        wire = std::move(s);
        return true;
      }, /*include_commit=*/false, /*consume_style_sync=*/false) ||
      wire.empty())
    return;


  HANDLE map = NULL, evt = NULL;
  {
    std::lock_guard<std::mutex> lock(s_snap_handles_mutex);
    auto it = s_snap_handles.find((DWORD)ipc_id);
    if (it != s_snap_handles.end()) {
      map = it->second.first;
      evt = it->second.second;
    }
  }
  if (!map || !evt)
    return;
  auto* view = (BYTE*)MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, 0);
  if (view && wire.size() * sizeof(wchar_t) <= kSnapSlotBytes) {
    memcpy(view, wire.c_str(), wire.size() * sizeof(wchar_t));
    SetEvent(evt);
  }
  if (view)
    UnmapViewOfFile(view);
}

void BangkeRimeHandler::OnNotify(void* context_object,
                                     uintptr_t session_id,
                                     const char* message_type,
                                     const char* message_value) {
  // rime 的通知在触发线程上同步回调——包括部署线程与插件 worker 线程。
  // 本函数只允许拷贝字符串 + PostMessage，绝不调用 rime API、绝不等待任何锁
  // （fcitx5-rime 的 eventDispatcher_.schedule / squirrel 的 GCD 同款纪律）。
  // 在此等锁或做重活，会与"销毁会话时 join 工作线程"互等成死锁。
  BangkeRimeHandler* self =
      reinterpret_cast<BangkeRimeHandler*>(context_object);
  if (!self || !message_type || !message_value)
    return;

  if (!strcmp(message_type, "property")) {
    // 属性通知里前端唯一关心的是 ai_predict/pending，但它不走这里：那是插件在
    // Compose（按键线程）里写的，前端在 _UpdateUI 之后自己读，见 _MaybeRequestAiPump
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_notifier_mutex);
    m_message_type = message_type;
    m_message_value = message_value;
  }
  if (RIME_API_AVAILABLE(rime_api, get_state_label) &&
      !strcmp(message_type, "option")) {
    // option 通知只来自按键处理（管道路径，已持有 API 串行权），就地补标签安全
    Bool state = message_value[0] != '!';
    const char* option_name = message_value + !state;
    const char* state_label =
        rime_api->get_state_label(session_id, option_name, state);
    std::lock_guard<std::mutex> lock(m_notifier_mutex);
    m_option_name = option_name;
    if (state_label) {
      m_message_label = std::string(state_label);
    }
  }
}

void BangkeRimeHandler::SetEventWindow(HWND wnd) {
  m_event_wnd = wnd;
}

void BangkeRimeHandler::OnDeferredEvent(int event,
                                            uintptr_t rime_session_id) {
  // 这里传的是 ipc 会话号，不是 rime 会话号：泵按 ipc 号认会话，rime 会话号是
  // 对象地址，销毁后新建的会话可能复用同一个值
  if (event == BK_EVENT_AI_PUMP) {
    AiPumpArm((BangkeSessionId)rime_session_id);
  } else if (event == BK_EVENT_AI_PUMP_STOP) {
    const BangkeSessionId ipc_id = (BangkeSessionId)rime_session_id;
    // 会话 A 退出时泵可能已经在为 B 跑，只有对得上才停
    if (ipc_id == 0 || ipc_id == m_ai_pump_ipc_id)
      _StopAiPump();
  }
}

// 把客户端读到的「光标前文本」交给插件。插件在自己的 Compose 阶段就要用，所以必须在
// process_key 之前写进去——调用点见 ServerImpl::OnKeyEvent。
//
// 值没变就不写：Context::set_property 每次都会发通知，插件据此重跑一趟推理，而打字
// 过程中这段文本绝大多数时候根本不动。
void BangkeRimeHandler::SetClientContext(BangkeSessionId ipc_id,
                                         const std::wstring& text) {
  auto it = m_session_status_map.find(ipc_id);
  if (it == m_session_status_map.end() || !it->second.session_id)
    return;
  std::string utf8 = wtou8(text);
  if (utf8 == it->second.published_context)
    return;
  it->second.published_context = utf8;
  rime_api->set_property(it->second.session_id, "ai_predict/client_context",
                         utf8.c_str());
}

bool BangkeRimeHandler::_IsAiPending(RimeSessionId session_id) {
  if (!session_id)
    return false;
  char buffer[8] = {0};
  // 值为空时 get_property 本身就返回 False，正好当作「没有推理在途」
  if (!rime_api->get_property(session_id, "ai_predict/pending", buffer,
                              sizeof(buffer)))
    return false;
  return buffer[0] == '1';
}

void BangkeRimeHandler::_MaybeRequestAiPump(BangkeSessionId ipc_id,
                                            RimeSessionId session_id) {
  if (!ipc_id || !m_event_wnd || !_IsAiPending(session_id))
    return;
  // 不能就地重排：调用点都在按键管道路径上，此刻 Context 刚被这一趟 Compose 用过，
  // 就地再 Compose 一趟等于在 process_key 里重入。交给消息循环线程。
  PostMessageW(m_event_wnd, WM_BK_RIME_EVENT, BK_EVENT_AI_PUMP, (LPARAM)ipc_id);
}

void BangkeRimeHandler::AiPumpArm(BangkeSessionId ipc_id) {
  if (!m_event_wnd)
    return;
  // 这里绝不能去取 ApiMutex，哪怕只是读一眼会话表。
  //
  // 本函数跑在消息循环线程上，而面板窗口的线程也是它。管道线程是在持着 ApiMutex 的
  // 情况下调 SetWindowPos 挪面板的——那是一次同步的跨线程窗口调用，要等本线程把窗口
  // 消息处理完。本线程若在等 ApiMutex，两边就互等成永久死锁：服务端进程还在，管道却
  // 再也不回话，之后每条请求都堵在 API 串行权上。
  //
  // 泵状态本来就只归消息循环线程，不需要锁；会话有效性交给 AiPumpTick（它拿得到锁才
  // 校验，拿不到就下一拍再说）。
  //
  // 已经在为该会话跑就别再 SetTimer：同 id 再设一次是重置间隔，连打快过间隔时
  // 定时器永远等不到触发，AI 候选就再也浮不出来
  if (m_ai_pump_ipc_id == ipc_id && m_ai_pump_deadline)
    return;
  m_ai_pump_ipc_id = ipc_id;
  m_ai_pump_deadline = GetTickCount() + kAiPumpTimeoutMs;
  m_ai_pump_interval = kAiPumpIntervalMs;
  SetTimer(m_event_wnd, kAiPumpTimerId, m_ai_pump_interval, nullptr);
}

void BangkeRimeHandler::AiPumpTick() {
  // 只试不抢：本函数的调用者是消息循环线程，死等 ApiMutex 会和 AiPumpArm 里说的那个
  // 环撞上。泵本来就是轮询，让出一拍没有任何代价。
  std::unique_lock<std::recursive_mutex> lock(ApiMutex(), std::try_to_lock);
  if (!lock.owns_lock())
    return;
  if (!m_event_wnd || m_disabled) {
    _StopAiPump();
    return;
  }
  // KillTimer 不会清掉已经排进消息队列的 WM_TIMER，会话可能在这之前就没了；同样用
  // find，避免查询把幽灵会话坐实
  auto it = m_session_status_map.find(m_ai_pump_ipc_id);
  if (m_ai_pump_ipc_id == 0 || it == m_session_status_map.end() ||
      !it->second.session_id || static_cast<LONG>(GetTickCount() - m_ai_pump_deadline) >= 0) {
    _StopAiPump();
    return;
  }
  const RimeSessionId session_id = it->second.session_id;
  const char* input = rime_api->get_input(session_id);
  if (!input || !*input) {
    _StopAiPump();
    return;
  }
  // 任何 option 变更都会让 ConcreteEngine::OnOptionUpdate 在调用线程上跑
  // RefreshNonConfirmedComposition —— 正是要的那次重排，且在正确的线程上。
  // soft_cursor 是本代码本就持有的开关，重设成同值无副作用。
  rime_api->set_option(session_id, "soft_cursor",
                       rime_api->get_option(session_id, "soft_cursor"));
  // 这发 set_option 会触发 option 通知，OnNotify 把选项名写进 m_message_*。正常路径
  // 由 _UpdateUI 清掉，泵不走那条路，自己清——留着会让下一次按键的提示气泡显示成
  // soft_cursor，并顶掉本该走的那条显隐分支。
  {
    std::lock_guard<std::mutex> lock(m_notifier_mutex);
    m_message_type.clear();
    m_message_value.clear();
    m_message_label.clear();
    m_option_name.clear();
  }
  // 这一拍的重排可能刚好把结果算了出来（worker 在上一拍与这一拍之间落地），复查一次：
  // 落下了就立刻推快照收工，不必再等下一拍
  if (!_IsAiPending(session_id)) {
    _PushAiSnapshot(session_id);
    _StopAiPump();
  } else {
    // Compose can be expensive for long input. Back off 150 -> 300 -> 600 ms.
    m_ai_pump_interval = (std::min)(m_ai_pump_interval * 2, UINT(600));
    SetTimer(m_event_wnd, kAiPumpTimerId, m_ai_pump_interval, nullptr);
  }
}

void BangkeRimeHandler::_StopAiPump() {
  if (m_event_wnd)
    KillTimer(m_event_wnd, kAiPumpTimerId);
  m_ai_pump_ipc_id = 0;
  m_ai_pump_deadline = 0;
}

// 其它线程（会话销毁、焦点离开）不能直接 _StopAiPump：泵状态只归消息循环线程，
// 就地改会和 AiPumpArm 抢。交给消息循环线程去停。
void BangkeRimeHandler::_RequestStopAiPump(BangkeSessionId ipc_id) {
  if (!m_event_wnd)
    return;
  PostMessageW(m_event_wnd, WM_BK_RIME_EVENT, BK_EVENT_AI_PUMP_STOP,
               (LPARAM)ipc_id);
}

void BangkeRimeHandler::_ReadClientInfo(BangkeSessionId ipc_id,
                                            LPWSTR buffer) {
  std::string app_name;
  // parse request text
  wbufferstream bs(buffer, BANGKE_IPC_BUFFER_LENGTH);
  std::wstring line;
  while (bs.good()) {
    std::getline(bs, line);
    if (!bs.good())
      break;
    // file ends
    if (line == L".")
      break;
    const std::wstring kClientAppKey = L"session.client_app=";
    if (starts_with(line, kClientAppKey)) {
      std::wstring lwr = line;
      to_lower(lwr);
      app_name = wtou8(lwr.substr(kClientAppKey.length()));
    }
  }
  SessionStatus& session_status = get_session_status(ipc_id);
  session_status.client_app = app_name;
  RimeSessionId session_id = session_status.session_id;
  // set app specific options
  if (!app_name.empty()) {
    rime_api->set_property(session_id, "client_app", app_name.c_str());

    auto it = m_app_options.find(app_name);
    if (it != m_app_options.end()) {
      AppOptions& options(m_app_options[it->first]);
      for (const auto& pair : options) {
        DLOG(INFO) << "set app option: " << pair.first << " = " << pair.second;
        rime_api->set_option(session_id, pair.first.c_str(), Bool(pair.second));
      }
    }
  }
  // inline preedit
  bool inline_preedit = session_status.style.inline_preedit;
  rime_api->set_option(session_id, "inline_preedit", Bool(inline_preedit));
  // show soft cursor on the panel but not inline
  rime_api->set_option(session_id, "soft_cursor", Bool(!inline_preedit));
}

void BangkeRimeHandler::_GetCandidateInfo(CandidateInfo& cinfo,
                                              RimeContext& ctx) {
  cinfo.candies.resize(ctx.menu.num_candidates);
  cinfo.comments.resize(ctx.menu.num_candidates);
  cinfo.labels.resize(ctx.menu.num_candidates);
  for (int i = 0; i < ctx.menu.num_candidates; ++i) {
    cinfo.candies[i].str = escape_string(u8tow(ctx.menu.candidates[i].text));
    if (ctx.menu.candidates[i].comment) {
      cinfo.comments[i].str =
          escape_string(u8tow(ctx.menu.candidates[i].comment));
    }
    if (RIME_STRUCT_HAS_MEMBER(ctx, ctx.select_labels) && ctx.select_labels) {
      cinfo.labels[i].str = escape_string(u8tow(ctx.select_labels[i]));
    } else if (ctx.menu.select_keys) {
      cinfo.labels[i].str =
          escape_string(std::wstring(1, ctx.menu.select_keys[i]));
    } else {
      cinfo.labels[i].str = std::to_wstring((i + 1) % 10);
    }
  }
  cinfo.highlighted = ctx.menu.highlighted_candidate_index;
  cinfo.currentPage = ctx.menu.page_no;
  cinfo.is_last_page = ctx.menu.is_last_page;
}

void BangkeRimeHandler::ReloadStyle() {
  // 样式热重载:重读 weasel 配置并推给全部会话 —— 不销毁引擎、不跑
  // schema/词典任务、不重载 AI 模型。配置的合并(deploy_config_file)
  // 由调用方在发命令前完成 —— 本函数假定 build 产物已是最新
  UpdateColorTheme(m_current_dark_mode);
}

void BangkeRimeHandler::StartMaintenance() {
  // 部署要销毁 rime 会话,但客户端仍握着旧 ipc_id;不快照身份的话,
  // AI 推送(按 rime_sid 查表)会哑火到应用重抢焦点
  m_pending_restore.clear();
  for (const auto& pair : m_session_status_map) {
    if (!pair.second.client_app.empty())
      m_pending_restore.emplace(pair.first, pair.second.client_app);
  }
  m_session_status_map.clear();
  Finalize();
  _UpdateUI(0);
}

void BangkeRimeHandler::EndMaintenance() {
  if (m_disabled) {
    _Initialize(true);
    _UpdateUI(0);
  }
  m_session_status_map.clear();
  // 按快照重建:客户端 ipc_id 不变,推送槽(s_snap_handles)未随部署销毁,
  // 恢复后 AI 推送无缝续上
  if (!m_pending_restore.empty()) {
    const auto pending = std::move(m_pending_restore);
    for (const auto& pair : pending)
      _RestoreSession(pair.first, pair.second);
  }
}

void BangkeRimeHandler::_RestoreSession(BangkeSessionId ipc_id,
                                            const std::string& client_app) {
  if (m_disabled)
    return;
  RimeSessionId session_id = (RimeSessionId)rime_api->create_session();
  if (!session_id)
    return;
  SessionStatus& session_status = new_session_status(ipc_id);
  session_status.style = m_base_style;
  session_status.session_id = session_id;
  session_status.client_app = client_app;
  if (!client_app.empty()) {
    rime_api->set_property(session_id, "client_app", client_app.c_str());
    auto it = m_app_options.find(client_app);
    if (it != m_app_options.end()) {
      AppOptions& options(m_app_options[it->first]);
      for (const auto& pair : options)
        rime_api->set_option(session_id, pair.first.c_str(), Bool(pair.second));
    }
  }
  rime_api->set_option(session_id, "inline_preedit",
                       Bool(m_base_style.inline_preedit));
  rime_api->set_option(session_id, "soft_cursor",
                       Bool(!m_base_style.inline_preedit));

  RIME_STRUCT(RimeStatus, status);
  if (rime_api->get_status(session_id, &status)) {
    m_last_schema_id = status.schema_id;
    _LoadSchemaSpecificSettings(ipc_id, status.schema_id);
    _LoadAppInlinePreeditSet(ipc_id, true);
    _UpdateInlinePreeditStatus(ipc_id);
    session_status.status = status;
    session_status.__synced = false;
    rime_api->free_status(&status);
  }
}

void BangkeRimeHandler::SetOption(BangkeSessionId ipc_id,
                                      const std::string& opt,
                                      bool val) {
  // from no-session client, not actual typing session
  if (!ipc_id) {
    if (m_global_ascii_mode && opt == "ascii_mode") {
      for (auto& pair : m_session_status_map)
        rime_api->set_option(to_session_id(pair.first), "ascii_mode", val);
    } else {
      rime_api->set_option(to_session_id(m_active_session), opt.c_str(), val);
    }
  } else {
    rime_api->set_option(to_session_id(ipc_id), opt.c_str(), val);
  }
  // refresh UI (and tray icon) so the option change takes effect immediately,
  // e.g. when toggling ascii_mode from the TSF language bar
  _UpdateUI(ipc_id ? ipc_id : m_active_session);
}

void BangkeRimeHandler::OnUpdateUI(std::function<void()> const& cb) {
  _UpdateUICallback = cb;
}

bool BangkeRimeHandler::_IsDeployerRunning() {
  HANDLE hMutex = CreateMutex(NULL, TRUE, L"BangkeDeployerMutex");
  bool deployer_detected = hMutex && GetLastError() == ERROR_ALREADY_EXISTS;
  if (hMutex) {
    CloseHandle(hMutex);
  }
  return deployer_detected;
}

void BangkeRimeHandler::_UpdateUI(BangkeSessionId ipc_id) {
  // if m_ui nullptr, _UpdateUI meaningless
  if (!m_ui)
    return;

  Status& bangke_status = m_ui->status();
  Context bangke_context;

  RimeSessionId session_id = to_session_id(ipc_id);

  if (ipc_id == 0)
    bangke_status.disabled = m_disabled;

  _GetStatus(bangke_status, ipc_id, bangke_context);

  SessionStatus& session_status = get_session_status(ipc_id);
  if (rime_api->get_option(session_id, "inline_preedit"))
    session_status.style.client_caps |= INLINE_PREEDIT_CAPABLE;
  else
    session_status.style.client_caps &= ~INLINE_PREEDIT_CAPABLE;

  if (!_ShowMessage(bangke_context, bangke_status)) {
    m_ui->Hide();
    m_ui->Update(bangke_context, bangke_status);
  }

  _RefreshTrayIcon(session_id, _UpdateUICallback);

  {
    std::lock_guard<std::mutex> lock(m_notifier_mutex);
    m_message_type.clear();
    m_message_value.clear();
    m_message_label.clear();
    m_option_name.clear();
  }

  // 放在最后：泵要重排一遍菜单，让这一步的显示先落定
  _MaybeRequestAiPump(ipc_id, session_id);
}

void BangkeRimeHandler::_LoadSchemaSpecificSettings(
    BangkeSessionId ipc_id,
    const std::string& schema_id) {
  if (!m_ui)
    return;
  RimeConfig config;
  if (!rime_api->schema_open(schema_id.c_str(), &config))
    return;
  SessionStatus& session_status = get_session_status(ipc_id);
  // 取生效配置里的值(含该方案的 custom 补丁),没有这个键就是关
  Bool ai_only = false;
  session_status.ai_only =
      rime_api->config_get_bool(&config, "ai_predict/ai_only", &ai_only) &&
      !!ai_only;
  _UpdateShowNotifications(&config);
  m_ui->style() = m_base_style;
  _UpdateUIStyle(&config, m_ui, false);
  session_status.style = m_ui->style();
  // 纯推理模式只出一条不带序号的预测,序号留着是噪音。序号的存在与否由
  // label_font_point 门控(见 Layout.cpp 的 labelFontValid):置 0 后标签矩形宽度塌成 0,
  // 布局不再为它留位、绘制对空矩形直接返回。改的是本会话的 style 副本,不波及别的会话
  if (session_status.ai_only)
    session_status.style.label_font_point = 0;
  UIStyle& style = session_status.style;
  // load schema color style config
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  const auto update_color_scheme = [&](bool dark) {
    std::string color_name(buffer);
    RimeConfigIterator preset = {0};
    if (rime_api->config_begin_map(
            &preset, &config, ("preset_color_schemes/" + color_name).c_str())) {
      _UpdateUIStyleColor(&config, style, color_name, dark);
      rime_api->config_end(&preset);
    } else {
      RimeConfig bangkeconfig;
      if (rime_api->config_open("weasel", &bangkeconfig)) {
        _UpdateUIStyleColor(&bangkeconfig, style, color_name, dark);
        rime_api->config_close(&bangkeconfig);
      }
    }
  };
  const char* key =
      m_current_dark_mode ? "style/color_scheme_dark" : "style/color_scheme";
  bool has_scheme =
      rime_api->config_get_string(&config, key, buffer, BUF_SIZE);
  if (!has_scheme && m_current_dark_mode)
    // 深色未配置时跟随浅色键;native 会在 _UpdateUIStyleColor 内按外观解析
    has_scheme =
        rime_api->config_get_string(&config, "style/color_scheme", buffer,
                                    BUF_SIZE);
  if (has_scheme)
    update_color_scheme(m_current_dark_mode);
  // load schema icon start
  {
    const auto load_icon = [](RimeConfig& config, const char* key1,
                              const char* key2) {
      const auto user_dir = BangkeUserDataPath();
      const auto shared_dir = BangkeSharedDataPath();
      const int BUF_SIZE = 255;
      char buffer[BUF_SIZE + 1] = {0};
      if (rime_api->config_get_string(&config, key1, buffer, BUF_SIZE) ||
          (key2 != NULL &&
           rime_api->config_get_string(&config, key2, buffer, BUF_SIZE))) {
        auto resource = u8tow(buffer);
        if (fs::is_regular_file(user_dir / resource))
          return (user_dir / resource).wstring();
        else if (fs::is_regular_file(shared_dir / resource))
          return (shared_dir / resource).wstring();
      }
      return std::wstring();
    };
    style.current_zhung_icon =
        load_icon(config, "schema/icon", "schema/zhung_icon");
    style.current_ascii_icon = load_icon(config, "schema/ascii_icon", NULL);
    style.current_full_icon = load_icon(config, "schema/full_icon", NULL);
    style.current_half_icon = load_icon(config, "schema/half_icon", NULL);
  }
  // load schema icon end
  rime_api->config_close(&config);
}

void BangkeRimeHandler::_LoadAppInlinePreeditSet(BangkeSessionId ipc_id,
                                                     bool ignore_app_name) {
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  static char _app_name[50];
  rime_api->get_property(session_id, "client_app", _app_name,
                         sizeof(_app_name) - 1);
  std::string app_name(_app_name);
  if (!ignore_app_name && m_last_app_name == app_name)
    return;
  m_last_app_name = app_name;
  bool inline_preedit = session_status.style.inline_preedit;
  bool found = false;
  if (!app_name.empty()) {
    auto it = m_app_options.find(app_name);
    if (it != m_app_options.end()) {
      AppOptions& options(m_app_options[it->first]);
      for (const auto& pair : options) {
        if (pair.first == "inline_preedit") {
          rime_api->set_option(session_id, pair.first.c_str(),
                               Bool(pair.second));
          session_status.style.inline_preedit = Bool(pair.second);
          found = true;
          break;
        }
      }
    }
  }
  if (!found) {
    session_status.style.inline_preedit = m_base_style.inline_preedit;
    // load from schema.
    RIME_STRUCT(RimeStatus, status);
    if (rime_api->get_status(session_id, &status)) {
      std::string schema_id = status.schema_id;
      RimeConfig config;
      if (rime_api->schema_open(schema_id.c_str(), &config)) {
        Bool value = False;
        if (rime_api->config_get_bool(&config, "style/inline_preedit",
                                      &value)) {
          session_status.style.inline_preedit = value;
        }
        rime_api->config_close(&config);
      }
      rime_api->free_status(&status);
    }
  }
  if (session_status.style.inline_preedit != inline_preedit)
    _UpdateInlinePreeditStatus(ipc_id);
}

bool BangkeRimeHandler::_ShowMessage(Context& ctx, Status& status) {
  std::lock_guard<std::mutex> lock(m_notifier_mutex);
  if (m_message_type.empty() || m_message_value.empty())
    return m_ui->IsCountingDown();
  // show as auxiliary string
  std::wstring& tips(ctx.aux.str);
  bool show_icon = false;
  if (m_message_type == "deploy") {
    if (m_message_value == "start")
      if (GetThreadUILanguage() == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
        tips = L"Deploying RIME";
      else
        tips = L"正在部署 RIME";
    else if (m_message_value == "success")
      if (GetThreadUILanguage() == MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US))
        tips = L"Deployed";
      else
        tips = L"部署完成";
    else if (m_message_value == "failure") {
      if (GetThreadUILanguage() ==
          MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL))
        tips = L"有錯誤，請查看日誌 %TEMP%\\rime.bangke\\rime.bangke.*.INFO";
      else if (GetThreadUILanguage() ==
               MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED))
        tips = L"有错误，请查看日志 %TEMP%\\rime.bangke\\rime.bangke.*.INFO";
      else
        tips =
            L"There is an error, please check the logs "
            L"%TEMP%\\rime.bangke\\rime.bangke.*.INFO";
    }
  } else if (m_message_type == "schema") {
    tips = /*L"【" + */ status.schema_name /* + L"】"*/;
  } else if (m_message_type == "option") {
    status.type = SCHEMA;
    if (m_message_value == "!ascii_mode") {
      show_icon = true;
    } else if (m_message_value == "ascii_mode") {
      show_icon = true;
    } else
      tips = u8tow(m_message_label);

    if (m_message_value == "full_shape" || m_message_value == "!full_shape")
      status.type = FULL_SHAPE;
  }
  auto counter = m_ui->IsCountingDown();
  if (!show_icon && counter)
    return counter;
  auto foption = m_show_notifications.find(m_option_name);
  auto falways = m_show_notifications.find("always");
  // deploy 提示不再旁路闸门:部署反馈由设置程序的保存 toast 承担,
  // 维护握手(如词典页进出)触发的"部署完成"气泡属于噪音
  if (!add_session && (foption != m_show_notifications.end() ||
                       falways != m_show_notifications.end())) {
    m_ui->Update(ctx, status);
    if (m_show_notifications_time)
      m_ui->ShowWithTimeout(m_show_notifications_time);
    return true;
  } else {
    return m_ui->IsCountingDown();
  }
}
inline std::string _GetLabelText(const std::vector<Text>& labels,
                                 int id,
                                 const wchar_t* format) {
  wchar_t buffer[128];
  swprintf_s<128>(buffer, format, labels.at(id).str.c_str());
  return wtou8(std::wstring(buffer));
}

bool BangkeRimeHandler::_Respond(BangkeSessionId ipc_id, EatLine eat,
                                     bool include_commit) {
  return _RespondFrame(ipc_id, eat, include_commit);
}

// proto=2 渲染:与 _Respond 相同的数据收集与副作用,输出二进制帧。
// 帧字节偶数补零后按 wchar 重解释经 eat 传输——管道体本就是不透明字节块,
// ③ 的客户端按字节取回。key_serial 暂为 0,由后续步骤引入单调序号。
bool BangkeRimeHandler::_RespondFrame(BangkeSessionId ipc_id,
                                          EatLine eat,
                                          bool include_commit,
                                          bool consume_style_sync) {
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;

  std::wstring commit;
  bool has_commit = false;
  RIME_STRUCT(RimeCommit, rime_commit);
  if (include_commit && rime_api->get_commit(session_id, &rime_commit)) {
    commit = u8tow(rime_commit.text);
    has_commit = true;
    rime_api->free_commit(&rime_commit);
  }

  bangke::Status status;
  RIME_STRUCT(RimeStatus, rime_status);
  if (rime_api->get_status(session_id, &rime_status)) {
    status.schema_name = rime_status.schema_name ? u8tow(rime_status.schema_name)
                                                 : std::wstring();
    status.schema_id = rime_status.schema_id ? u8tow(rime_status.schema_id)
                                             : std::wstring();
    status.ascii_mode = !!rime_status.is_ascii_mode;
    status.composing = !!rime_status.is_composing;
    status.disabled = !!rime_status.is_disabled;
    status.full_shape = !!rime_status.is_full_shape;
    if (m_global_ascii_mode &&
        (session_status.status.is_ascii_mode != rime_status.is_ascii_mode)) {
      for (auto& pair : m_session_status_map) {
        if (pair.first != ipc_id)
          rime_api->set_option(to_session_id(pair.first), "ascii_mode",
                               !!rime_status.is_ascii_mode);
      }
    }
    session_status.status = rime_status;
    rime_api->free_status(&rime_status);
  }

  bangke::Context ctx;
  bool has_ctx = false;
  RIME_STRUCT(RimeContext, rime_ctx);
  if (rime_api->get_context(session_id, &rime_ctx)) {
    has_ctx = true;
    const bool has_candidates = rime_ctx.menu.num_candidates > 0;
    if (has_candidates)
      _GetCandidateInfo(ctx.cinfo, rime_ctx);
    // 记下这一帧的候选数：纯推理模式下菜单空着等于「模型还没答」，
    // 空格据此决定吞不吞（见 ProcessKeyEvent）
    session_status.menu_candidates = ctx.cinfo.candies.size();

    if (rime_ctx.composition.length > 0) {
      const char* preedit = rime_ctx.composition.preedit;
      const int start = rime_ctx.composition.sel_start;
      const int end = rime_ctx.composition.sel_end;
      const int cursor = rime_ctx.composition.cursor_pos;
      static const auto u8pos = [](const char* u8str, int wlen) {
        return utf8towcslen(u8str, wlen);
      };
      switch (session_status.style.preedit_type) {
        case UIStyle::PREVIEW: {
          if (rime_ctx.commit_text_preview) {
            ctx.preedit.str = u8tow(rime_ctx.commit_text_preview);
            const int len = u8pos(rime_ctx.commit_text_preview, 0);
            ctx.preedit.attributes.push_back(
                bangke::TextAttribute(0, len, bangke::HIGHLIGHTED));
            ctx.preedit.attributes.back().range.cursor = len;
            break;
          }
          [[fallthrough]];
        }
        case UIStyle::COMPOSITION: {
          ctx.preedit.str = u8tow(preedit);
          if (start <= end) {
            ctx.preedit.attributes.push_back(bangke::TextAttribute(
                u8pos(preedit, start), u8pos(preedit, end),
                bangke::HIGHLIGHTED));
            ctx.preedit.attributes.back().range.cursor = u8pos(preedit, cursor);
          }
          break;
        }
        case UIStyle::PREVIEW_ALL: {
          // 与文本协议相同的内联拼装:标记 + 标签 + 候选 + 注释
          auto label_valid = session_status.style.label_font_point > 0;
          auto comment_valid = session_status.style.comment_font_point > 0;
          const std::wstring mark =
              session_status.style.mark_text.empty()
                  ? std::wstring(L"*")
                  : session_status.style.mark_text;
          std::wstring& s = ctx.preedit.str;
          s = u8tow(preedit) + L"  [";
          for (auto i = 0; i < rime_ctx.menu.num_candidates; i++) {
            std::wstring label;
            if (label_valid) {
              wchar_t buf[128];
              swprintf_s<128>(buf, session_status.style.label_text_format.c_str(),
                              ctx.cinfo.labels.at(i).str.c_str());
              label = buf;
            }
            const std::wstring comment =
                comment_valid ? ctx.cinfo.comments.at(i).str : std::wstring();
            const std::wstring prefix =
                (i != rime_ctx.menu.highlighted_candidate_index)
                    ? std::wstring()
                    : mark;
            s += L" " + prefix + label + u8tow(rime_ctx.menu.candidates[i].text) +
                 L" " + comment;
          }
          s += L" ]";
          if (start <= end) {
            ctx.preedit.attributes.push_back(bangke::TextAttribute(
                u8pos(preedit, start), u8pos(preedit, end),
                bangke::HIGHLIGHTED));
            ctx.preedit.attributes.back().range.cursor = u8pos(preedit, cursor);
          }
          break;
        }
      }
    }
    rime_api->free_context(&rime_ctx);
  }

  bangke::Config config;
  config.inline_preedit = session_status.style.inline_preedit;

  const bool send_style = !session_status.__synced;

  auto frame =
      bangke::BuildFrame(ipc_id, s_key_serial.load(),
                         has_commit ? &commit : nullptr, has_ctx ? &ctx : nullptr,
                         &status, &config,
                         send_style ? &session_status.style : nullptr);
  if (send_style && consume_style_sync)
    session_status.__synced = true;
  if (frame.size() % 2)
    frame.push_back(0);
  std::wstring wire(reinterpret_cast<const wchar_t*>(frame.data()),
                    frame.size() / 2);
  return eat(wire);
}

// Blend foreground and background ARGB colors taking alpha into account.
// Returns an ABGR COLORREF with premultiplied alpha blended result.
static inline COLORREF blend_colors(COLORREF fcolor, COLORREF bcolor) {
  // Extract ARGB channels from both colors.
  BYTE fA = (fcolor >> 24) & 0xFF;
  BYTE fB = (fcolor >> 16) & 0xFF;
  BYTE fG = (fcolor >> 8) & 0xFF;
  BYTE fR = fcolor & 0xFF;
  BYTE bA = (bcolor >> 24) & 0xFF;
  BYTE bB = (bcolor >> 16) & 0xFF;
  BYTE bG = (bcolor >> 8) & 0xFF;
  BYTE bR = bcolor & 0xFF;
  // Convert alpha to [0,1]
  float fAlpha = fA / 255.0f;
  float bAlpha = bA / 255.0f;
  // Result alpha
  float retAlpha = fAlpha + (1 - fAlpha) * bAlpha;
  if (retAlpha <= 1e-6f) {
    // Fully transparent result — return background unchanged as fallback.
    return bcolor;
  }
  auto mix = [&](float fc, float bc) -> BYTE {
    return static_cast<BYTE>((fc * fAlpha + bc * bAlpha * (1 - fAlpha)) /
                             retAlpha);
  };
  BYTE retR = mix(fR, bR);
  BYTE retG = mix(fG, bG);
  BYTE retB = mix(fB, bB);
  BYTE outA = static_cast<BYTE>(retAlpha * 255.0f);
  return (static_cast<COLORREF>(outA) << 24) | (retB << 16) | (retG << 8) |
         retR;
}
// parse color value, with fallback value
static Bool _RimeGetColor(RimeConfig* config,
                          const std::string& key,
                          int& value,
                          const ColorFormat& fmt,
                          const unsigned int& fallback) {
  char color[256] = {0};
  if (!rime_api->config_get_string(config, key.c_str(), color, 256)) {
    value = fallback;
    return False;
  }
  const auto color_str = std::string(color);
  // adjudge if str is 0x 0X # hex color format, return trimmed hex part
  // out part is 6 or 8 length hex string without white space
  const auto parse_color_code = [](const std::string& str, std::string& out) {
    if (str.empty())
      return false;
    size_t start = 0;
    if (str[0] == '#') {
      start = 1;
    } else if (str.size() >= 2 &&
               (str.compare(0, 2, "0x") == 0 || str.compare(0, 2, "0X") == 0)) {
      start = 2;
    } else {
      return false;
    }
    const std::string hex_part = str.substr(start);
    if (hex_part.empty())
      return false;
    if ((start == 1 || start == 2) && hex_part.length() != 3 &&
        hex_part.length() != 4 && hex_part.length() != 6 &&
        hex_part.length() != 8) {
      return false;
    }
    for (char c : hex_part) {
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F')))
        return false;
    }
    out = str.substr(start).substr(0, 8);
#define _2C(c) std::string(2, c)
    if (out.size() == 3)
      out = _2C(out[0]) + _2C(out[1]) + _2C(out[2]);
    else if (out.size() == 4)
      out = _2C(out[0]) + _2C(out[1]) + _2C(out[2]) + _2C(out[3]);
#undef _2C
    return true;
  };
  auto hex_color = std::string();
  if (parse_color_code(color_str, hex_color)) {
    value = std::stoul(hex_color, 0, 16);
    if (hex_color.length() == 6)
      value = (fmt != COLOR_RGBA) ? (value | 0xff000000)
                                  : (((unsigned int)value << 8) | 0x000000ff);
  } else {
    if (!rime_api->config_get_int(config, key.c_str(), &value)) {
      value = fallback;
      return False;
    }
    if (value <= 0xffffff)
      value = (fmt != COLOR_RGBA) ? (value | 0xff000000)
                                  : (((unsigned int)value << 8) | 0x000000ff);
    else if (value > 0xffffffff)
      value &= 0xffffffff;
  }
  if (fmt == COLOR_ARGB)
    value = ARGB2ABGR(value);
  else if (fmt == COLOR_RGBA)
    value = RGBA2ABGR(value);
  value &= 0xffffffff;
  return True;
}

template <typename T, size_t N>
using Array = std::array<std::pair<const char*, T>, N>;

// parset bool type configuration to T type value trueValue / falseValue
template <typename T>
void _RimeGetBool(RimeConfig* config,
                  const char* key,
                  bool cond,
                  T& value,
                  const T& trueValue = true,
                  const T& falseValue = false) {
  Bool tempb = False;
  if (rime_api->config_get_bool(config, key, &tempb) || cond)
    value = (!!tempb) ? trueValue : falseValue;
}
// parse string option to T type value, with fallback
template <typename T, size_t N>
void _RimeParseStringOptWithFallback(RimeConfig* config,
                                     const char* key,
                                     T& value,
                                     const Array<T, N>& arr,
                                     const T& fallback) {
  char str_buff[256] = {0};
  if (rime_api->config_get_string(config, key, str_buff, 255)) {
    for (size_t i = 0; i < N; ++i) {
      if (strcmp(arr[i].first, str_buff) == 0) {
        value = arr[i].second;
        return;
      }
    }
  }
  value = fallback;
}

template <typename T>
void _RimeGetIntStr(RimeConfig* config,
                    const char* key,
                    T& value,
                    const char* fb_key = nullptr,
                    const void* fb_value = nullptr,
                    const std::function<void(T&)>& func = nullptr) {
  if constexpr (std::is_same<T, int>::value) {
    if (!rime_api->config_get_int(config, key, &value) && fb_key != 0)
      rime_api->config_get_int(config, fb_key, &value);
  } else if constexpr (std::is_same<T, std::wstring>::value) {
    const int BUF_SIZE = 2047;
    char buffer[BUF_SIZE + 1] = {0};
    if (rime_api->config_get_string(config, key, buffer, BUF_SIZE) ||
        rime_api->config_get_string(config, fb_key, buffer, BUF_SIZE)) {
      value = u8tow(buffer);
    } else if (fb_value) {
      value = *(T*)fb_value;
    }
  }
  if (func)
    func(value);
}

// Helper to iterate a Rime map and invoke callback with key/path
static void ForEachRimeMap(
    RimeConfig* config,
    const std::string& path,
    const std::function<void(const char* key, const char* child_path)>& cb) {
  RimeConfigIterator iter;
  if (!rime_api->config_begin_map(&iter, config, path.c_str()))
    return;
  while (rime_api->config_next(&iter)) {
    cb(iter.key, iter.path);
  }
  rime_api->config_end(&iter);
}

// Helper to iterate a Rime list and invoke callback with item path
static void ForEachRimeList(
    RimeConfig* config,
    const std::string& path,
    const std::function<void(const char* item_path)>& cb) {
  RimeConfigIterator iter;
  if (!rime_api->config_begin_list(&iter, config, path.c_str()))
    return;
  while (rime_api->config_next(&iter)) {
    cb(iter.path);
  }
  rime_api->config_end(&iter);
}

void BangkeRimeHandler::_UpdateShowNotifications(RimeConfig* config,
                                                     bool initialize) {
  Bool show_notifications = true;
  if (initialize)
    m_show_notifications_base.clear();
  m_show_notifications.clear();

  if (rime_api->config_get_bool(config, "show_notifications",
                                &show_notifications)) {
    // config read as bool, for global all on or off
    if (show_notifications)
      m_show_notifications["always"] = true;
    if (initialize)
      m_show_notifications_base = m_show_notifications;
  } else {
    // read as list using helper
    ForEachRimeList(config, "show_notifications", [&](const char* item_path) {
      char buffer[256] = {0};
      if (rime_api->config_get_string(config, item_path, buffer, 256))
        m_show_notifications[std::string(buffer)] = true;
    });
    if (initialize)
      m_show_notifications_base = m_show_notifications;
    if (m_show_notifications.empty()) {
      // not configured, or incorrect type
      if (initialize)
        m_show_notifications_base["always"] = true;
      m_show_notifications = m_show_notifications_base;
    }
  }
}

// update ui's style parameters, ui has been check before referenced
static void _UpdateUIStyle(RimeConfig* config, UI* ui, bool initialize) {
  UIStyle& style(ui->style());
  const std::function<void(std::wstring&)> rmspace = [](std::wstring& str) {
    str = std::regex_replace(str, std::wregex(L"\\s*(,|:|^|$)\\s*"), L"$1");
  };
  const std::function<void(int&)> _abs = [](int& value) { value = abs(value); };
  // get font faces
  _RimeGetIntStr(config, "style/font_face", style.font_face, 0, 0, rmspace);
  std::wstring* const pFallbackFontFace = initialize ? &style.font_face : NULL;
  _RimeGetIntStr(config, "style/label_font_face", style.label_font_face, 0,
                 pFallbackFontFace, rmspace);
  _RimeGetIntStr(config, "style/comment_font_face", style.comment_font_face, 0,
                 pFallbackFontFace, rmspace);
  // able to set label font/comment font empty, force fallback to font face.
  if (style.label_font_face.empty())
    style.label_font_face = style.font_face;
  if (style.comment_font_face.empty())
    style.comment_font_face = style.font_face;
  // get font points
  _RimeGetIntStr(config, "style/font_point", style.font_point);
  if (style.font_point <= 0)
    style.font_point = 12;
  _RimeGetIntStr(config, "style/label_font_point", style.label_font_point,
                 "style/font_point", 0, _abs);
  _RimeGetIntStr(config, "style/comment_font_point", style.comment_font_point,
                 "style/font_point", 0, _abs);
  _RimeGetIntStr(config, "style/candidate_abbreviate_length",
                 style.candidate_abbreviate_length, 0, 0, _abs);
  _RimeGetBool(config, "style/inline_preedit", initialize,
               style.inline_preedit);
  _RimeGetBool(config, "style/vertical_auto_reverse", initialize,
               style.vertical_auto_reverse);
  static constexpr Array<UIStyle::PreeditType, 3> _preeditArr = {
      {{"composition", UIStyle::COMPOSITION},
       {"preview", UIStyle::PREVIEW},
       {"preview_all", UIStyle::PREVIEW_ALL}}};
  _RimeParseStringOptWithFallback(config, "style/preedit_type",
                                  style.preedit_type, _preeditArr,
                                  style.preedit_type);
  static constexpr Array<UIStyle::AntiAliasMode, 5> _aliasModeArr = {
      {{"force_dword", UIStyle::FORCE_DWORD},
       {"cleartype", UIStyle::CLEARTYPE},
       {"grayscale", UIStyle::GRAYSCALE},
       {"aliased", UIStyle::ALIASED},
       {"default", UIStyle::DEFAULT}}};
  _RimeParseStringOptWithFallback(config, "style/antialias_mode",
                                  style.antialias_mode, _aliasModeArr,
                                  style.antialias_mode);
  static constexpr Array<UIStyle::HoverType, 3> _hoverTypeArr = {
      {{"none", UIStyle::HoverType::NONE},
       {"semi_hilite", UIStyle::HoverType::SEMI_HILITE},
       {"hilite", UIStyle::HoverType::HILITE}}};
  _RimeParseStringOptWithFallback(config, "style/hover_type", style.hover_type,
                                  _hoverTypeArr, style.hover_type);
  static constexpr Array<UIStyle::LayoutAlignType, 3> _alignType = {
      {{"top", UIStyle::ALIGN_TOP},
       {"center", UIStyle::ALIGN_CENTER},
       {"bottom", UIStyle::ALIGN_BOTTOM}}};
  _RimeParseStringOptWithFallback(config, "style/layout/align_type",
                                  style.align_type, _alignType,
                                  style.align_type);
  _RimeGetBool(config, "style/display_tray_icon", initialize,
               style.display_tray_icon);
  _RimeGetBool(config, "style/ascii_tip_follow_cursor", initialize,
               style.ascii_tip_follow_cursor);
  _RimeGetBool(config, "style/horizontal", initialize, style.layout_type,
               UIStyle::LAYOUT_HORIZONTAL, UIStyle::LAYOUT_VERTICAL);
  _RimeGetBool(config, "style/paging_on_scroll", initialize,
               style.paging_on_scroll);
  _RimeGetBool(config, "style/vertical_text", false, style.layout_type,
               UIStyle::LAYOUT_VERTICAL_TEXT, style.layout_type);
  _RimeGetBool(config, "style/vertical_text_left_to_right", false,
               style.vertical_text_left_to_right);
  _RimeGetBool(config, "style/vertical_text_with_wrap", false,
               style.vertical_text_with_wrap);
  static constexpr Array<bool, 2> _text_orientation = {
      {{"horizontal", false}, {"vertical", true}}};
  bool _text_orientation_bool = false;
  _RimeParseStringOptWithFallback(config, "style/text_orientation",
                                  _text_orientation_bool, _text_orientation,
                                  _text_orientation_bool);
  if (_text_orientation_bool)
    style.layout_type = UIStyle::LAYOUT_VERTICAL_TEXT;
  _RimeGetIntStr(config, "style/label_format", style.label_text_format);
  _RimeGetIntStr(config, "style/mark_text", style.mark_text);
  _RimeGetIntStr(config, "style/layout/baseline", style.baseline, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/linespacing", style.linespacing, 0, 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/min_width", style.min_width, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/max_width", style.max_width, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/min_height", style.min_height, 0, 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/max_height", style.max_height, 0, 0,
                 _abs);
  // layout (alternative to style/horizontal)
  static constexpr Array<UIStyle::LayoutType, 3> _layoutArr = {
      {{"vertical", UIStyle::LAYOUT_VERTICAL},
       {"horizontal", UIStyle::LAYOUT_HORIZONTAL},
       {"vertical_text", UIStyle::LAYOUT_VERTICAL_TEXT}}};
  _RimeParseStringOptWithFallback(config, "style/layout/type",
                                  style.layout_type, _layoutArr,
                                  style.layout_type);
  _RimeGetIntStr(config, "style/layout/border", style.border,
                 "style/layout/border_width", 0, _abs);
  _RimeGetIntStr(config, "style/layout/margin_x", style.margin_x);
  _RimeGetIntStr(config, "style/layout/margin_y", style.margin_y);
  _RimeGetIntStr(config, "style/layout/spacing", style.spacing, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/candidate_spacing",
                 style.candidate_spacing, 0, 0, _abs);
  _RimeGetIntStr(config, "style/layout/hilite_spacing", style.hilite_spacing, 0,
                 0, _abs);
  _RimeGetIntStr(config, "style/layout/hilite_padding_x",
                 style.hilite_padding_x, "style/layout/hilite_padding", 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/hilite_padding_y",
                 style.hilite_padding_y, "style/layout/hilite_padding", 0,
                 _abs);
  _RimeGetIntStr(config, "style/layout/shadow_radius", style.shadow_radius, 0,
                 0, _abs);
  _RimeGetIntStr(config, "style/layout/shadow_offset_x", style.shadow_offset_x);
  _RimeGetIntStr(config, "style/layout/shadow_offset_y", style.shadow_offset_y);
  // round_corner as alias of hilited_corner_radius
  _RimeGetIntStr(config, "style/layout/hilited_corner_radius",
                 style.round_corner, "style/layout/round_corner", 0, _abs);
  // corner_radius not set, fallback to round_corner
  _RimeGetIntStr(config, "style/layout/corner_radius", style.round_corner_ex,
                 "style/layout/round_corner", 0, _abs);
  // fix padding and spacing settings
  if (style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT) {
    // hilite_padding vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = max(style.spacing, style.hilite_padding_y * 2);
    // hilite_padding vs candidate_spacing
    if (style.layout_type == UIStyle::LAYOUT_VERTICAL) {
      // vertical, if hilite_padding_y over candidate spacing,
      // increase candidate spacing
      style.candidate_spacing =
          max(style.candidate_spacing, style.hilite_padding_y * 2);
    } else {
      // horizontal, if hilite_padding_x over candidate
      // spacing, increase candidate spacing
      style.candidate_spacing =
          max(style.candidate_spacing, style.hilite_padding_x * 2);
    }
    // hilite_padding_x vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing = max(style.hilite_spacing, style.hilite_padding_x);
  } else  // LAYOUT_VERTICAL_TEXT
  {
    // hilite_padding_x vs spacing
    // if hilite_padding over spacing, increase spacing
    style.spacing = max(style.spacing, style.hilite_padding_x * 2);
    // hilite_padding vs candidate_spacing
    // if hilite_padding_x over candidate
    // spacing, increase candidate spacing
    style.candidate_spacing =
        max(style.candidate_spacing, style.hilite_padding_x * 2);
    // vertical_text_with_wrap and hilite_padding_y over candidate_spacing
    if (style.vertical_text_with_wrap)
      style.candidate_spacing =
          max(style.candidate_spacing, style.hilite_padding_y * 2);
    // hilite_padding_y vs hilite_spacing
    if (!style.inline_preedit)
      style.hilite_spacing = max(style.hilite_spacing, style.hilite_padding_y);
  }
  // fix padding and margin settings
  int scale = style.margin_x < 0 ? -1 : 1;
  style.margin_x = scale * max(style.hilite_padding_x, abs(style.margin_x));
  scale = style.margin_y < 0 ? -1 : 1;
  style.margin_y = scale * max(style.hilite_padding_y, abs(style.margin_y));
  // get enhanced_position
  _RimeGetBool(config, "style/enhanced_position", initialize,
               style.enhanced_position, true, false);
  // get color scheme
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  if (initialize && rime_api->config_get_string(config, "style/color_scheme",
                                                buffer, BUF_SIZE))
    _UpdateUIStyleColor(config, style);
}
// macOS 专有字体名 → Windows 可用替代;逐分量替换,保住回退链结构
static std::wstring _AliasFontFace(const std::wstring& face) {
  static const std::pair<std::wstring, std::wstring> aliases[] = {
      {L"STHeitiSC-Light", L"Microsoft YaHei UI:light"},
      {L"STHeitiSC", L"Microsoft YaHei UI"},
      {L"PingFang SC", L"Microsoft YaHei UI"},
      {L"Hiragino Sans GB", L"Microsoft YaHei UI"},
      {L"Avenir", L"Segoe UI"},
  };
  std::wstring result;
  size_t pos = 0;
  while (pos < face.size()) {
    const size_t comma = face.find(L',', pos);
    std::wstring token =
        face.substr(pos, comma == std::wstring::npos ? std::wstring::npos
                                                     : comma - pos);
    const size_t first = token.find_first_not_of(L" \t");
    if (first != std::wstring::npos) {
      const size_t last = token.find_last_not_of(L" \t");
      token = token.substr(first, last - first + 1);
      for (const auto& alias : aliases) {
        if (token == alias.first) {
          token = alias.second;
          break;
        }
      }
      if (!result.empty())
        result += L",";
      result += token;
    }
    if (comma == std::wstring::npos)
      break;
    pos = comma + 1;
  }
  return result;
}

// 方案内字体键(mac 的 preset_color_schemes 可带 label/comment 字体),
// 仅在键存在且有效时覆盖 style 全局值
static void _ApplySchemeFonts(RimeConfig* config,
                              const std::string& prefix,
                              UIStyle& style) {
  int v = 0;
  if (rime_api->config_get_int(config, (prefix + "/font_point").c_str(), &v) &&
      v > 0)
    style.font_point = v;
  if (rime_api->config_get_int(config, (prefix + "/label_font_point").c_str(),
                               &v) &&
      v > 0)
    style.label_font_point = v;
  if (rime_api->config_get_int(config, (prefix + "/comment_font_point").c_str(),
                               &v) &&
      v > 0)
    style.comment_font_point = v;
  const int BUF_SIZE = 2047;
  char fbuf[BUF_SIZE + 1] = {0};
  if (rime_api->config_get_string(config,
                                  (prefix + "/label_font_face").c_str(), fbuf,
                                  BUF_SIZE)) {
    const std::wstring face = _AliasFontFace(u8tow(fbuf));
    if (!face.empty())
      style.label_font_face = face;
  }
  memset(fbuf, 0, sizeof(fbuf));
  if (rime_api->config_get_string(config,
                                  (prefix + "/comment_font_face").c_str(), fbuf,
                                  BUF_SIZE)) {
    const std::wstring face = _AliasFontFace(u8tow(fbuf));
    if (!face.empty())
      style.comment_font_face = face;
  }
}

// 对齐 macOS alphaValue：先绘制背景、高亮和文字，再对整窗统一透明。
static void _ApplyStyleAlpha(RimeConfig* config, UIStyle& style) {
  double opacity = 1.0;
  rime_api->config_get_double(config, "style/alpha", &opacity);
  style.window_alpha = bangke::WindowOpacity(opacity);
}

// load color configs to style, by "style/color_scheme" or specific scheme name
// "color" which is default empty
static bool _UpdateUIStyleColor(RimeConfig* config,
                                UIStyle& style,
                                const std::string& color,
                                bool dark_mode) {
  const int BUF_SIZE = 255;
  char buffer[BUF_SIZE + 1] = {0};
  std::string color_mark = "style/color_scheme";
  // color scheme
  if (rime_api->config_get_string(config, color_mark.c_str(), buffer,
                                  BUF_SIZE) ||
      !color.empty()) {
    const std::string scheme = color.empty() ? std::string(buffer) : color;
    // native(系统配色):不带色值,mac 走 AppKit 语义色,Windows 用
    // BangkeNativeTheme 烘焙的两套固定色按外观解析
    if (scheme == "native") {
      ApplyNativeColors(style, dark_mode);
      _ApplyStyleAlpha(config, style);
      return true;
    }
    std::string prefix("preset_color_schemes/");
    prefix += scheme;
    // define color format, default abgr if not set
    ColorFormat fmt = COLOR_ABGR;
    static constexpr Array<ColorFormat, 3> _colorFmt = {
        {{"argb", COLOR_ARGB}, {"rgba", COLOR_RGBA}, {"abgr", COLOR_ABGR}}};
    _RimeParseStringOptWithFallback(config, (prefix + "/color_format").c_str(),
                                    fmt, _colorFmt, COLOR_ABGR);
#define COLOR(key, value, fallback) \
  _RimeGetColor(config, (prefix + "/" + key), value, fmt, fallback)
    COLOR("back_color", style.back_color, 0xffffffff);
    // 面板软影默认 20% 黑:mac 的 NSPanel 恒有系统窗影,mac 方案不带
    // shadow_color,靠这个兜底才有等效的层次
    COLOR("shadow_color", style.shadow_color, 0x33000000);
    COLOR("prevpage_color", style.prevpage_color, 0);
    COLOR("nextpage_color", style.nextpage_color, 0);
    COLOR("text_color", style.text_color, 0xff000000);
    COLOR("candidate_text_color", style.candidate_text_color, style.text_color);
    COLOR("candidate_back_color", style.candidate_back_color, 0);
    COLOR("border_color", style.border_color, style.text_color);
    COLOR("hilited_text_color", style.hilited_text_color, style.text_color);
    COLOR("hilited_back_color", style.hilited_back_color, style.back_color);
    COLOR("hilited_candidate_text_color", style.hilited_candidate_text_color,
          style.hilited_text_color);
    COLOR("hilited_candidate_back_color", style.hilited_candidate_back_color,
          style.hilited_back_color);
    COLOR("hilited_candidate_shadow_color",
          style.hilited_candidate_shadow_color, 0);
    COLOR("hilited_shadow_color", style.hilited_shadow_color, 0);
    COLOR("candidate_shadow_color", style.candidate_shadow_color, 0);
    COLOR("candidate_border_color", style.candidate_border_color, 0);
    COLOR("hilited_candidate_border_color",
          style.hilited_candidate_border_color, 0);
    COLOR("label_color", style.label_text_color,
          blend_colors(style.candidate_text_color, style.candidate_back_color));
    COLOR("hilited_label_color", style.hilited_label_text_color,
          blend_colors(style.hilited_candidate_text_color,
                       style.hilited_candidate_back_color));
    COLOR("comment_text_color", style.comment_text_color,
          style.label_text_color);
    COLOR("hilited_comment_text_color", style.hilited_comment_text_color,
          style.hilited_label_text_color);
    COLOR("hilited_mark_color", style.hilited_mark_color, 0);
#undef COLOR
    _ApplySchemeFonts(config, prefix, style);
    _ApplyStyleAlpha(config, style);
    return true;
  }
  return false;
}
static void _LoadAppOptions(RimeConfig* config,
                            AppOptionsByAppName& app_options) {
  app_options.clear();
  ForEachRimeMap(
      config, "app_options", [&](const char* app_key, const char* app_path) {
        AppOptions& options(app_options[app_key]);
        ForEachRimeMap(
            config, app_path, [&](const char* opt_key, const char* opt_path) {
              Bool value = False;
              if (rime_api->config_get_bool(config, opt_path, &value)) {
                options[opt_key] = !!value;
              }
            });
      });
}

void BangkeRimeHandler::_GetStatus(Status& stat,
                                       BangkeSessionId ipc_id,
                                       Context& ctx) {
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  RIME_STRUCT(RimeStatus, status);
  if (rime_api->get_status(session_id, &status)) {
    std::string schema_id = "";
    if (status.schema_id)
      schema_id = status.schema_id;
    stat.schema_name = u8tow(status.schema_name);
    stat.schema_id = u8tow(status.schema_id);
    stat.ascii_mode = !!status.is_ascii_mode;
    stat.composing = !!status.is_composing;
    stat.disabled = !!status.is_disabled;
    stat.full_shape = !!status.is_full_shape;
    if (schema_id != m_last_schema_id) {
      session_status.__synced = false;
      m_last_schema_id = schema_id;
      if (schema_id != ".default") {  // don't load for schema select menu
        bool inline_preedit = session_status.style.inline_preedit;
        _LoadSchemaSpecificSettings(ipc_id, schema_id);
        _LoadAppInlinePreeditSet(ipc_id, true);
        if (session_status.style.inline_preedit != inline_preedit)
          // in case of inline_preedit set in schema
          _UpdateInlinePreeditStatus(ipc_id);
        // refresh icon after schema changed
        _RefreshTrayIcon(session_id, _UpdateUICallback);
        m_ui->style() = session_status.style;
        if (m_show_notifications.find("schema") != m_show_notifications.end() &&
            m_show_notifications_time > 0) {
          ctx.aux.str = stat.schema_name;
          m_ui->Update(ctx, stat);
          m_ui->ShowWithTimeout(m_show_notifications_time);
        }
      }
    }
    rime_api->free_status(&status);
  }
}

void BangkeRimeHandler::_GetContext(Context& bangke_context,
                                        RimeSessionId session_id) {
  RIME_STRUCT(RimeContext, ctx);
  if (rime_api->get_context(session_id, &ctx)) {
    if (ctx.composition.length > 0) {
      bangke_context.preedit.str = u8tow(ctx.composition.preedit);
      if (ctx.composition.sel_start < ctx.composition.sel_end) {
        TextAttribute attr;
        attr.type = HIGHLIGHTED;
        attr.range.start =
            utf8towcslen(ctx.composition.preedit, ctx.composition.sel_start);
        attr.range.end =
            utf8towcslen(ctx.composition.preedit, ctx.composition.sel_end);

        bangke_context.preedit.attributes.push_back(attr);
      }
    }
    if (ctx.menu.num_candidates) {
      CandidateInfo& cinfo(bangke_context.cinfo);
      _GetCandidateInfo(cinfo, ctx);
    }
    rime_api->free_context(&ctx);
  }
}

void BangkeRimeHandler::_UpdateInlinePreeditStatus(BangkeSessionId ipc_id) {
  if (!m_ui)
    return;
  SessionStatus& session_status = get_session_status(ipc_id);
  RimeSessionId session_id = session_status.session_id;
  // set inline_preedit option
  bool inline_preedit = session_status.style.inline_preedit;
  rime_api->set_option(session_id, "inline_preedit", Bool(inline_preedit));
  // show soft cursor on the panel but not inline
  rime_api->set_option(session_id, "soft_cursor", Bool(!inline_preedit));
}

