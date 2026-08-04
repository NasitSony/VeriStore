#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kv {

class BloomFilter {
public:
  BloomFilter(
      std::size_t bit_count = 1024,
      std::size_t hash_count = 3);

  void add(std::string_view key);

  bool possibly_contains(
      std::string_view key) const;

  std::size_t bit_count() const noexcept;
  std::size_t hash_count() const noexcept;

private:
  static std::uint64_t hash_with_seed(
      std::string_view key,
      std::uint64_t seed);

  void set_bit(std::size_t bit_index);
  bool get_bit(std::size_t bit_index) const;

  std::size_t bit_count_;
  std::size_t hash_count_;
  std::vector<std::uint8_t> bits_;
};

} // namespace kv