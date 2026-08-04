#include "kv/bloom_filter.h"

#include <algorithm>

namespace kv {

BloomFilter::BloomFilter(
    std::size_t bit_count,
    std::size_t hash_count)
    : bit_count_(std::max<std::size_t>(bit_count, 8)),
      hash_count_(std::max<std::size_t>(hash_count, 1)),
      bits_((bit_count_ + 7) / 8, 0) {}

std::uint64_t BloomFilter::hash_with_seed(
    std::string_view key,
    std::uint64_t seed) {
  std::uint64_t hash =
      1469598103934665603ULL ^ seed;

  for (const unsigned char byte : key) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }

  // Additional mixing.
  hash ^= hash >> 33;
  hash *= 0xff51afd7ed558ccdULL;
  hash ^= hash >> 33;

  return hash;
}

void BloomFilter::set_bit(
    std::size_t bit_index) {
  const std::size_t byte_index = bit_index / 8;
  const std::size_t bit_offset = bit_index % 8;

  bits_[byte_index] |=
      static_cast<std::uint8_t>(1U << bit_offset);
}

bool BloomFilter::get_bit(
    std::size_t bit_index) const {
  const std::size_t byte_index = bit_index / 8;
  const std::size_t bit_offset = bit_index % 8;

  return (
      bits_[byte_index] &
      static_cast<std::uint8_t>(1U << bit_offset)
  ) != 0;
}

void BloomFilter::add(
    std::string_view key) {
  for (std::size_t i = 0;
       i < hash_count_;
       ++i) {
    const std::uint64_t hash =
        hash_with_seed(
            key,
            0x9e3779b97f4a7c15ULL * (i + 1)
        );

    set_bit(
        static_cast<std::size_t>(
            hash % bit_count_
        )
    );
  }
}

bool BloomFilter::possibly_contains(
    std::string_view key) const {
  for (std::size_t i = 0;
       i < hash_count_;
       ++i) {
    const std::uint64_t hash =
        hash_with_seed(
            key,
            0x9e3779b97f4a7c15ULL * (i + 1)
        );

    const std::size_t bit_index =
        static_cast<std::size_t>(
            hash % bit_count_
        );

    if (!get_bit(bit_index)) {
      return false;
    }
  }

  return true;
}

std::size_t BloomFilter::bit_count() const noexcept {
  return bit_count_;
}

std::size_t BloomFilter::hash_count() const noexcept {
  return hash_count_;
}

} // namespace kv