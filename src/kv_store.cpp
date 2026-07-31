#include <mutex>
#include "kv/kv_store.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>

#include "kv/sstable.h"
#include "kv/sstable_reader.h"


namespace kv {

/*bool KVStore::open(const std::string& wal_path) {
  std::unique_lock lock(mu_);
  if (opened_) return true;

  if (!wal_.open(wal_path)) return false;

  // Replay WAL into map_ (no locks inside replay; we already hold mu_)
  // But replay_into calls apply_* which touches map_. We are holding mu_ here -> safe.
  uint64_t max_seq = 0;
  bool ok = wal_.replay_into(*this, max_seq);
  if (!ok) return false;

  seq_ = max_seq;
  opened_ = true;

  // Optional cleanup: remove corrupted tail
  // wal_.truncate_to_last_good();

  return true;
}*/

//int group_commit_every_ = 5;   // default


bool KVStore::open(const std::string& wal_path) {
  std::unique_lock lock(mu_);
  //std::cerr << "[open] start\n";

  if (opened_) return true;

  //std::cerr << "[open] load snapshot\n";
  (void)load_from_file_unlocked("/tmp/kv.snapshot"); // <-- NO DEADLOCK

  //std::cerr << "[open] wal open: " << wal_path << "\n";
  if (!wal_.open(wal_path)) return false;

  //std::cerr << "[open] wal replay\n";
  uint64_t max_seq = 0;
  if (!wal_.replay_into(*this, max_seq)) return false;

  seq_ = max_seq;
  opened_ = true;
  std::cerr << "[open] done (seq=" << seq_ << ")\n";
  //std::cerr << "[open] map size after replay = " << map_.size() << "\n";
  return true;
}

void KVStore::set_group_commit_every(int n) {
  std::unique_lock lock(mu_);
  group_commit_every_ = (n <= 0) ? 1 : n;
}


bool KVStore::maybe_flush_memtable_unlocked() {
  if (memtable_.approximate_size_bytes() <
      kMemTableFlushThresholdBytes) {
    return true;
  }

  const auto entries = memtable_.snapshot_entries();

  if (entries.empty()) {
    return true;
  }

  const std::string path =
      "/tmp/veristore-" +
      std::to_string(next_sstable_id_) +
      ".sst";

  if (!SSTableWriter::write(path, entries)) {
    std::cerr
        << "[lsm] failed to flush MemTable to "
        << path << '\n';

    return false;
  }

  sstable_paths_.push_back(path);
  ++next_sstable_id_;

  memtable_.clear();

  std::cout
      << "[lsm] flushed MemTable to "
      << path << '\n';

  return true;
}
/*void KVStore::put(std::string key, std::string value) {
  std::unique_lock lock(mu_);
  if (!opened_) return; // or throw

  uint64_t s = ++seq_;
  if (!wal_.append_put(s, key, value))  return;

  if ((s % 5) == 0) {
     if (!wal_.flush()) return; // better than ignoring
  
     map_[std::move(key)] = std::move(value);
  }
  }*/

 void KVStore::put(std::string key, std::string value) {
  std::unique_lock lock(mu_);
  if (!opened_) return;

  const Timestamp s = ++seq_;

  // WAL first.
  if (!wal_.append_put(s, key, value)) return;

  // Latest-value view.
  map_[key] = value;

  // MVCC history: oldest to newest.
  /*versions_[key].push_back(
      Version{s, value}
  );*/

  memtable_.put(key, s, value);

  if (!maybe_flush_memtable_unlocked()) {
    return;
  }

  /*if (memtable_.approximate_size_bytes() >=
        kMemTableFlushThresholdBytes) {
        std::cout << "[memtable] flush threshold reached\n";
    }*/

  // Periodic durability boundary.
  if ((s % group_commit_every_) == 0) {
    if (!wal_.flush()) return;
  }
}

std::optional<std::string> KVStore::get(const std::string& key) const {
  std::shared_lock lock(mu_);
  auto it = map_.find(key);
  if (it == map_.end()) return std::nullopt;
  return it->second;
}

std::optional<std::string>
KVStore::get_at(const std::string& key,
                Timestamp read_timestamp) const {
  const LookupResult mem_result =
      memtable_.get_at(key, read_timestamp);

  if (mem_result.state == LookupState::Value) {
    return mem_result.value;
  }

  if (mem_result.state == LookupState::Tombstone) {
    return std::nullopt;
  }

  for (auto it = sstable_paths_.rbegin();
       it != sstable_paths_.rend();
       ++it) {
    SSTableReader reader(*it);

    const LookupResult result =
        reader.get_at(key, read_timestamp);

    if (result.state == LookupState::Value) {
      return result.value;
    }

    if (result.state == LookupState::Tombstone) {
      return std::nullopt;
    }
  }

  return std::nullopt;
}

bool KVStore::del(const std::string& key) {
  std::unique_lock lock(mu_);

  if (!opened_) {
    return false;
  }

  auto current = map_.find(key);
  if (current == map_.end()) {
    return false;
  }

  const Timestamp s = ++seq_;

  if (!wal_.append_del(s, key)) {
    return false;
  }

  memtable_.del(key, s);
  map_.erase(current);

  if (!maybe_flush_memtable_unlocked()) {
    return false;
  }

  if ((s % group_commit_every_) == 0) {
    if (!wal_.flush()) {
      return false;
    }
  }

  return true;
}

std::size_t KVStore::size() const {
  std::shared_lock lock(mu_);
  return map_.size();
}

bool KVStore::save_snapshot(const std::string& path) {
  std::shared_lock lock(mu_);

  std::string tmp = path + ".tmp";

  std::ofstream out(tmp);
  if (!out) return false;

  for (const auto& [k, v] : map_) {
    out << k << '\t' << v << '\n';
  }

  out.close();

  // atomic replace
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    return false;
  }

