#pragma once

#include "kv/version.h"
#include "kv/lookup_result.h"


#include <optional>
#include <string>

namespace kv {

class SSTableReader {
public:
  explicit SSTableReader(std::string path);

  LookupResult
  get_at(const std::string& key,
         Timestamp read_timestamp) const;

private:
  std::string path_;
};

} // namespace kv