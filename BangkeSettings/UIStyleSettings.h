#pragma once

#include <rime_levers_api.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct ColorSchemeInfo {
  std::string color_scheme_id;
  std::string name;
  std::string author;
};

class UIStyleSettings {
 public:
  UIStyleSettings();

  bool Load();
  bool Save();

  bool GetPresetColorSchemes(std::vector<ColorSchemeInfo>* result);
  // 指定方案的原始配色键值,统一归一为 0xAARRGGBB
  std::map<std::string, unsigned int> GetSchemeColors(
      const std::string& color_scheme_id);
  std::string GetActiveColorScheme();
  bool SelectColorScheme(const std::string& color_scheme_id);

  int GetFontSize(int fallback);
  void SetFontSize(int value);

  bool GetHorizontal(bool fallback);
  void SetHorizontal(bool value);

  // ---- 布局微调(style/layout/*):读 = 补丁 ⊕ 共享默认,写 = 补丁 ----
  // key 为 layout 下的键名,如 "corner_radius"
  int GetLayoutInt(const std::string& key, int fallback);
  void SetLayoutInt(const std::string& key, int value);

  // ---- style/ 下的普通键,同样读 = 补丁 ⊕ 共享默认、写 = 补丁 ----
  // key 为 style 下的相对路径,如 "inline_preedit"、"layout/spacing"
  bool GetStyleBool(const std::string& key, bool fallback);
  void SetStyleBool(const std::string& key, bool value);
  int GetStyleInt(const std::string& key, int fallback);
  void SetStyleInt(const std::string& key, int value);
  std::string GetStyleString(const std::string& key,
                             const std::string& fallback);
  void SetStyleString(const std::string& key, const std::string& value);

  // ---- 自定义配色(用户补丁里的 preset_color_schemes/bangke_custom)----
  // 颜色一律 0xAARRGGBB;方案节点声明 color_format: argb,直读直写
  bool HasCustomScheme();
  unsigned int GetCustomColor(const std::string& key, unsigned int fallback);
  void SetCustomColor(const std::string& key, unsigned int argb);

  RimeCustomSettings* settings() { return settings_; }

 private:
  RimeLeversApi* api_;
  RimeCustomSettings* settings_;
};
