#pragma once

// Shared Mist (dark) and Paper (light) candidate palettes. Constants are ARGB.

#include <map>
#include <string>

namespace bangke {

struct NativePalette {
  unsigned int back_color;
  unsigned int text_color;
  unsigned int candidate_text_color;
  unsigned int hilited_text_color;
  unsigned int hilited_candidate_text_color;
  unsigned int hilited_candidate_back_color;
  unsigned int label_color;
  unsigned int comment_color;
  unsigned int hilited_label_color;
  unsigned int hilited_comment_color;
};

inline constexpr NativePalette kNativeLight{
    0xFFF6F3ED, 0xFF302C26, 0xFF302C26, 0xFF302C26, 0xFFFFFFFF, 0xFF785330,
    0xFF6B6257, 0xFF6B6257, 0xFFFFFFFF, 0xFFFFFFFF};

inline constexpr NativePalette kNativeDark{
    0xFF171C23, 0xFFEDF1F5, 0xFFEDF1F5, 0xFFEDF1F5, 0xFF102722, 0xFF94C8BC,
    0xFFADB8C7, 0xFFADB8C7, 0xFF102722, 0xFF102722};

inline constexpr unsigned int ArgbToAbgr(unsigned int argb) {
  return (argb & 0xFF00FF00u) | ((argb & 0x00FF0000u) >> 16) |
         ((argb & 0x000000FFu) << 16);
}

// 填 UIStyle 的颜色槽位;未列出的槽位(shadow/border/mark 等)置透明,
// 与 mac native 不带这些键的行为一致。StyleT 即 UIStyle,duck-typing
// 以免头文件反向依赖 IPC 定义。
template <typename StyleT>
void ApplyNativeColors(StyleT& style, bool dark) {
  const NativePalette& p = dark ? kNativeDark : kNativeLight;
  style.back_color = ArgbToAbgr(p.back_color);
  style.shadow_color = 0;
  style.prevpage_color = 0;
  style.nextpage_color = 0;
  style.text_color = ArgbToAbgr(p.text_color);
  style.candidate_text_color = ArgbToAbgr(p.candidate_text_color);
  style.candidate_back_color = 0;
  style.border_color = 0;
  style.hilited_text_color = ArgbToAbgr(p.hilited_text_color);
  style.hilited_back_color = 0;
  style.hilited_candidate_text_color = ArgbToAbgr(p.hilited_candidate_text_color);
  style.hilited_candidate_back_color = ArgbToAbgr(p.hilited_candidate_back_color);
  style.hilited_candidate_shadow_color = 0;
  style.hilited_shadow_color = 0;
  style.candidate_shadow_color = 0;
  style.candidate_border_color = 0;
  style.hilited_candidate_border_color = 0;
  style.label_text_color = ArgbToAbgr(p.label_color);
  style.hilited_label_text_color = ArgbToAbgr(p.hilited_label_color);
  style.comment_text_color = ArgbToAbgr(p.comment_color);
  style.hilited_comment_text_color = ArgbToAbgr(p.hilited_comment_color);
  style.hilited_mark_color = 0;
}

// 以 yaml 方案键的形态给出颜色表,供设置端与普通方案同一套消费路径使用。
inline std::map<std::string, unsigned int> NativeSchemeColors(bool dark) {
  const NativePalette& p = dark ? kNativeDark : kNativeLight;
  return {
      {"back_color", p.back_color},
      {"text_color", p.text_color},
      {"candidate_text_color", p.candidate_text_color},
      {"hilited_text_color", p.hilited_text_color},
      {"hilited_candidate_text_color", p.hilited_candidate_text_color},
      {"hilited_candidate_back_color", p.hilited_candidate_back_color},
      {"label_color", p.label_color},
      {"comment_text_color", p.comment_color},
      {"hilited_label_color", p.hilited_label_color},
      {"hilited_comment_text_color", p.hilited_comment_color},
  };
}

}  // namespace bangke
