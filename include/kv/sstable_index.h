#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>

#include <ios>

namespace kv {

class SSTableIndex {
public:
  explicit SSTableIndex(std::size_t stride = 4);

  bool build(const std::string& sstable_path);

  std::optional<std::streampos>
  find_start_offset(const std::string& key) const;

  const std::map<std::string, std::streampos>&
  entries() const;

private:
  std::size_t stride_;
  std::map<std::string, std::streampos> index_;
};

} // namespace kv