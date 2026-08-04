#include "kv/compactor.h"
#include "kv/memtable.h"
#include "kv/sstable.h"
#include "kv/sstable_reader.h"

#include <iostream>
#include <vector>

void print_result(
    const std::string& label,
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
  const std::string first_path =
      "/tmp/compaction-0.sst";

  const std::string second_path =
      "/tmp/compaction-1.sst";

  const std::string output_path =
      "/tmp/compacted.sst";

  kv::MemTable first;
  first.put("alpha", 1, "one");
  first.put("alpha", 2, "two");
  first.put("beta", 3, "three");

  kv::MemTable second;
  second.put("alpha", 4, "four");
  second.del("beta", 5);
  second.put("gamma", 6, "six");

  if (!kv::SSTableWriter::write(
          first_path,
          first.snapshot_entries())) {
    return 1;
  }

  if (!kv::SSTableWriter::write(
          second_path,
          second.snapshot_entries())) {
    return 1;
  }

  if (!kv::Compactor::compact(
          {first_path, second_path},
          output_path)) {
    std::cerr << "compaction failed\n";
    return 1;
  }

  kv::SSTableReader reader(output_path);

  print_result("alpha@1", reader.get_at("alpha", 1));
  print_result("alpha@2", reader.get_at("alpha", 2));
  print_result("alpha@4", reader.get_at("alpha", 4));
  print_result("beta@3", reader.get_at("beta", 3));
  print_result("beta@5", reader.get_at("beta", 5));
  print_result("gamma@6", reader.get_at("gamma", 6));

  return 0;
}