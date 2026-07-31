#include "kv/sstable.h"

#include <fstream>

namespace kv {

bool SSTableWriter::write(
    const std::string& path,
    const MemTable::Entries& entries) {
  std::ofstream out(path, std::ios::binary);

  if (!out) {
    return false;
  }

  for (const auto& [key, versions] : entries) {
    for (const auto& version : versions) {
      out << key << '\t'
          << version.timestamp << '\t';

      if (version.value.has_value()) {
        out << 'P' << '\t'
            << *version.value;
      } else {
        out << 'D';
      }

      out << '\n';

      if (!out) {
        return false;
      }
    }
  }

  out.flush();
  return static_cast<bool>(out);
}

} // namespace kv