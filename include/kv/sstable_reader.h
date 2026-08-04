#pragma once

#include "kv/version.h"
#include "kv/lookup_result.h"
#include "kv/sstable_index.h"
#include "kv/bloom_filter.h"



#include <optional>
#include <string>

namespace kv {

class SSTableReader {
public:
  explicit SSTableReader(
    std::string path,
    std::size_t index_stride = 4,
    std::size_t bloom_bits = 8192,
    std::size_t bloom_hashes = 4);

  LookupResult
  get_at(const std::string& key,
         Timestamp read_timestamp) const;

private:
  std::string path_;
  SSTableIndex index_;
  BloomFilter bloom_filter_;
  bool bloom_ready_{false};
  bool index_ready_{false};
  bool build_bloom_filter();
};

} // namespace kv