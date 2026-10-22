#include "stdafx.h"
#include <fstream>
#include <StringAlgorithm.hpp>
#include <filesystem>
#include <string>
#include <BangkeUtility.h>
#include <BangkeModelFiles.h>

fs::path BangkeUserDataPath() {
  WCHAR _path[MAX_PATH] = {0};
  const WCHAR KEY[] = L"Software\\Bangke";
  HKEY hKey;
  LSTATUS ret = RegOpenKey(HKEY_CURRENT_USER, KEY, &hKey);
  if (ret == ERROR_SUCCESS) {
    DWORD len = sizeof(_path);
    DWORD type = 0;
    DWORD data = 0;
    ret =
        RegQueryValueEx(hKey, L"BangkeUserDir", NULL, &type, (LPBYTE)_path, &len);
    RegCloseKey(hKey);
    if (ret == ERROR_SUCCESS && type == REG_SZ && _path[0]) {
      return fs::path(_path);
    }
  }
  // default location
  ExpandEnvironmentStringsW(L"%AppData%\\Bangke", _path, _countof(_path));
  return fs::path(_path);
}

fs::path BangkeSharedDataPath() {
  wchar_t _path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, _path, _countof(_path));
  return fs::path(_path).remove_filename().append("data");
}

std::string GetCustomResource(const char* name, const char* type) {
  const HINSTANCE module = 0;  // main executable
  HRSRC hRes = FindResourceA(module, name, type);
  if (hRes) {
    HGLOBAL hData = LoadResource(module, hRes);
    if (hData) {
      const char* data = (const char*)::LockResource(hData);
      size_t size = ::SizeofResource(module, hRes);

      if (data && size) {
        if (data[size - 1] == '\0')  // null-terminated string
          size--;
        return std::string(data, size);
      }
    }
  }

  return std::string();
}

