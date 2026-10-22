#pragma once
#include <filesystem>
#include <system_error>

namespace bangke {
// Repair incomplete first-run copies without overwriting nonempty user files.
inline std::error_code StageMissingModelFiles(const std::filesystem::path& source,
                                             const std::filesystem::path& target) {
  namespace fs = std::filesystem;
  std::error_code error;
  if (!fs::is_directory(source, error))
    return error ? error : std::make_error_code(std::errc::no_such_file_or_directory);
  fs::create_directories(target, error);
  if (error) return error;
  fs::recursive_directory_iterator iterator(source, error), end;
  while (!error && iterator != end) {
    const auto entry = *iterator;
    const auto destination = target / entry.path().lexically_relative(source);
    if (entry.is_directory(error)) {
      fs::create_directories(destination, error);
    } else if (!error && entry.is_regular_file(error)) {
      const bool exists = fs::exists(destination, error);
      if (!error && (!exists || fs::file_size(destination, error) == 0))
        fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, error);
    }
    if (!error) iterator.increment(error);
  }
  return error;
}
}  // namespace bangke
