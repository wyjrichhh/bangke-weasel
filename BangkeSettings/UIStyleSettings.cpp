#include "UIStyleSettings.h"

#include <windows.h>

#include <fstream>
#include <string>

#include "Levers.h"
#include <BangkeUtility.h>
#include <rime_api.h>

UIStyleSettings::UIStyleSettings() {
  api_ = leversApi();
  settings_ = api_->custom_settings_init("weasel", "Bangke::UIStyleSettings");
}

bool UIStyleSettings::Load() {
  return api_->load_settings(settings_);
}

bool UIStyleSettings::Save() {
  return api_->save_settings(settings_);
}

bool UIStyleSettings::GetPresetColorSchemes(
    std::vector<ColorSchemeInfo>* result) {
  if (!result)
    return false;
  result->clear();
  // 直接读共享 weasel.yaml,行扫描 preset_color_schemes: 块:
  // 二空格 "  <id>:" 条目行与随后的 "    name: <名称>" 配对。
  // 不走 rime API——config_load_string 对大 yaml 不稳定。
  std::ifstream ifs(wtou8(BangkeSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return false;
  std::string line;
  bool inBlock = false;
  std::string pendingId;
  while (std::getline(ifs, line)) {
    if (line.find("preset_color_schemes:") != std::string::npos) {
      inBlock = true;
      continue;
    }
    if (!inBlock)
      continue;
    if (!line.empty() && line[0] != ' ' && line[0] != '#')
      break;  // 退出块
    if (line.size() > 2 && line[0] == ' ' && line[1] == ' ' && line[2] != ' ' &&
        line[2] != '#') {
      const auto colon = line.find(':');
      if (colon != std::string::npos) {
        pendingId = line.substr(2, colon - 2);
        continue;
      }
    }
    auto namePos = line.find("name:");
    if (namePos != std::string::npos && namePos < 8 && !pendingId.empty()) {
      std::string name = line.substr(namePos + 5);
      name.erase(0, name.find_first_not_of(" \t"));
      name.erase(name.find_last_not_of(" \t\r\n") + 1);
      if (!name.empty()) {
        result->push_back(ColorSchemeInfo());
        result->back().name = name;
        result->back().color_scheme_id = pendingId;
        pendingId.clear();
      }
    }
  }
  return !result->empty();
}

static inline bool IfFileExist(std::string filename) {
  DWORD dwAttrib = GetFileAttributes(acptow(filename).c_str());
  return (INVALID_FILE_ATTRIBUTES != dwAttrib &&
          0 == (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// 归一为 0xAARRGGBB。6 位值补 0xff alpha,8 位按声明的 color_format 换序
// (与 server 端 _RimeGetColor 的语义一致)
static unsigned int NormalizeColor(unsigned int v,
                                   const std::string& format,
                                   bool has_alpha) {
  if (!has_alpha)
    v |= 0xFF000000;
  if (format == "argb")
    return v;
  if (format == "rgba")
    return ((v & 0x000000FF) << 24) | (v & 0xFFFFFF00);
  return (v & 0xFF000000) | ((v & 0x000000FF) << 16) | (v & 0x0000FF00) |
         ((v & 0x00FF0000) >> 16);
}

std::map<std::string, unsigned int> UIStyleSettings::GetSchemeColors(
    const std::string& color_scheme_id) {
  std::map<std::string, unsigned int> result;
  std::ifstream ifs(wtou8(BangkeSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return result;
  std::string line;
  bool inSchemes = false, inScheme = false;
  std::string format = "abgr";
  const std::string want = "  " + color_scheme_id + ":";
  while (std::getline(ifs, line)) {
    if (line.find("preset_color_schemes:") != std::string::npos) {
      inSchemes = true;
      continue;
    }
    if (!inSchemes)
      continue;
    if (!line.empty() && line[0] != ' ' && line[0] != '#')
      break;  // 退出块
    if (line.rfind(want, 0) == 0) {
      inScheme = true;
      continue;
    }
    if (inScheme && line.size() > 2 && line[0] == ' ' && line[1] == ' ' &&
        line[2] != ' ') {
      break;  // 下一个方案开始
    }
    if (!inScheme)
      continue;
    const auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    std::string key = line.substr(4, colon - 4);
    std::string val = line.substr(colon + 1);
    val.erase(0, val.find_first_not_of(" \t"));
    if (key == "color_format") {
      val.erase(val.find_last_not_of(" \t\r\n#") + 1);
      format = val;
      continue;
    }
    if (key == "name" || key == "author")
      continue;
    // 只收颜色键:剥注释后 0x.. / 十进制
    const auto hash = val.find('#');
    if (hash != std::string::npos)
      val = val.substr(0, hash);
    val.erase(val.find_last_not_of(" \t\r\n") + 1);
    if (val.empty())
      continue;
    try {
      // 0x 前缀按 16 进制;带 0x 共 10 字符(=8 位数字)视为带 alpha
      const bool hex = val.rfind("0x", 0) == 0 || val.rfind("0X", 0) == 0;
      unsigned int v = hex ? std::stoul(val, nullptr, 16) : std::stoul(val);
      result[key] = NormalizeColor(
          v, format, hex ? val.length() > 8 : val.length() >= 10);
    } catch (...) {
    }
  }
  return result;
}

std::string UIStyleSettings::GetActiveColorScheme() {
  RimeConfig config = {0};
  api_->settings_get_config(settings_, &config);
  const char* value =
      rime_get_api()->config_get_cstring(&config, "style/color_scheme");
  if (!value)
    return std::string();
  return std::string(value);
}

bool UIStyleSettings::SelectColorScheme(const std::string& color_scheme_id) {
  api_->customize_string(settings_, "style/color_scheme",
                         color_scheme_id.c_str());
  return true;
}

int UIStyleSettings::GetFontSize(int fallback) {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return fallback;
  int value = fallback;
  if (!rime_get_api()->config_get_int(&config, "style/font_point", &value))
    return fallback;
  return value;
}

void UIStyleSettings::SetFontSize(int value) {
  api_->customize_int(settings_, "style/font_point", value);
}

bool UIStyleSettings::GetHorizontal(bool fallback) {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return fallback;
  Bool value = fallback;
  if (!rime_get_api()->config_get_bool(&config, "style/horizontal", &value))
    return fallback;
  return value != False;
}

void UIStyleSettings::SetHorizontal(bool value) {
  api_->customize_bool(settings_, "style/horizontal", value);
}

static void StripYamlComment(std::string& val) {
  const auto hash = val.find('#');
  if (hash != std::string::npos)
    val = val.substr(0, hash);
  val.erase(0, val.find_first_not_of(" \t"));
  if (val.empty())
    return;
  val.erase(val.find_last_not_of(" \t\r\n") + 1);
}

// 共享 weasel.yaml 的 style: 块整表(显示"当前生效值"的默认基线)。
// 嵌套一层的子块展平成 "子块/键"(现只有 layout:)。
static std::map<std::string, std::string> ScanSharedStyle() {
  std::map<std::string, std::string> result;
  std::ifstream ifs(wtou8(BangkeSharedDataPath().wstring()) + "\\weasel.yaml");
  if (!ifs.good())
    return result;
  std::string line;
  bool inStyle = false;
  std::string sub;
  while (std::getline(ifs, line)) {
    if (line.rfind("style:", 0) == 0) {
      inStyle = true;
      continue;
    }
    if (inStyle && !line.empty() && line[0] != ' ' && line[0] != '#')
      break;  // style 块结束
    if (!inStyle)
      continue;
    if (line.size() > 2 && line[0] == ' ' && line[1] == ' ' && line[2] != ' ' &&
        line[2] != '#') {
      const auto colon = line.find(':');
      if (colon == std::string::npos)
        continue;
      std::string val = line.substr(colon + 1);
      StripYamlComment(val);
      if (val.empty()) {
        sub = line.substr(2, colon - 2);  // 子块头
      } else {
        sub.clear();
        result[line.substr(2, colon - 2)] = val;
      }
      continue;
    }
    if (!sub.empty() && line.size() > 4 && line[0] == ' ' && line[1] == ' ' &&
        line[2] == ' ' && line[3] == ' ' && line[4] != ' ') {
      const auto colon = line.find(':');
      if (colon == std::string::npos)
        continue;
      std::string val = line.substr(colon + 1);
      StripYamlComment(val);
      if (!val.empty())
        result[sub + "/" + line.substr(4, colon - 4)] = val;
    }
  }
  return result;
}

// 补丁优先;未打补丁则用共享默认;再兜底代码默认
static bool PatchRaw(RimeCustomSettings* settings,
                     RimeLeversApi* api,
                     const std::string& key,
                     std::string* out) {
  RimeConfig config = {0};
  if (!api->settings_get_config(settings, &config))
    return false;
  char buffer[MAX_PATH] = {0};
  if (!rime_get_api()->config_get_string(&config, ("style/" + key).c_str(),
                                         buffer, sizeof(buffer)))
    return false;
  *out = buffer;
  return true;
}

static std::string SharedStyleValue(const std::string& key) {
  const auto shared = ScanSharedStyle();
  auto it = shared.find(key);
  return it == shared.end() ? std::string() : it->second;
}

int UIStyleSettings::GetStyleInt(const std::string& key, int fallback) {
  std::string raw;
  if (!PatchRaw(settings_, api_, key, &raw))
    raw = SharedStyleValue(key);
  if (raw.empty())
    return fallback;
  try {
    return std::stoi(raw);
  } catch (...) {
    return fallback;
  }
}

void UIStyleSettings::SetStyleInt(const std::string& key, int value) {
  api_->customize_int(settings_, ("style/" + key).c_str(), value);
}

bool UIStyleSettings::GetStyleBool(const std::string& key, bool fallback) {
  RimeConfig config = {0};
  if (api_->settings_get_config(settings_, &config)) {
    Bool v = fallback ? True : False;
    if (rime_get_api()->config_get_bool(&config, ("style/" + key).c_str(), &v))
      return v != False;
  }
  const std::string raw = SharedStyleValue(key);
  if (raw.empty())
    return fallback;
  return raw == "true" || raw == "yes" || raw == "on" || raw == "1";
}

void UIStyleSettings::SetStyleBool(const std::string& key, bool value) {
  api_->customize_bool(settings_, ("style/" + key).c_str(), value);
}

std::string UIStyleSettings::GetStyleString(const std::string& key,
                                            const std::string& fallback) {
  std::string raw;
  if (PatchRaw(settings_, api_, key, &raw))
    return raw;
  const std::string shared = SharedStyleValue(key);
  return shared.empty() ? fallback : shared;
}

void UIStyleSettings::SetStyleString(const std::string& key,
                                     const std::string& value) {
  api_->customize_string(settings_, ("style/" + key).c_str(), value.c_str());
}

int UIStyleSettings::GetLayoutInt(const std::string& key, int fallback) {
  return GetStyleInt("layout/" + key, fallback);
}

void UIStyleSettings::SetLayoutInt(const std::string& key, int value) {
  SetStyleInt("layout/" + key, value);
}

bool UIStyleSettings::HasCustomScheme() {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return false;
  return rime_get_api()->config_get_cstring(
             &config, "preset_color_schemes/bangke_custom/back_color") !=
         nullptr;
}

unsigned int UIStyleSettings::GetCustomColor(const std::string& key,
                                             unsigned int fallback) {
  RimeConfig config = {0};
  if (!api_->settings_get_config(settings_, &config))
    return fallback;
  char buf[32] = {0};
  if (!rime_get_api()->config_get_string(
          &config, ("preset_color_schemes/bangke_custom/" + key).c_str(), buf,
          sizeof(buf)))
    return fallback;
  // 只认 0x 十六进制写入;历史十进制条目解析端本就拒收,按未设置处理
  if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X'))
    return (unsigned int)strtoul(buf + 2, nullptr, 16);
  return fallback;
}

void UIStyleSettings::SetCustomColor(const std::string& key,
                                     unsigned int argb) {
  // 必须写 "0xAARRGGBB" 字符串:customize_int 会把负数序列化成带引号的
  // 十进制串,server 端 _RimeGetColor 既不认其格式、也无法从字符串节点
  // 取 int,整个方案会回落默认色(透明度失灵即此因)
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%08X", argb);
  api_->customize_string(settings_,
                         ("preset_color_schemes/bangke_custom/" + key).c_str(),
                         buf);
  api_->customize_string(settings_, "preset_color_schemes/bangke_custom/name",
                         "自定义");
  api_->customize_string(settings_,
                         "preset_color_schemes/bangke_custom/color_format",
                         "argb");
}
