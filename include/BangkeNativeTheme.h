#pragma once

// native(系统配色)方案的两套烘焙色。
// macOS 上 native 由 AppKit 语义色动态解析(windowBackgroundColor/
// selectedTextBackgroundColor/labelColor/secondaryLabelColor);Windows 没有
// 对应的动态通道,于 2026-09 在 macOS 26 上实测后固化为常量。不跟随用户
// 自定义强调色,两侧一致。
//
// 常量为 AARRGGBB。BangkeRime 的 UIStyle 颜色槽位是 ABGR(COLORREF),
// 经 ArgbToAbgr 换入;BangkeSettings 的 GetSchemeColors 直接用 ARGB。

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
    0xFFFFFFFF, 0xD9000000, 0xD9000000, 0xD9000000, 0xD9000000, 0xFFB3D7FF,
    0x80000000, 0x80000000, 0x80000000, 0x80000000};

inline constexpr NativePalette kNativeDark{
    0xFF1E1E1E, 0xD9FFFFFF, 0xD9FFFFFF, 0xD9FFFFFF, 0xD9FFFFFF, 0xFF3F638B,
    0x8CFFFFFF, 0x8CFFFFFF, 0x8CFFFFFF, 0x8CFFFFFF};

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