// 用户目录种子:AI 接线/方案表/模型。文件缺失**或为空**都写——
// 设置程序构造器会先创建空 default.custom.yaml,只查 exists 会永远跳过。
// 在设置程序 /install 与服务端首次启动两处调用,谁先到谁补,幂等。
int BangkeEnsureUserSeeds() {
  // 与插件契约对齐:model_path 相对路径按 user_data_dir 解析,
  // 因此模型必须落到用户目录;接线走 luna_pinyin.custom.yaml patch
  const std::filesystem::path user_dir = BangkeUserDataPath();

  // 宽字面量经 wtou8 落盘:窄字面量受源文件编码影响,不冒险
  static const wchar_t kAiWiringYaml[] =
      L"patch:\n"
      L"  schema/name: 蚌壳-明月\n"
      L"  # ai-predict: translator 必须列首位(MergedTranslation::Elect 顺序),\n"
      L"  # 因此整列重写朙月拼音原生 translator 并在最前加 ai_predict_translator\n"
      L"  engine/translators:\n"
      L"    - ai_predict_translator\n"
      L"    - punct_translator\n"
      L"    - table_translator@custom_phrase\n"
      L"    - reverse_lookup_translator\n"
      L"    - script_translator\n"
      L"  # filter 用 @next 追加,保留原生 simplifier/uniquifier 链\n"
      L"  'engine/filters/@next': ai_predict_filter\n"
      L"  # 默认简体(CT2 模型简体训练)\n"
      L"  switches/@2/reset: 1\n"
      L"  ai_predict:\n"
      L"    model_path: predict_models/zh-base-ct2-int8\n"
      L"    enabled: true\n"
      L"    device: cpu\n"
      L"    min_input_length: 12\n"
      L"    min_context_prompt_length: 2\n"
      L"    context_window_size: 10\n"
      L"    debounce_ms: 200\n"
      L"    max_tokens: 64\n"
      L"    target_index: 1\n"
      L"    search_range: 10\n"
      L"    min_hanzi: 2\n"
      L"    # 是否剥掉模型输出里的标点。默认 true 与上线前行为一致;关掉可保留\n"
      L"    # 模型吐出的句读(如「，我们」),代价是上下文复读的标点残留也会进候选\n"
      L"    filter_punctuation: true\n"
      L"    # 纯推理模式:候选只来自模型,词库词不参与,候选栏只剩一条不带序号的预测\n"
      L"    ai_only: false\n";

  // 全新安装的方案集:明月 + 雾凇(对齐 Linux 版),仅缺文件时写入
  // 列序即默认方案序:雾凇打头,全新安装落在雾凇上
  static const wchar_t kDefaultSchemasYaml[] =
      L"patch:\n"
      L"  schema_list:\n"
      L"    - schema: rime_ice\n"
      L"    - schema: luna_pinyin\n";
  {
    std::error_code dec;
    const std::filesystem::path dcustom = user_dir / L"default.custom.yaml";
    if (!std::filesystem::exists(dcustom, dec) ||
        std::filesystem::file_size(dcustom, dec) == 0) {
      std::ofstream o(dcustom, std::ios::binary);
      o << wtou8(kDefaultSchemasYaml);
    }
  }

  const std::filesystem::path custom = user_dir / L"luna_pinyin.custom.yaml";
  std::error_code ec;
  if (!std::filesystem::exists(custom, ec) ||
      std::filesystem::file_size(custom, ec) == 0) {
    std::ofstream o(custom, std::ios::binary);
    o << wtou8(kAiWiringYaml);
  }

  // 雾凇:translator 链整列重写(明月同款纪律),仅缺文件时写入
  static const wchar_t kIceWiringYaml[] =
      L"patch:\n"
      L"  schema/name: 蚌壳-雾凇\n"
      L"  # ai-predict: translator 必须列首位(MergedTranslation::Elect 顺序)\n"
      L"  engine/translators:\n"
      L"    - ai_predict_translator\n"
      L"    - punct_translator\n"
      L"    - script_translator\n"
      L"    - lua_translator@*date_translator\n"
      L"    - lua_translator@*lunar\n"
      L"    - lua_translator@*uuid\n"
      L"    - table_translator@custom_phrase\n"
      L"    - table_translator@melt_eng\n"
      L"    - table_translator@cn_en\n"
      L"    - table_translator@radical_lookup\n"
      L"    - lua_translator@*unicode\n"
      L"    - lua_translator@*number_translator\n"
      L"    - lua_translator@*calc_translator\n"
      L"    - lua_translator@*force_gc\n"
      L"  'engine/filters/@next': ai_predict_filter\n"
      L"  ai_predict:\n"
      L"    model_path: predict_models/zh-base-ct2-int8\n"
      L"    enabled: true\n"
      L"    device: cpu\n"
      L"    min_input_length: 12\n"
      L"    min_context_prompt_length: 2\n"
      L"    context_window_size: 10\n"
      L"    debounce_ms: 200\n"
      L"    max_tokens: 64\n"
      L"    target_index: 1\n"
      L"    search_range: 10\n"
      L"    min_hanzi: 2\n"
      L"    # 是否剥掉模型输出里的标点。默认 true 与上线前行为一致;关掉可保留\n"
      L"    # 模型吐出的句读(如「，我们」),代价是上下文复读的标点残留也会进候选\n"
      L"    filter_punctuation: true\n"
      L"    # 纯推理模式:候选只来自模型,词库词不参与,候选栏只剩一条不带序号的预测\n"
      L"    ai_only: false\n";
  const std::filesystem::path iceCustom = user_dir / L"rime_ice.custom.yaml";
  if (!std::filesystem::exists(iceCustom, ec) ||
      std::filesystem::file_size(iceCustom, ec) == 0) {
    std::ofstream o(iceCustom, std::ios::binary);
    o << wtou8(kIceWiringYaml);
  }

  // Repair missing/empty files from an interrupted first-run copy, while
  // retaining nonempty user files and any separately selected model directory.
  const auto source = BangkeSharedDataPath().parent_path() / L"predict_models" / L"zh-base-ct2-int8";
  const auto destination = user_dir / L"predict_models" / L"zh-base-ct2-int8";
  return bangke::StageMissingModelFiles(source, destination) ? 1 : 0;
}
