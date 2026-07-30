#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace kv {

using Timestamp = uint64_t;

struct Version {
  Timestamp timestamp;
  std::optional<std::string> value;

  bool is_tombstone() const noexcept {
    return !value.has_value();
  }
};

} // namespace kv