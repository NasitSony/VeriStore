#include "kv/memtable.h"
#include <mutex>

#include <utility>

namespace kv {

void MemTable::put(std::string key,
                   Timestamp timestamp,
                   std::string value) {
  std::unique_lock lock(mu_);

  entries_[std::move(key)].push_back(
      Version{timestamp, std::move(value)}
  );
}

void MemTable::del(std::string key,
                   Timestamp timestamp) {
  std::unique_lock lock(mu_);

  entries_[std::move(key)].push_back(
      Version{timestamp, std::nullopt}
  );
}

std::optional<std::string>
MemTable::get_at(const std::string& key,
                 Timestamp read_timestamp) const {
  std::shared_lock lock(mu_);

  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return std::nullopt;
  }

  const auto& versions = it->second;

  for (auto version_it = versions.rbegin();
       version_it != versions.rend();
       ++version_it) {
    if (version_it->timestamp <= read_timestamp) {
      return version_it->value;
    }
  }

  return std::nullopt;
}

std::size_t MemTable::key_count() const {
  std::shared_lock lock(mu_);
  return entries_.size();
}

void MemTable::clear() {
  std::unique_lock lock(mu_);
  entries_.clear();
}

MemTable::Entries MemTable::snapshot_entries() const {
  std::shared_lock lock(mu_);
  return entries_;
}

} // namespace kv