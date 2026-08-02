#include "kv/flush_worker.h"
#include "kv/manifest.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
  kv::MemTable memtable;

  memtable.put("alpha", 1, "one");
  memtable.put("alpha", 2, "two");
  memtable.put("beta", 3, "three");
  memtable.del("beta", 4);

  kv::FlushWorker worker;
  worker.start();

  const bool queued =worker.enqueue(
        "/tmp/flush-worker-demo.sst",
        "/tmp/flush-worker-demo-MANIFEST",
        memtable.take_entries()
    );

  if (!queued) {
    std::cerr << "failed to queue flush\n";
    return 1;
  }

  // stop() waits for queued work to finish.
  worker.stop();

  kv::Manifest manifest;
    manifest.open("/tmp/flush-worker-demo-MANIFEST");

    auto files = manifest.load_sstables();

    std::cout << "Manifest contains "
            << files.size()
            << " SSTables\n";

  std::cout << "flush worker stopped cleanly\n";
  return 0;
}