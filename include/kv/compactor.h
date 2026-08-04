#pragma once

#include "kv/memtable.h"

#include <string>
#include <vector>

namespace kv {

class Compactor {
public:
  static bool compact(
      const std::vector<std::string>& input_paths,
      const std::string& output_path);

private:
  static bool load_sstable(
      const std::string& path,
      MemTable::Entries& merged);
};

} // namespace kv