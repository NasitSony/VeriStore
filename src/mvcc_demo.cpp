#include "kv/kv_store.h"

#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <chrono>
#include <thread>

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

    
    for (int batch = 0; batch < 4; ++batch) {
        store.put(
            "large-" + std::to_string(batch * 2),
            std::string(100, 'a' + batch)
        );

        store.put(
            "large-" + std::to_string(batch * 2 + 1),
            std::string(100, 'a' + batch)
        );

        std::this_thread::sleep_for(
            std::chrono::milliseconds(100)
        );

        // A public operation drains completed flushes.
        (void)store.get_at("name", 3);
     }

     std::this_thread::sleep_for(
        std::chrono::milliseconds(200)
    );

    (void)store.get_at("name", 3);

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