#include "Configurator.h"

#include <windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Levers.h"
#include <BangkeConstants.h>
#include <BangkeIPC.h>
#include <BangkeUtility.h>
#include "UIStyleSettings.h"
#pragma warning(disable : 4005)
#include <rime_api.h>
#pragma warning(default : 4005)

#include <set>

static void CreateFileIfNotExist(std::string filename) {
  std::filesystem::path file_path = BangkeUserDataPath() / u8tow(filename);
  DWORD dwAttrib = GetFileAttributes(file_path.c_str());
  if (!(INVALID_FILE_ATTRIBUTES != dwAttrib &&
        0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))) {
    std::wofstream o(file_path.c_str(), std::ios::app);
    o.close();
  }
}

Configurator::Configurator() {
  CreateFileIfNotExist("default.custom.yaml");
  CreateFileIfNotExist("weasel.custom.yaml");
}

Configurator::~Configurator() {
  EndDictSession();
}

void Configurator::Initialize() {
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
  RimeApi* rime_api = rime_get_api();
  assert(rime_api);
  rime_api->setup(&bangke_traits);
  rime_api->deployer_initialize(NULL);
}

// 一次性迁移(0.24 → 0.25 升级):
// ① 配色方案表与 mac 对齐后,补丁里指向已删方案的 style/color_scheme(_dark)
//   改写为 native,否则服务端落进 fallback 色;
// ② 0.3.x 设置程序写进补丁的布局/字体/样式键(旧度量:margin 12/7、胶囊
//   高亮、字号 14…)会把 0.25 的 mac 化出厂全部盖掉 —— 删除这些补丁行,
//   让新出厂生效。保留:配色方案选择、alpha(0.4.0 键)、preset_color_schemes
//   用户自定义块。以补丁里的标记键保证只跑一次(以后版本的保存不受影响)。
// 只扫 levers 写出的扁平键行;幂等,处于调用方的部署临界区内。
static void MigrateColorSchemes() {
  std::vector<ColorSchemeInfo> presets;
  {
    UIStyleSettings ui;
    if (!ui.GetPresetColorSchemes(&presets))
      return;
  }
  std::set<std::string> valid{"bangke_custom"};
  for (const auto& s : presets)
    valid.insert(s.color_scheme_id);

  const std::filesystem::path patch =
      BangkeUserDataPath() / L"weasel.custom.yaml";
  std::ifstream in(patch, std::ios::binary);
  if (!in.good())
    return;
  const std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
  in.close();
  if (content.find("__migrated_025") != std::string::npos)
    return;  // 已迁移过

  bool dirty = false;
  bool legacyHit = false;
  std::string out;
  out.reserve(content.size());
  size_t pos = 0;
  while (pos <= content.size()) {
    const size_t eol = content.find('\n', pos);
    std::string line = content.substr(
        pos, eol == std::string::npos ? std::string::npos : eol - pos);
    std::string trimmed = line;
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    // 剥掉引号形态的键名前缀,得到裸键(带冒号后缀的形态不影响前缀判断)
    std::string bare = trimmed;
    if (bare.size() >= 2 && bare[0] == '"') {
      const auto close = bare.find('"', 1);
      if (close != std::string::npos)
        bare = bare.substr(1, close - 1) + ":";
    }

    const bool isStyleKey = bare.rfind("style/", 0) == 0;
    const bool keep = isStyleKey &&
                      (bare.rfind("style/color_scheme", 0) == 0 ||
                       bare.rfind("style/alpha", 0) == 0 ||
                       bare.rfind("style/preset_color_schemes", 0) == 0);
    const bool legacy = isStyleKey && !keep;

    for (const char* key : {"style/color_scheme", "style/color_scheme_dark"}) {
      const std::string plain = std::string(key) + ":";
      const std::string quoted = "\"" + std::string(key) + "\":";
      const bool plainHit = trimmed.rfind(plain, 0) == 0;
      const bool quotedHit = trimmed.rfind(quoted, 0) == 0;
      if (!plainHit && !quotedHit)
        continue;
      std::string value =
          trimmed.substr((plainHit ? plain : quoted).size());
      const auto hash = value.find('#');
      if (hash != std::string::npos)
        value = value.substr(0, hash);
      value.erase(0, value.find_first_not_of(" \t"));
      value.erase(value.find_last_not_of(" \t\r") + 1);
      if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
      if (!value.empty() && !valid.count(value)) {
        const size_t indent = line.find_first_not_of(" \t");
        line = line.substr(0, indent) + plain + std::string(" native");
        dirty = true;
      }
    }

    if (legacy) {
      legacyHit = true;
      dirty = true;
      // 跳过该行(删除)
    } else {
      out += line;
      if (eol != std::string::npos)
        out += '\n';
    }
    if (eol == std::string::npos)
      break;
    pos = eol + 1;
  }
  if (legacyHit) {
    // 迁移标记写进补丁,保证只跑一次
    if (!out.empty() && out.back() != '\n')
      out += '\n';
    out += "  \"style/__migrated_025\": true\n";
  }
  if (!dirty)
    return;
  std::ofstream of(patch, std::ios::binary | std::ios::trunc);
  of << out;
}

