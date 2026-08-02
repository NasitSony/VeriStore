#pragma once

#include "kv/memtable.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace kv {

struct FlushTask {
    std::string sstable_path;

    std::string manifest_path;

    MemTable::Entries entries;
};

class FlushWorker {
public:
  FlushWorker() = default;
  ~FlushWorker();

  FlushWorker(const FlushWorker&) = delete;
  FlushWorker& operator=(const FlushWorker&) = delete;

  void start();
  void stop();

  bool enqueue(
    std::string sstable_path,
    std::string manifest_path,
    MemTable::Entries entries);

  std::size_t pending_tasks() const;

private:
  void run();

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::queue<FlushTask> tasks_;

  std::thread worker_;
  bool started_{false};
  bool stopping_{false};
};

} // namespace kv