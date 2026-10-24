#include "../../BangkeTSF/StatusIcon.h"
#include <cstdlib>
#include <iostream>

static void Require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

static std::vector<std::uint32_t> ReadIcon(HICON icon, int size) {
  ICONINFO info{};
  Require(GetIconInfo(icon, &info) != FALSE, "GetIconInfo failed");
  BITMAP bitmap{};
  Require(GetObjectW(info.hbmColor, sizeof(bitmap), &bitmap) != 0, "GetObject failed");
  Require(bitmap.bmWidth == size && bitmap.bmHeight == size, "Incorrect icon size");
  BITMAPINFO format{};
  format.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  format.bmiHeader.biWidth = size;
  format.bmiHeader.biHeight = -size;
  format.bmiHeader.biPlanes = 1;
  format.bmiHeader.biBitCount = 32;
  format.bmiHeader.biCompression = BI_RGB;
  std::vector<std::uint32_t> pixels(size * size);
  HDC dc = CreateCompatibleDC(nullptr);
  Require(GetDIBits(dc, info.hbmColor, 0, size, pixels.data(), &format, DIB_RGB_COLORS) == size,
          "GetDIBits failed");
  DeleteDC(dc);
  DeleteObject(info.hbmColor);
  DeleteObject(info.hbmMask);
  return pixels;
}

int main() {
  for (const int size : {16, 20, 24, 32, 48}) {
    HICON chinese = bangke::CreateStatusIcon(false, size, size);
    HICON english = bangke::CreateStatusIcon(true, size, size);
    Require(chinese && english, "Icon creation failed");
    const auto zh = ReadIcon(chinese, size);
    const auto en = ReadIcon(english, size);
    Require(zh != en, "Language states are indistinguishable");
    Require((zh.front() >> 24) == 0, "Rounded corner must be transparent");
    Require((zh[(size / 2) * size + size / 2] >> 24) == 255, "Center must be opaque");
    for (auto pixel : zh) {
      const auto alpha = pixel >> 24;
      Require(((pixel >> 16) & 255) <= alpha && ((pixel >> 8) & 255) <= alpha &&
              (pixel & 255) <= alpha, "Icon alpha must be premultiplied");
    }
    DestroyIcon(chinese);
    DestroyIcon(english);
  }
  const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  for (int i = 0; i < 200; ++i) {
    HICON icon = bangke::CreateStatusIcon(i % 2 != 0, 16, 16);
    Require(icon != nullptr, "Repeated icon creation failed");
    DestroyIcon(icon);
  }
  Require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before,
          "Icon creation leaked GDI objects");
  Require(!bangke::CreateStatusIcon(false, 0, 16), "Invalid size accepted");
  std::cout << "PASS: icon sizes, language states, alpha, and GDI lifetime\n";
}