int Configurator::UpdateWorkspace(bool report_errors) {
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerMutex");
  if (!hMutex)
    return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    if (report_errors)
      emit info(QStringLiteral(u"蚌壳拼音"),
                QStringLiteral(u"正在执行另一项部署任务，方才所做的修改将在输入法再次启动后生效。"));
    return 1;
  }

  bangke::Client client;
  if (client.Connect())
    client.StartMaintenance();

  {
    RimeApi* rime = rime_get_api();
    MigrateColorSchemes();
    rime->deploy();
    rime->deploy_config_file("weasel.yaml", "config_version");
  }

  CloseHandle(hMutex);

  if (client.Connect())
    client.EndMaintenance();
  return 0;
}

// 轻量生效:样式/行为类改动 = 只合并 weasel 配置 + 服务端热重载。
// deploy_config_file 按【源文件时间戳】对照(ConfigNeedsUpdate),custom
// 补丁一变就重建 build/weasel.yaml —— 毫秒级,SchemaUpdate/词典编译
// 完全不跑(词典只在进程初始化或方案/AI 配置变更保存时才碰)。
// 方案列表/AI 配置影响 schema 编译,仍走 UpdateWorkspace 全量部署
int Configurator::ReloadService() {
  RimeApi* rime = rime_get_api();
  rime->deploy_config_file("weasel.yaml", "config_version");
  bangke::Client client;
  if (!client.Connect())
    return 1;
  client.ReloadStyle();
  return 0;
}

int Configurator::EnsureAiDefaults() {
  return BangkeEnsureUserSeeds();
}

