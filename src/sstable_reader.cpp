#include "kv/sstable_reader.h"

#include <sstream>
#include <utility>

namespace kv {

SSTableReader::SSTableReader(
    std::string path,
    std::size_t index_stride,
    std::size_t bloom_bits,
    std::size_t bloom_hashes)
    : path_(std::move(path)),
      index_(index_stride),
      bloom_filter_(bloom_bits, bloom_hashes) {

  index_ready_ = index_.build(path_);
  bloom_ready_ = build_bloom_filter();

  // Open once and retain the stream for subsequent lookups.
  file_.open(path_, std::ios::binary);
  file_ready_ = static_cast<bool>(file_);
}

bool SSTableReader::is_open() const noexcept {
  return file_ready_;
}

bool SSTableReader::possibly_contains(
    const std::string& key) const {
  return !bloom_ready_ ||
         bloom_filter_.possibly_contains(key);
}

bool SSTableReader::build_bloom_filter() {
  // Use a separate stream during construction. The persistent stream is
  // opened after the Bloom filter and sparse index are built.
  std::ifstream in(path_, std::ios::binary);

  if (!in) {
    return false;
  }

  std::string line;

  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    std::istringstream row(line);
    std::string key;

    if (!(row >> key)) {
      continue;
    }

    bloom_filter_.add(key);
  }

  return true;
}

LookupResult SSTableReader::get_at(
    const std::string& key,
    Timestamp read_timestamp) const {

  // A negative Bloom-filter result is definitive, so no file lock or
  // file operation is required.
  if (bloom_ready_ &&
      !bloom_filter_.possibly_contains(key)) {
    return LookupResult::not_found();
  }

  if (!file_ready_) {
    return LookupResult::not_found();
  }

  std::lock_guard<std::mutex> lock(file_mu_);

  // Previous reads may have reached EOF and set eofbit/failbit.
  file_.clear();

  std::streampos start_offset = std::streampos{0};

  if (index_ready_) {
    const auto indexed_offset =
        index_.find_start_offset(key);

    if (indexed_offset.has_value()) {
      start_offset = *indexed_offset;
    }
  }

  file_.seekg(start_offset);

  if (!file_) {
    return LookupResult::not_found();
  }

  LookupResult best =
      LookupResult::not_found();

  Timestamp best_timestamp = 0;
  bool found_visible_version = false;

  std::string line;

  while (std::getline(file_, line)) {
    if (line.empty()) {
      continue;
    }

    std::istringstream row(line);

    std::string stored_key;
    Timestamp timestamp = 0;
    char type = '\0';

    if (!(row >> stored_key >> timestamp >> type)) {
      continue;
    }

    // SSTable keys are sorted. Once we pass the requested key,
    // no later record can match it.
    if (stored_key > key) {
      break;
    }

    if (stored_key < key) {
      continue;
    }

    if (timestamp > read_timestamp) {
      continue;
    }

    if (!found_visible_version ||
        timestamp >= best_timestamp) {
      found_visible_version = true;
      best_timestamp = timestamp;

      if (type == 'P') {
        std::string value;
        std::getline(row >> std::ws, value);

        best = LookupResult::found_value(
            std::move(value)
        );
      } else if (type == 'D') {
        best = LookupResult::tombstone();
      }
    }
  }

  return best;
}

} // namespace kv