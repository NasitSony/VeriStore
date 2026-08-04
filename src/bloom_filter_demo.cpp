#include "kv/bloom_filter.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
  kv::BloomFilter filter(1024, 3);

  const std::vector<std::string> inserted{
      "alpha",
      "beta",
      "delta",
      "gamma"
  };

  for (const auto& key : inserted) {
    filter.add(key);
  }

  for (const auto& key : inserted) {
    std::cout
        << key
        << ": "
        << filter.possibly_contains(key)
        << '\n';
  }

  std::cout
      << "missing-key: "
      << filter.possibly_contains("missing-key")
      << '\n';

  return 0;
}