#pragma once

#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include <cstddef>

#include "kv/version.h"
#include "kv/lookup_result.h"

namespace kv {

class MemTable {
public:
  
  using Entries =
      std::map<std::string, std::vector<Version>>;

  void put(const std::string& key,
           Timestamp timestamp,
           const std::string& value);

  void del(const std::string& key,
           Timestamp timestamp);

  LookupResult get_at(
    const std::string& key,
    Timestamp read_timestamp) const;
  

  size_t key_count() const;

  size_t approximate_size_bytes() const;

  bool empty() const;

  void clear();

  Entries snapshot_entries() const;

  Entries take_entries();
  

private:
  mutable std::shared_mutex mu_;
  Entries entries_;

  

  // Add it here
  size_t approximate_size_bytes_{0};

};

} // namespace kv