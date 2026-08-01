#include "kv/kv_store.h"

#include <cstdio>
#include <iostream>
#include <optional>
#include <string>

void print_value(const std::string& label,
                 const std::optional<std::string>& value) {
  std::cout << label << ": "
            << (value ? *value : "<deleted/not found>")
            << '\n';
}

int main() {
  const std::string wal_path = "/tmp/veristore_mvcc_test.wal";

  // Start with a clean WAL.
  std::remove(wal_path.c_str());

  {
    kv::KVStore store;

    if (!store.open(wal_path)) {
      std::cerr << "Failed to open store\n";
      return 1;
    }

    // Force every WAL record to disk for this test.
    store.set_group_commit_every(1);

    store.put("name", "version-1");  // timestamp 1
    store.put("name", "version-2");  // timestamp 2
    store.del("name");               // timestamp 3

    

    print_value("Before restart at ts=1", store.get_at("name", 1));
    print_value("Before restart at ts=2", store.get_at("name", 2));
    print_value("Before restart at ts=3", store.get_at("name", 3));
    print_value("Before restart latest", store.get("name"));

    if (!store.flush_wal()) {
      std::cerr << "Failed to flush WAL\n";
      return 1;
    }
  }

  std::cout << "\nRestarting...\n\n";

  {
    kv::KVStore recovered;

    if (!recovered.open(wal_path)) {
      std::cerr << "Failed to recover store\n";
      return 1;
    }

    print_value("After restart at ts=1", recovered.get_at("name", 1));
    print_value("After restart at ts=2", recovered.get_at("name", 2));
    print_value("After restart at ts=3", recovered.get_at("name", 3));
    print_value("After restart latest", recovered.get("name"));
  }

  std::remove(wal_path.c_str());
  return 0;
}