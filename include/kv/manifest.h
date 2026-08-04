#pragma once

#include <string>
#include <vector>

namespace kv {

class Manifest {
public:
  bool open(const std::string& path);

  bool append_sstable(const std::string& sstable_path);

  std::vector<std::string> load_sstables() const;

  bool replace_sstables(
    const std::vector<std::string>& sstable_paths);

private:
  std::string path_;
};

} // namespace kv