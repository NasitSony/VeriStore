#pragma once

#include "kv/memtable.h"

#include <string>

namespace kv {

class SSTableWriter {
public:
  static bool write(
      const std::string& path,
      const MemTable::Entries& entries);
};

} // namespace kv