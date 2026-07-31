#include "kv/memtable.h"
#include "kv/sstable.h"
#include "kv/sstable_reader.h"
#include "kv/lookup_result.h"

#include <iostream>

void print_result(const std::string& label,
                  const kv::LookupResult& result) {
  std::cout << label << ": ";

  if (result.state == kv::LookupState::Value) {
    std::cout << *result.value;
  } else if (result.state == kv::LookupState::Tombstone) {
    std::cout << "<deleted>";
  } else {
    std::cout << "<not found>";
  }

  std::cout << '\n';
}

int main() {
  kv::MemTable memtable;

  memtable.put("alpha", 1, "one");
  memtable.put("alpha", 2, "two");
  memtable.put("beta", 3, "three");
  memtable.del("beta", 4);

  const auto entries = memtable.snapshot_entries();

  if (!kv::SSTableWriter::write(
          "demo.sst",
          entries)) {
    std::cerr << "failed to write SSTable\n";
    return 1;
  }

  std::cout << "wrote demo.sst\n";

  kv::SSTableReader reader("demo.sst");

    auto alpha1 = reader.get_at("alpha", 1);
    auto alpha2 = reader.get_at("alpha", 2);
    auto beta3 = reader.get_at("beta", 3);
    auto beta4 = reader.get_at("beta", 4);

    print_result("alpha@1", alpha1);
    print_result("alpha@2", alpha2);
    print_result("beta@3", beta3);
    print_result("beta@4", beta4);
  return 0;
}