// 用户的输入法列表是 HKCU\Control Panel\International\User Profile\<语言标签> 下
// 一组"以输入法 ID 为名、以顺序为值"的 REG_DWORD。ID 形如
// <langid>:{CLSID}{ProfileGUID}，两个 GUID 与 BangkeTSF/Globals.cpp 里的同一对。
//
// 为什么不能只靠 MSI 的 RegisterTSF：那条走的是 regsvr32 → DllRegisterServer →
// ITfInputProcessorProfileMgr::RegisterProfile，它只做机器层面的注册（HKLM 下能看到
// InprocServer32 和 LanguageProfile），不碰用户列表。大版本升级是先卸后装，卸载会把用户
// 列表里那条一并带走，于是装完 Win+Space 切不过去——表现成"只能打英文"。
int Configurator::EnsureInputMethodRegistered(bool setDefault) {
  constexpr wchar_t kProfileKey[] =
      L"Control Panel\\International\\User Profile\\zh-Hans-CN";
  constexpr wchar_t kTip[] =
      L"0804:{9D44BD49-B647-4010-9ADC-16DA253F5CCA}"
      L"{ED00E0F3-7EB2-4D43-898F-E2DA5ACD15CA}";

  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kProfileKey, 0, nullptr, 0, KEY_ALL_ACCESS,
                      nullptr, &key, nullptr) != ERROR_SUCCESS)
    return 1;

  DWORD type = 0;
  // 升级安装链(UnregisterTSF→RegisterTSF)会先删后加用户列表条目,
  // 重新加入时被系统追加到末尾 —— 默认输入法顺位掉到微软拼音后面,
  // 任务栏右键菜单随之变成微软的。setDefault(安装器上下文传入)把
  // 蚌壳设回 1,其余条目顺移;用户手动排序不受日常路径影响
  if (setDefault) {
    for (DWORD i = 0;; ++i) {
      wchar_t name[256] = {0};
      DWORD name_len = _countof(name);
      type = 0;
      DWORD v = 0;
      DWORD vsz = sizeof(v);
      if (RegEnumValueW(key, i, name, &name_len, nullptr, &type,
                        (LPBYTE)&v, &vsz) != ERROR_SUCCESS)
        break;
      if (type != REG_DWORD || wcscmp(name, kTip) == 0)
        continue;
      ++v;  // 其他输入法顺移,蚌壳腾出 1
      RegSetValueExW(key, name, 0, REG_DWORD,
                     (const BYTE*)&v, sizeof(v));
    }
    const DWORD one = 1;
    RegSetValueExW(key, kTip, 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
    RegCloseKey(key);
    return 0;
  }

  DWORD size = 0;
  if (RegQueryValueExW(key, kTip, nullptr, &type, nullptr, &size) == ERROR_SUCCESS) {
    RegCloseKey(key);
    return 0;  // 已经在用户列表里
  }

  // 顺序取现有条目数 + 1：列表里的值本身就是排序号
  DWORD order = 1;
  for (DWORD i = 0;; ++i) {
    wchar_t name[256] = {0};
    DWORD name_len = _countof(name);
    type = 0;
    if (RegEnumValueW(key, i, name, &name_len, nullptr, &type, nullptr,
                      nullptr) != ERROR_SUCCESS)
      break;
    if (type == REG_DWORD)
      ++order;
  }
  const DWORD data = order;
  const LSTATUS ret = RegSetValueExW(key, kTip, 0, REG_DWORD,
                                     reinterpret_cast<const BYTE*>(&data),
                                     sizeof(data));
  RegCloseKey(key);
  if (ret != ERROR_SUCCESS)
    return 1;

  // 让 shell 立刻重读语言配置；拿不到也无妨，下次登录一样生效
  DWORD_PTR unused = 0;
  SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                      reinterpret_cast<LPARAM>(L"UserProfile"),
                      SMTO_ABORTIFHUNG, 2000, &unused);
  return 0;
}

int Configurator::CleanupResidue() {
  // 静态：勿依赖实例状态（构造器会写用户目录，SYSTEM 下路径错误）
  // 被应用进程加载的 TSF dll 删不掉时转由下次重启删除；BangkeServer 自启键
  // 仅在目标文件已成幽灵时移除，避免误伤共存的官方小狼毫
  // TSF 机器级注册(DllRegisterServer 写入 HKLM CTF\TIP)卸载无人反注册,一并清
  {
    HKEY ctf = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\CTF\\TIP", 0,
                      DELETE, &ctf) == ERROR_SUCCESS) {
      RegDeleteTreeW(
          ctf, L"{9D44BD49-B647-4010-9ADC-16DA253F5CCA}");
      RegCloseKey(ctf);
    }
  }
  const wchar_t* residues[] = {L"C:\\Windows\\System32\\bangke.dll",
                               L"C:\\Windows\\System32\\bangke.dll.old"};
  for (const wchar_t* f : residues) {
    if (GetFileAttributesW(f) == INVALID_FILE_ATTRIBUTES)
      continue;
    if (!DeleteFileW(f))
      MoveFileExW(f, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
  }

  HKEY run = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_QUERY_VALUE | KEY_SET_VALUE, &run) == ERROR_SUCCESS) {
    wchar_t val[1024] = {0};
    DWORD sz = sizeof(val);
    if (RegQueryValueExW(run, L"WeaselServer", nullptr, nullptr, (LPBYTE)val, &sz) ==
            ERROR_SUCCESS &&
        GetFileAttributesW(val) == INVALID_FILE_ATTRIBUTES)
      RegDeleteValueW(run, L"WeaselServer");
    RegCloseKey(run);
  }
  return 0;
}

