#pragma once
#include <string>

namespace bangke::installer {
// SHFileOperation consumes a double-NUL-terminated list of complete paths.
inline std::wstring SinglePathList(const std::wstring& path) {
  std::wstring list = path;
  list.push_back(L'\0');
  list.push_back(L'\0');
  return list;
}
}  // namespace bangke::installer
