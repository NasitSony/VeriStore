#include "kv/sstable_reader.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace kv {

SSTableReader::SSTableReader(
    std::string path,
    std::size_t index_stride)
    : path_(std::move(path)),
      index_(index_stride),
      index_ready_(index_.build(path_)) {}

LookupResult SSTableReader::get_at(
    const std::string& key,
    Timestamp read_timestamp) const {
  std::ifstream in(path_, std::ios::binary);

  if (!in) {
    return LookupResult::not_found();
  }

  if (index_ready_) {
  const auto offset =
      index_.find_start_offset(key);

  if (offset.has_value()) {
    in.seekg(*offset);

    if (!in) {
      return LookupResult::not_found();
    }
  }
}

  Timestamp best_timestamp = 0;
  bool found = false;
  LookupResult best = LookupResult::not_found();

  std::string line;

  while (std::getline(in, line)) {
    std::istringstream row(line);

    std::string stored_key;
    Timestamp timestamp;
    char type;

    if (!(row >> stored_key >> timestamp >> type)) {
      continue;
    }

    if (stored_key > key) {
        break;
    }

    if (stored_key < key) {
      continue;
    }

    if (stored_key != key ||
        timestamp > read_timestamp) {
      continue;
    }

    if (!found || timestamp >= best_timestamp) {
      found = true;
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