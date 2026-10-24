#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>

namespace bangke {

inline bool TaskbarUsesDarkTheme() {
  DWORD light = 1, size = sizeof(light);
  RegGetValueW(HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
      L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
  return light == 0;
}

// Render at each requested size; do not scale a single 16px resource.
inline HICON CreateStatusIcon(bool ascii, int width, int height) {
  if (width <= 0 || height <= 0 || width > 256 || height > 256)
    return nullptr;
  const bool dark = TaskbarUsesDarkTheme();
  HIGHCONTRASTW contrast{sizeof(contrast)};
  SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
  const bool highContrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
  COLORREF back = ascii ? (dark ? RGB(32,38,46) : RGB(230,224,215))
                        : (dark ? RGB(148,200,188) : RGB(120,83,48));
  COLORREF text = ascii ? (dark ? RGB(173,184,199) : RGB(107,98,87))
                        : (dark ? RGB(16,39,34) : RGB(255,255,255));
  if (highContrast) {
    back = GetSysColor(ascii ? COLOR_WINDOW : COLOR_HIGHLIGHT);
    text = GetSysColor(ascii ? COLOR_WINDOWTEXT : COLOR_HIGHLIGHTTEXT);
  }
  constexpr int scale = 4;
  const int w = width * scale, h = height * scale;
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = w;
  info.bmiHeader.biHeight = -h;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* pixels = nullptr;
  HDC dc = CreateCompatibleDC(nullptr);
  HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
  if (!dc || !bitmap || !pixels) {
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    return nullptr;
  }
  auto oldBitmap = SelectObject(dc, bitmap);
  RECT rect{0, 0, w, h};
  HBRUSH brush = CreateSolidBrush(back);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
  HFONT font = CreateFontW(-h * 78 / 100, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
      FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
      ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
  if (!font) {
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return nullptr;
  }
  auto oldFont = SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, text);
  DrawTextW(dc, ascii ? L"英" : L"中", 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  GdiFlush();
  std::vector<std::uint32_t> output(width * height);
  auto* source = static_cast<std::uint32_t*>(pixels);
  const double radius = (w < h ? w : h) * 0.22;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      unsigned a = 0, red = 0, green = 0, blue = 0;
      for (int sy = 0; sy < scale; ++sy) {
        for (int sx = 0; sx < scale; ++sx) {
          const int px = x * scale + sx, py = y * scale + sy;
          const double fx = px + 0.5, fy = py + 0.5;
          const double dx = fx < radius ? radius - fx : (fx > w-radius ? fx-(w-radius) : 0);
          const double dy = fy < radius ? radius - fy : (fy > h-radius ? fy-(h-radius) : 0);
          if (dx*dx + dy*dy > radius*radius) continue;
          const auto color = source[py*w+px];
          a += 255;
          red += (color >> 16) & 255;
          green += (color >> 8) & 255;
          blue += color & 255;
        }
      }
      // CreateIconIndirect expects premultiplied alpha for the color bitmap.
      output[y*width+x] = ((a/16)<<24) | ((red/16)<<16) | ((green/16)<<8) | (blue/16);
    }
  }
  SelectObject(dc, oldFont);
  DeleteObject(font);
  SelectObject(dc, oldBitmap);
  DeleteObject(bitmap);
  DeleteDC(dc);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  void* iconPixels = nullptr;
  HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &iconPixels, nullptr, 0);
  const int maskStride = ((width + 15) / 16) * 2;
  std::vector<unsigned char> maskBits(maskStride * height, 0);
  HBITMAP mask = CreateBitmap(width, height, 1, 1, maskBits.data());
  HICON result = nullptr;
  if (color && iconPixels && mask) {
    memcpy(iconPixels, output.data(), output.size() * sizeof(output[0]));
    ICONINFO icon{TRUE, 0, 0, mask, color};
    result = CreateIconIndirect(&icon);
  }
  if (color) DeleteObject(color);
  if (mask) DeleteObject(mask);
  return result;
}

}  // namespace bangke