int Configurator::ClearPendingDeletes() {
  // 静态：勿依赖实例状态（构造器会写用户目录，SYSTEM 下路径错误）
  // PFRO 条目可能带 "*N"/"!" 等类型前缀（如 "*1\??\C:\..."），按 "\??\" 起始的
  // 路径主体比对
  static const wchar_t* kProductFiles[] = {
      L"\\??\\C:\\Windows\\System32\\bangke.dll",
      L"\\??\\C:\\Windows\\System32\\bangke.dll.old",
  };
  auto isOurPath = [](const std::wstring& e) {
    const size_t at = e.find(L"\\??\\");
    if (at == std::wstring::npos)
      return false;
    const std::wstring body = e.substr(at);
    for (const wchar_t* f : kProductFiles)
      if (_wcsicmp(body.c_str(), f) == 0)
        return true;
    return false;
  };
  const wchar_t* kSessionMgr =
      L"SYSTEM\\CurrentControlSet\\Control\\Session Manager";
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kSessionMgr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE,
                    &key) != ERROR_SUCCESS)
    return 0;
  wchar_t buf[32768] = {0};
  DWORD sz = sizeof(buf);
  if (RegQueryValueExW(key, L"PendingFileRenameOperations", nullptr, nullptr,
                       (LPBYTE)buf, &sz) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return 0;
  }
  // REG_MULTI_SZ 的单个元素可以是空串（删除对的目标），必须按总长遍历，
  // while(*p) 会在第一个空串处提前终止
  std::vector<std::wstring> entries;
  {
    const size_t total = sz / sizeof(wchar_t);
    size_t pos = 0;
    while (pos < total) {
      const std::wstring e(buf + pos);
      entries.push_back(e);
      pos += e.size() + 1;
    }
    if (!entries.empty() && entries.back().empty())
      entries.pop_back();  // 末尾的终止空串不是元素
  }
  bool changed = false;
  std::vector<std::wstring> kept;
  // PFRO 按 (源, 目标) 成对排列，目标为空串表示删除；整对取舍，绝不能拆散别人的对
  for (size_t i = 0; i < entries.size(); i += 2) {
    if (isOurPath(entries[i])) {
      changed = true;
      continue;
    }
    kept.push_back(entries[i]);
    if (i + 1 < entries.size())
      kept.push_back(entries[i + 1]);
  }
  if (changed) {
    std::wstring flat;
    for (const auto& e : kept) {
      flat += e;
      flat += L'\0';  // wstring+L"\0" 会因 wcslen=0 丢分隔符，必须按字符追加
    }
    flat += L'\0';
    RegSetValueExW(key, L"PendingFileRenameOperations", 0, REG_MULTI_SZ,
                   (const BYTE*)flat.c_str(), (DWORD)(flat.size() * sizeof(wchar_t)));
  }
  RegCloseKey(key);
  return 0;
}

int Configurator::SyncUserData() {
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerMutex");
  if (!hMutex)
    return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    emit info(QStringLiteral(u"蚌壳拼音"),
              QStringLiteral(u"正在执行另一项部署任务，请稍候再试。"));
    return 1;
  }

  bangke::Client client;
  if (client.Connect())
    client.StartMaintenance();

  int ret = 0;
  {
    RimeApi* rime = rime_get_api();
    if (!rime->sync_user_data()) {
      ret = 1;
    }
    rime->join_maintenance_thread();
  }

  CloseHandle(hMutex);

  if (client.Connect())
    client.EndMaintenance();
  return ret;
}

bool Configurator::BeginDictSession() {
  if (m_hDictMutex)
    return true;
  HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BangkeDeployerMutex");
  if (!hMutex)
    return false;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(hMutex);
    emit info(QStringLiteral(u"蚌壳拼音"),
              QStringLiteral(u"正在执行另一项部署任务，请稍候再试。"));
    return false;
  }
  m_hDictMutex = hMutex;

  bangke::Client client;
  if (client.Connect())
    client.StartMaintenance();

  RimeApi* rime = rime_get_api();
  if (RIME_API_AVAILABLE(rime, run_task))
    rime->run_task("installation_update");  // 建立用户数据同步目录
  return true;
}

void Configurator::EndDictSession() {
  if (!m_hDictMutex)
    return;
  CloseHandle(m_hDictMutex);
  m_hDictMutex = nullptr;

  bangke::Client client;
  if (client.Connect())
    client.EndMaintenance();
}
