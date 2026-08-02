#pragma once

#include "kv/version.h"
#include "kv/lookup_result.h"
#include "kv/sstable_index.h"


#include <optional>
#include <string>

namespace kv {

class SSTableReader {
public:
  explicit SSTableReader(
    std::string path,
    std::size_t index_stride = 4);

  LookupResult
  get_at(const std::string& key,
         Timestamp read_timestamp) const;

private:
  std::string path_;
  SSTableIndex index_;
  bool index_ready_{false};
};

} // namespace kv