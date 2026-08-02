#include "kv/sstable_index.h"

#include <fstream>
#include <sstream>

namespace kv {

SSTableIndex::SSTableIndex(std::size_t stride)
    : stride_(stride == 0 ? 1 : stride) {}

bool SSTableIndex::build(
    const std::string& sstable_path) {
  std::ifstream in(sstable_path, std::ios::binary);

  if (!in) {
    return false;
  }

  index_.clear();

  std::size_t record_number = 0;
  std::string line;

  while (true) {
    const std::streampos offset = in.tellg();

    if (!std::getline(in, line)) {
      break;
    }

    std::istringstream row(line);
    std::string key;

    if (!(row >> key)) {
      continue;
    }

    if ((record_number % stride_) == 0) {
      index_[key] = offset;
    }

    ++record_number;
  }

  return true;
}

std::optional<std::streampos>
SSTableIndex::find_start_offset(
    const std::string& key) const {
  if (index_.empty()) {
    return std::nullopt;
  }

  auto it = index_.upper_bound(key);

  if (it == index_.begin()) {
    return it->second;
  }

  --it;
  return it->second;
}

const std::map<std::string, std::streampos>&
SSTableIndex::entries() const {
  return index_;
}

} // namespace kv