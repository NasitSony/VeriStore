#include "kv/memtable.h"
#include "kv/sstable.h"
#include "kv/sstable_index.h"

#include <iostream>

int main() {
  kv::MemTable memtable;

  memtable.put("alpha", 1, "one");
  memtable.put("beta", 2, "two");
  memtable.put("delta", 3, "three");
  memtable.put("epsilon", 4, "four");
  memtable.put("gamma", 5, "five");
  memtable.put("omega", 6, "six");

  const std::string path = "/tmp/sstable-index-demo.sst";

  if (!kv::SSTableWriter::write(
          path,
          memtable.snapshot_entries())) {
    std::cerr << "failed to write SSTable\n";
    return 1;
  }

  kv::SSTableIndex index(2);

  if (!index.build(path)) {
    std::cerr << "failed to build SSTable index\n";
    return 1;
  }

  for (const auto& [key, offset] : index.entries()) {
    std::cout << key
              << " -> "
              << offset
              << '\n';
  }

  return 0;
}