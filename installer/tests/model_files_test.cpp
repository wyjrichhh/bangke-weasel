#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../include/BangkeModelFiles.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

int main() {
  namespace fs = std::filesystem;
  const auto root = fs::temp_directory_path() / ("bangke-model-test-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto source = root / "source", target = root / "target";
  fs::create_directories(source / "nested"); fs::create_directories(target);
  std::ofstream(source / "model.bin") << "bundled model";
  std::ofstream(source / "tokenizer.json") << "{}";
  std::ofstream(source / "nested/vocabulary.json") << "{}";
  std::ofstream(target / "model.bin") << "customized model";
  std::ofstream(target / "tokenizer.json"); // Interrupted copy left an empty file.
  assert(!bangke::StageMissingModelFiles(source, target));
  assert(fs::file_size(target / "model.bin") == std::string("customized model").size());
  assert(fs::file_size(target / "tokenizer.json") == 2);
  assert(fs::file_size(target / "nested/vocabulary.json") == 2);
  const auto time = fs::last_write_time(target / "model.bin");
  assert(!bangke::StageMissingModelFiles(source, target));
  assert(fs::last_write_time(target / "model.bin") == time);
  assert(bangke::StageMissingModelFiles(root / "missing", target));
  fs::remove_all(root);
  std::cout << "PASS partial model repair, nested files, idempotence and preservation\n";
}
