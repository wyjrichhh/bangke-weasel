#include "BangkeColorOpacity.h"
#include <cstdio>
#include <limits>
int main() {
  struct Case { double opacity; unsigned expected; };
  const Case cases[] = {{0.6, 153}, {0.0, 0}, {1.0, 255}, {0.5, 128},
                        {-1.0, 0}, {2.0, 255},
                        {std::numeric_limits<double>::quiet_NaN(), 255}};
  for (const auto& c : cases) {
    if (bangke::WindowOpacity(c.opacity) != c.expected) {
      std::fprintf(stderr, "Window opacity regression\n");
      return 1;
    }
  }
  return 0;
}
