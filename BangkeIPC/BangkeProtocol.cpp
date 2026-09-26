#include "stdafx.h"
#include <BangkeProtocol.h>

namespace bangke {

void PutText(Writer& w, const bangke::Text& t) {
  w.WStr(t.str);
  w.U32(static_cast<uint32_t>(t.attributes.size()));
  for (const auto& a : t.attributes) {
    w.I32(a.range.start);
    w.I32(a.range.end);
    w.I32(a.range.cursor);
    w.I32(static_cast<int32_t>(a.type));
  }
}

bool GetText(Reader& r, bangke::Text& t) {
  t.str = r.WStr();
  const uint32_t n = r.U32();
  if (!r.Ok() || n > 4096)
    return false;
  t.attributes.resize(n);
  for (uint32_t i = 0; i < n; ++i) {
    t.attributes[i].range.start = r.I32();
    t.attributes[i].range.end = r.I32();
    t.attributes[i].range.cursor = r.I32();
    t.attributes[i].type = static_cast<bangke::TextAttributeType>(r.I32());
  }
  return r.Ok();
}

void PutCandidateInfo(Writer& w, const bangke::CandidateInfo& ci) {
  w.I32(ci.currentPage);
  w.Bool(ci.is_last_page);
  w.I32(ci.totalPages);
  w.I32(ci.highlighted);
  w.U32(static_cast<uint32_t>(ci.candies.size()));
  for (const auto& c : ci.candies) PutText(w, c);
  w.U32(static_cast<uint32_t>(ci.comments.size()));
  for (const auto& c : ci.comments) PutText(w, c);
  w.U32(static_cast<uint32_t>(ci.labels.size()));
  for (const auto& c : ci.labels) PutText(w, c);
}

bool GetCandidateInfo(Reader& r, bangke::CandidateInfo& ci) {
  ci.currentPage = r.I32();
  ci.is_last_page = r.Bool();
  ci.totalPages = r.I32();
  ci.highlighted = r.I32();
  // 与 PutCandidateInfo 严格同序:计数与数据交错(曾因三计数连读导致流错位)
  uint32_t n = r.U32();
  if (!r.Ok() || n > 1024)
    return false;
  ci.candies.resize(n);
  for (uint32_t i = 0; i < n; ++i)
    if (!GetText(r, ci.candies[i])) return false;
  n = r.U32();
  if (!r.Ok() || n > 1024)
    return false;
  ci.comments.resize(n);
  for (uint32_t i = 0; i < n; ++i)
    if (!GetText(r, ci.comments[i])) return false;
  n = r.U32();
  if (!r.Ok() || n > 1024)
    return false;
  ci.labels.resize(n);
  for (uint32_t i = 0; i < n; ++i)
    if (!GetText(r, ci.labels[i])) return false;
  return r.Ok();
}

void PutContext(Writer& w, const bangke::Context& ctx) {
  PutText(w, ctx.preedit);
  PutText(w, ctx.aux);
  PutCandidateInfo(w, ctx.cinfo);
}

bool GetContext(Reader& r, bangke::Context& ctx) {
  return GetText(r, ctx.preedit) && GetText(r, ctx.aux) &&
         GetCandidateInfo(r, ctx.cinfo);
}

void PutStatus(Writer& w, const bangke::Status& s) {
  w.WStr(s.schema_name);
  w.WStr(s.schema_id);
  w.Bool(s.ascii_mode);
  w.Bool(s.composing);
  w.Bool(s.disabled);
  w.Bool(s.full_shape);
  w.I32(static_cast<int32_t>(s.type));
}

bool GetStatus(Reader& r, bangke::Status& s) {
  s.schema_name = r.WStr();
  s.schema_id = r.WStr();
  s.ascii_mode = r.Bool();
  s.composing = r.Bool();
  s.disabled = r.Bool();
  s.full_shape = r.Bool();
  s.type = static_cast<bangke::IconType>(r.I32());
  return r.Ok();
}

void PutConfig(Writer& w, const bangke::Config& c) {
  w.Bool(c.inline_preedit);
}

bool GetConfig(Reader& r, bangke::Config& c) {
  c.inline_preedit = r.Bool();
  return r.Ok();
}

void PutUIStyle(Writer& w, const bangke::UIStyle& s) {
  w.WStr(s.font_face);
  w.WStr(s.label_font_face);
  w.WStr(s.comment_font_face);
  w.I32(static_cast<int32_t>(s.hover_type));
  w.I32(s.font_point);
  w.I32(s.label_font_point);
  w.I32(s.comment_font_point);
  w.I32(s.candidate_abbreviate_length);
  w.Bool(s.inline_preedit);
  w.I32(static_cast<int32_t>(s.align_type));
  w.I32(static_cast<int32_t>(s.antialias_mode));
  w.WStr(s.mark_text);
  w.I32(static_cast<int32_t>(s.preedit_type));
  w.Bool(s.display_tray_icon);
  w.Bool(s.ascii_tip_follow_cursor);
  w.WStr(s.current_zhung_icon);
  w.WStr(s.current_ascii_icon);
  w.WStr(s.current_half_icon);
  w.WStr(s.current_full_icon);
  w.Bool(s.enhanced_position);
  w.Bool(s.click_to_capture);
  w.WStr(s.label_text_format);
  w.I32(static_cast<int32_t>(s.layout_type));
  w.Bool(s.vertical_text_left_to_right);
  w.Bool(s.vertical_text_with_wrap);
  w.Bool(s.paging_on_scroll);
  w.I32(s.min_width);
  w.I32(s.max_width);
  w.I32(s.min_height);
  w.I32(s.max_height);
  w.I32(s.border);
  w.I32(s.margin_x);
  w.I32(s.margin_y);
  w.I32(s.spacing);
  w.I32(s.candidate_spacing);
  w.I32(s.hilite_spacing);
  w.I32(s.hilite_padding_x);
  w.I32(s.hilite_padding_y);
  w.I32(s.round_corner);
  w.I32(s.round_corner_ex);
  w.I32(s.shadow_radius);
  w.I32(s.shadow_offset_x);
  w.I32(s.shadow_offset_y);
  w.Bool(s.vertical_auto_reverse);
  w.I32(s.text_color);
  w.I32(s.candidate_text_color);
  w.I32(s.candidate_back_color);
  w.I32(s.candidate_shadow_color);
  w.I32(s.candidate_border_color);
  w.I32(s.label_text_color);
  w.I32(s.comment_text_color);
  w.I32(s.back_color);
  w.I32(s.shadow_color);
  w.I32(s.border_color);
  w.I32(s.hilited_text_color);
  w.I32(s.hilited_back_color);
  w.I32(s.hilited_shadow_color);
  w.I32(s.hilited_candidate_text_color);
  w.I32(s.hilited_candidate_back_color);
  w.I32(s.hilited_candidate_shadow_color);
  w.I32(s.hilited_candidate_border_color);
  w.I32(s.hilited_label_text_color);
  w.I32(s.hilited_comment_text_color);
  w.I32(s.hilited_mark_color);
  w.I32(s.prevpage_color);
  w.I32(s.nextpage_color);
  w.I32(s.client_caps);
  w.I32(s.baseline);
  w.I32(s.linespacing);
  w.I32(s.window_alpha);
}

bool GetUIStyle(Reader& r, bangke::UIStyle& s) {
  s.font_face = r.WStr();
  s.label_font_face = r.WStr();
  s.comment_font_face = r.WStr();
  s.hover_type = static_cast<bangke::UIStyle::HoverType>(r.I32());
  s.font_point = r.I32();
  s.label_font_point = r.I32();
  s.comment_font_point = r.I32();
  s.candidate_abbreviate_length = r.I32();
  s.inline_preedit = r.Bool();
  s.align_type = static_cast<bangke::UIStyle::LayoutAlignType>(r.I32());
  s.antialias_mode = static_cast<bangke::UIStyle::AntiAliasMode>(r.I32());
  s.mark_text = r.WStr();
  s.preedit_type = static_cast<bangke::UIStyle::PreeditType>(r.I32());
  s.display_tray_icon = r.Bool();
  s.ascii_tip_follow_cursor = r.Bool();
  s.current_zhung_icon = r.WStr();
  s.current_ascii_icon = r.WStr();
  s.current_half_icon = r.WStr();
  s.current_full_icon = r.WStr();
  s.enhanced_position = r.Bool();
  s.click_to_capture = r.Bool();
  s.label_text_format = r.WStr();
  s.layout_type = static_cast<bangke::UIStyle::LayoutType>(r.I32());
  s.vertical_text_left_to_right = r.Bool();
  s.vertical_text_with_wrap = r.Bool();
  s.paging_on_scroll = r.Bool();
  s.min_width = r.I32();
  s.max_width = r.I32();
  s.min_height = r.I32();
  s.max_height = r.I32();
  s.border = r.I32();
  s.margin_x = r.I32();
  s.margin_y = r.I32();
  s.spacing = r.I32();
  s.candidate_spacing = r.I32();
  s.hilite_spacing = r.I32();
  s.hilite_padding_x = r.I32();
  s.hilite_padding_y = r.I32();
  s.round_corner = r.I32();
  s.round_corner_ex = r.I32();
  s.shadow_radius = r.I32();
  s.shadow_offset_x = r.I32();
  s.shadow_offset_y = r.I32();
  s.vertical_auto_reverse = r.Bool();
  s.text_color = r.I32();
  s.candidate_text_color = r.I32();
  s.candidate_back_color = r.I32();
  s.candidate_shadow_color = r.I32();
  s.candidate_border_color = r.I32();
  s.label_text_color = r.I32();
  s.comment_text_color = r.I32();
  s.back_color = r.I32();
  s.shadow_color = r.I32();
  s.border_color = r.I32();
  s.hilited_text_color = r.I32();
  s.hilited_back_color = r.I32();
  s.hilited_shadow_color = r.I32();
  s.hilited_candidate_text_color = r.I32();
  s.hilited_candidate_back_color = r.I32();
  s.hilited_candidate_shadow_color = r.I32();
  s.hilited_candidate_border_color = r.I32();
  s.hilited_label_text_color = r.I32();
  s.hilited_comment_text_color = r.I32();
  s.hilited_mark_color = r.I32();
  s.prevpage_color = r.I32();
  s.nextpage_color = r.I32();
  s.client_caps = r.I32();
  s.baseline = r.I32();
  s.linespacing = r.I32();
  s.window_alpha = r.I32();
  return r.Ok();
}

std::vector<uint8_t> BuildFrame(uint32_t ipc_sid,
                                uint32_t key_serial,
                                const std::wstring* commit,
                                const bangke::Context* ctx,
                                const bangke::Status* status,
                                const bangke::Config* config,
                                const bangke::UIStyle* style) {
  uint16_t flags = 0;
  if (commit) flags |= SNAP_HAS_COMMIT;
  if (ctx) flags |= SNAP_CTX;
  if (status) flags |= SNAP_STATUS;
  if (config) flags |= SNAP_CONFIG;
  if (style) flags |= SNAP_STYLE;

  Writer w;
  if (commit) w.WStr(*commit);
  if (ctx) PutContext(w, *ctx);
  if (status) PutStatus(w, *status);
  if (config) PutConfig(w, *config);
  if (style) PutUIStyle(w, *style);

  FrameHeader h{kFrameMagic, kProtoVersion, flags, ipc_sid, key_serial,
                static_cast<uint32_t>(w.buf.size())};
  std::vector<uint8_t> out(sizeof(h) + w.buf.size());
  std::memcpy(out.data(), &h, sizeof(h));
  std::memcpy(out.data() + sizeof(h), w.buf.data(), w.buf.size());
  return out;
}

bool ParseFrame(const uint8_t* data,
                size_t len,
                FrameHeader* hdr,
                std::wstring* commit,
                bangke::Context* ctx,
                bangke::Status* status,
                bangke::Config* config,
                bangke::UIStyle* style) {
  // 严格版:载荷长度必须与 len 精确一致(用于已知边界的场景)
  return ParseFramePrefix(data, len, hdr, commit, ctx, status, config, style) &&
         hdr && hdr->payload_len + sizeof(FrameHeader) == len;
}

bool ParseFramePrefix(const uint8_t* data,
                      size_t cap,
                      FrameHeader* hdr,
                      std::wstring* commit,
                      bangke::Context* ctx,
                      bangke::Status* status,
                      bangke::Config* config,
                      bangke::UIStyle* style) {
  if (cap < sizeof(FrameHeader))
    return false;
  FrameHeader h;
  std::memcpy(&h, data, sizeof(h));
  if (hdr)
    *hdr = h;
  if (h.magic != kFrameMagic || h.version != kProtoVersion)
    return false;
  if (h.payload_len > cap - sizeof(FrameHeader))
    return false;

  Reader r(data + sizeof(FrameHeader), h.payload_len);
  // 即使调用方不要某字段,也必须消费其字节——否则后续字段错位
  std::wstring tmpCommit;
  bangke::Context tmpCtx;
  bangke::Status tmpStatus;
  bangke::Config tmpConfig;
  bangke::UIStyle tmpStyle;
  if (h.flags & SNAP_HAS_COMMIT)
    *(commit ? commit : &tmpCommit) = r.WStr();
  if ((h.flags & SNAP_CTX) && !GetContext(r, ctx ? *ctx : tmpCtx))
    return false;
  if ((h.flags & SNAP_STATUS) && !GetStatus(r, status ? *status : tmpStatus))
    return false;
  if ((h.flags & SNAP_CONFIG) && !GetConfig(r, config ? *config : tmpConfig))
    return false;
  if ((h.flags & SNAP_STYLE) && !GetUIStyle(r, style ? *style : tmpStyle))
    return false;
  return r.Ok();
}

}  // namespace bangke
