#include "kv/manifest.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

namespace kv {

bool Manifest::open(const std::string& path) {
  path_ = path;

  // Ensure the file exists.
  std::ofstream out(path_, std::ios::app);
  return static_cast<bool>(out);
}

bool Manifest::append_sstable(
    const std::string& sstable_path) {
  if (path_.empty()) {
    return false;
  }

  const auto existing = load_sstables();

  if (std::find(existing.begin(),
                existing.end(),
                sstable_path) != existing.end()) {
    return true;
  }

  std::ofstream out(path_, std::ios::app);
  if (!out) {
    return false;
  }

  out << sstable_path << '\n';
  out.flush();

  return static_cast<bool>(out);
}

std::vector<std::string>
Manifest::load_sstables() const {
  std::vector<std::string> paths;

  if (path_.empty()) {
    return paths;
  }

  std::ifstream in(path_);
  if (!in) {
    return paths;
  }

  std::string line;

  while (std::getline(in, line)) {
    if (!line.empty()) {
      paths.push_back(std::move(line));
    }
  }

  return paths;
}

} // namespace kv