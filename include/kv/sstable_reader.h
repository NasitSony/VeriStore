#pragma once

#include "kv/bloom_filter.h"
#include "kv/lookup_result.h"
#include "kv/sstable_index.h"
#include "kv/version.h"

#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>

namespace kv {

class SSTableReader {
public:
  explicit SSTableReader(
      std::string path,
      std::size_t index_stride = 4,
      std::size_t bloom_bits = 8192,
      std::size_t bloom_hashes = 4);

  SSTableReader(const SSTableReader&) = delete;
  SSTableReader& operator=(const SSTableReader&) = delete;

  LookupResult get_at(
      const std::string& key,
      Timestamp read_timestamp) const;

  bool is_open() const noexcept;

  bool possibly_contains(
    const std::string& key) const;

private:
  bool build_bloom_filter();

  std::string path_;

  SSTableIndex index_;
  BloomFilter bloom_filter_;

  bool index_ready_{false};
  bool bloom_ready_{false};
  bool file_ready_{false};

  // get_at() is const, but reading changes the stream cursor and flags.
  mutable std::ifstream file_;

  // Protects clear(), seekg(), and getline() when one reader is shared
  // across multiple threads.
  mutable std::mutex file_mu_;
};

} // namespace kv