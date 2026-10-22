#pragma once

#include <cmath>
#include <cstdint>

namespace bangke {
// macOS alphaValue maps to ULW's one opacity for the fully composed window.
inline std::uint8_t WindowOpacity(double opacity) {
  if (!std::isfinite(opacity))
    opacity = 1.0;
  if (opacity < 0.0)
    opacity = 0.0;
  if (opacity > 1.0)
    opacity = 1.0;
  return static_cast<std::uint8_t>(std::lround(opacity * 255.0));
}
}  // namespace bangke
