#pragma once
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <vector>

#include "kv/wal.h"
#include "kv/raft_sm.h"
#include "kv/version.h"
#include "kv/memtable.h"
#include "kv/sstable_reader.h"
#include "kv/lookup_result.h"
#include "kv/sstable_reader.h"
#include "kv/manifest.h"

#include <utility>


namespace kv {


class KVStore : public IRaftStateMachine{
public:
  // v0.2: must be called before PUT/DEL for WAL + recovery
  bool open(const std::string& wal_path);

  void put(std::string key, std::string value);
  std::optional<std::string> get(const std::string& key) const;

  std::optional<std::string> get_at(
      const std::string& key,
      Timestamp read_timestamp
  ) const;

  bool del(const std::string& key);
  std::size_t size() const;

  bool save_to_file(const std::string& path) const;
  bool load_from_file(const std::string& path);

  bool save_snapshot(const std::string& path);  
  bool load_snapshot(const std::string& path);
  bool checkpoint(const std::string& snapshot_path,
                const std::string& wal_path);
  
  bool flush_wal();

  // v0.7 prefix scan API
  std::vector<std::string>
  list_keys_with_prefix(const std::string& prefix) const;


  void set_group_commit_every(int n) ;

  // Raft state machine apply (must NOT append to WAL)
  void ApplyPut(std::string key, std::string value) override;
  void ApplyDel(const std::string& key) override;

private:
  friend class Wal;

  bool load_from_file_unlocked(const std::string& path);
  bool save_to_file_unlocked(const std::string& path) const;

  bool maybe_flush_memtable_unlocked();

  mutable std::shared_mutex mu_;

  std::unordered_map<std::string, std::string> map_;
  MemTable memtable_;

  std::vector<std::string> sstable_paths_;
  uint64_t next_sstable_id_{0};

  Manifest manifest_;
  std::string manifest_path_{"/tmp/veristore-MANIFEST"};

  static constexpr std::size_t kMemTableFlushThresholdBytes =
      4 * 1024 * 1024;

  Wal wal_;
  uint64_t seq_{0};
  int group_commit_every_{5};
  bool opened_{false};

};

} // namespace kv