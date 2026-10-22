#pragma once

#include <array>
#include <charconv>
#include <optional>
#include <string_view>

namespace bangke::installer {
using Version = std::array<unsigned, 4>;

// UI compares all four displayed fields (MSI upgrade matching uses only three).
// Reject malformed input rather than
// accidentally presenting an unknown version as a valid downgrade.
inline std::optional<Version> ParseVersion(std::string_view text) {
  Version version{};
  if (text.empty()) return std::nullopt;
  for (size_t i = 0; i < version.size(); ++i) {
    const auto dot = text.find('.');
    const auto part = text.substr(0, dot);
    if (part.empty()) return std::nullopt;
    const auto result = std::from_chars(part.data(), part.data() + part.size(), version[i]);
    if (result.ec != std::errc{} || result.ptr != part.data() + part.size()) return std::nullopt;
    if (dot == std::string_view::npos) return version;
    text.remove_prefix(dot + 1);
  }
  return std::nullopt;
}

// Extract action starts, not arbitrary mentions in MSI property dumps.
inline std::optional<std::string_view> ActionName(std::string_view line) {
  if (line.find("Action start ") != 0 && line.find("操作开始 ") != 0) return std::nullopt;
  const auto start = line.rfind(": ");
  if (start == std::string_view::npos) return std::nullopt;
  line.remove_prefix(start + 2);
  auto dot = line.find('.');
  if (dot == std::string_view::npos) dot = line.find("。");
  if (dot == std::string_view::npos || dot == 0) return std::nullopt;
  line = line.substr(0, dot);
  for (const auto c : line)
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '_')) return std::nullopt;
  return line;
}

inline bool RequiresReboot(int code) { return code == 3010 || code == 1641; }
inline bool Succeeded(int code) { return code == 0 || RequiresReboot(code); }

// Installation and removal have distinct sequences. Ignore unknown actions
// and actions from a nested upgrade removal so progress never runs backwards.
inline int StageIndex(std::string_view action, bool uninstall) {
  constexpr std::array install{"InstallValidate", "InstallFiles", "ClearPendingDelete",
      "RegisterTSF", "WriteRegistryValues", "FirstDeploy", "StartServer", "InstallFinalize"};
  constexpr std::array remove{"InstallValidate", "UnregisterTSF", "Cleanup", "RemoveFiles",
      "RemoveRegistryValues", "InstallFinalize"};
  if (uninstall) {
    for (size_t i = 0; i < remove.size(); ++i) if (action == remove[i]) return int(i);
  } else {
    for (size_t i = 0; i < install.size(); ++i) if (action == install[i]) return int(i);
  }
  return -1;
}
// A major upgrade logs the nested old-product uninstall first. Its finalize
// marker must not advance the new installation to its final stage.
inline std::optional<int> AcceptStage(int current, std::string_view action, bool uninstall) {
  const int stage = StageIndex(action, uninstall);
  if (stage < current || stage < 0) return std::nullopt;
  if (!uninstall && action == "InstallFinalize" && current < StageIndex("FirstDeploy", false))
    return std::nullopt;
  return stage;
}
}  // namespace bangke::installer
