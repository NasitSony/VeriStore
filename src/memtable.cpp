#include "kv/memtable.h"
#include <mutex>

#include <utility>

namespace kv {

void MemTable::put(const std::string& key,
                   Timestamp timestamp,
                   const std::string& value) {
  std::unique_lock lock(mu_);

  approximate_size_bytes_ +=
      key.size() +
      value.size() +
      sizeof(Timestamp) +
      sizeof(Version);

  entries_[key].push_back(
      Version{timestamp, value}
  );
}

void MemTable::del(const std::string& key,
                   Timestamp timestamp) {
  std::unique_lock lock(mu_);

  approximate_size_bytes_ +=
      key.size() +
      sizeof(Timestamp) +
      sizeof(Version);

  entries_[key].push_back(
      Version{timestamp, std::nullopt}
  );
}
size_t MemTable::approximate_size_bytes() const {
  std::shared_lock lock(mu_);
  return approximate_size_bytes_;
}

bool MemTable::empty() const {
  std::shared_lock lock(mu_);
  return entries_.empty();
}


LookupResult
MemTable::get_at(const std::string& key,
                 Timestamp read_timestamp) const {
  std::shared_lock lock(mu_);

  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return LookupResult::not_found();
  }

  const auto& versions = it->second;

  for (auto version_it = versions.rbegin();
       version_it != versions.rend();
       ++version_it) {
    if (version_it->timestamp <= read_timestamp) {
      if (version_it->is_tombstone()) {
        return LookupResult::tombstone();
      }

      return LookupResult::found_value(
          *version_it->value
      );
    }
  }

  return LookupResult::not_found();
}

std::size_t MemTable::key_count() const {
  std::shared_lock lock(mu_);
  return entries_.size();
}

void MemTable::clear() {
  std::unique_lock lock(mu_);

  entries_.clear();
  approximate_size_bytes_ = 0;
}

MemTable::Entries MemTable::snapshot_entries() const {
  std::shared_lock lock(mu_);
  return entries_;
}

} // namespace kv