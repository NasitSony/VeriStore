#pragma once

#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "kv/version.h"

namespace kv {

class MemTable {
public:
  
  using Entries =
    std::map<std::string, std::vector<Version>>;

Entries snapshot_entries() const;

  void put(std::string key,
           Timestamp timestamp,
           std::string value);

  void del(std::string key,
           Timestamp timestamp);

  std::optional<std::string>
  get_at(const std::string& key,
         Timestamp read_timestamp) const;

  std::size_t key_count() const;

  void clear();

private:
  mutable std::shared_mutex mu_;

  Entries entries_;
};

} // namespace kv