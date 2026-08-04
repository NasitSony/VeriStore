#include "kv/compactor.h"

#include "kv/sstable.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

namespace kv {

bool Compactor::load_sstable(
    const std::string& path,
    MemTable::Entries& merged) {
  std::ifstream in(path, std::ios::binary);

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
    Timestamp timestamp = 0;
    char type = '\0';

    if (!(row >> key >> timestamp >> type)) {
      return false;
    }

    if (type == 'P') {
      std::string value;
      std::getline(row >> std::ws, value);

      merged[key].push_back(
          Version{timestamp, std::move(value)}
      );
    } else if (type == 'D') {
      merged[key].push_back(
          Version{timestamp, std::nullopt}
      );
    } else {
      return false;
    }
  }

  return true;
}

bool Compactor::compact(
    const std::vector<std::string>& input_paths,
    const std::string& output_path) {
  if (input_paths.empty()) {
    return false;
  }

  MemTable::Entries merged;

  for (const auto& path : input_paths) {
    if (!load_sstable(path, merged)) {
      return false;
    }
  }

  for (auto& [key, versions] : merged) {
    std::sort(
        versions.begin(),
        versions.end(),
        [](const Version& left, const Version& right) {
          return left.timestamp < right.timestamp;
        }
    );

    // Protect against the same version being present in
    // multiple input SSTables.
    versions.erase(
        std::unique(
            versions.begin(),
            versions.end(),
            [](const Version& left, const Version& right) {
              return left.timestamp == right.timestamp;
            }
        ),
        versions.end()
    );
  }

  return SSTableWriter::write(output_path, merged);
}

} // namespace kv