  return true;
}

bool KVStore::load_snapshot(const std::string& path) {
  std::unique_lock lock(mu_);

  std::ifstream in(path);
  if (!in) return false;

  map_.clear();

  std::string k, v;
  while (in >> k >> v) {
    map_[k] = v;
  }

  return true;
}

// Keep snapshot utilities if you want, but note:
// v0.2 correctness is via WAL; snapshot is optional.
bool KVStore::load_from_file(const std::string& path) {
  std::unique_lock lock(mu_);
  return load_from_file_unlocked(path);
}

bool KVStore::save_to_file(const std::string& path) const {
  std::shared_lock lock(mu_);
  return save_to_file_unlocked(path);
}



bool KVStore::checkpoint(const std::string& snapshot_path,
                         const std::string& wal_path) {
  std::unique_lock lock(mu_);
  if (!opened_) return false;

  // 1. Save snapshot
  if (!save_to_file_unlocked(snapshot_path)) return false;

  // 2. Rotate WAL
  wal_.close();
  std::remove(wal_path.c_str());

  if (!wal_.open(wal_path)) return false;

  return true;
}

bool KVStore::load_from_file_unlocked(const std::string& path) {
  std::ifstream in(path);
  if (!in) return false;

  map_.clear();
  memtable_.clear();

  std::string line;

  while (std::getline(in, line)) {
    if (line.empty()) continue;

    const std::size_t first_tab = line.find('\t');
    const std::size_t second_tab =
        first_tab == std::string::npos
            ? std::string::npos
            : line.find('\t', first_tab + 1);
    const std::size_t third_tab =
        second_tab == std::string::npos
            ? std::string::npos
            : line.find('\t', second_tab + 1);

    if (first_tab == std::string::npos ||
        second_tab == std::string::npos) {
      return false;
    }

    const std::string key = line.substr(0, first_tab);
    const std::string timestamp_text =
        line.substr(first_tab + 1, second_tab - first_tab - 1);

    const std::string type =
        third_tab == std::string::npos
            ? line.substr(second_tab + 1)
            : line.substr(second_tab + 1,
                          third_tab - second_tab - 1);

    Timestamp timestamp = 0;

    try {
      timestamp = std::stoull(timestamp_text);
    } catch (...) {
      return false;
    }

    if (type == "P") {
      if (third_tab == std::string::npos) {
        return false;
      }

      const std::string value = line.substr(third_tab + 1);

      memtable_.put(key, timestamp, value);
      map_[key] = value;

    } else if (type == "D") {
      memtable_.del(key, timestamp);
      map_.erase(key);

    } else {
      return false;
    }

    seq_ = std::max(seq_, timestamp);
  }

  return true;
}

bool KVStore::save_to_file_unlocked(const std::string& path) const {
  std::ofstream out(path);
  if (!out) return false;

  const auto entries = memtable_.snapshot_entries();

  for (const auto& [key, versions] : entries) {
    for (const auto& version : versions) {
      if (version.value.has_value()) {
        out << key << '\t'
            << version.timestamp << '\t'
            << 'P' << '\t'
            << *version.value << '\n';
      } else {
        out << key << '\t'
            << version.timestamp << '\t'
            << 'D' << '\n';
      }

      if (!out) return false;
    }
  }

  return true;
}

/*void KVStore::apply_put_no_log_unlocked(std::string key, std::string value) {
  map_[std::move(key)] = std::move(value);
}

void KVStore::apply_del_no_log_unlocked(const std::string& key) {
  map_.erase(key);
}*/

bool KVStore::flush_wal() {
  std::unique_lock lock(mu_);
  return wal_.flush();
}

void KVStore::ApplyPut(std::string key, std::string value) {
  std::unique_lock lock(mu_);
  map_[std::move(key)] = std::move(value);
}

void KVStore::ApplyDel(const std::string& key) {
  std::unique_lock lock(mu_);
  map_.erase(key);
}

std::vector<std::string>
KVStore::list_keys_with_prefix(const std::string& prefix) const {
  std::shared_lock lock(mu_);

  std::vector<std::string> result;

  for (const auto& [k, _] : map_) {
    if (k.rfind(prefix, 0) == 0) {
      result.push_back(k);
    }
  }

  return result;
}

} // namespace kv