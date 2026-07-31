#pragma once

#include <optional>
#include <string>
#include <utility>

namespace kv {

enum class LookupState {
  NotFound,
  Value,
  Tombstone
};

struct LookupResult {
  LookupState state{LookupState::NotFound};
  std::optional<std::string> value;

  static LookupResult not_found() {
    return {LookupState::NotFound, std::nullopt};
  }

  static LookupResult found_value(std::string value) {
    return {
        LookupState::Value,
        std::move(value)
    };
  }

  static LookupResult tombstone() {
    return {LookupState::Tombstone, std::nullopt};
  }
};

} // namespace kv