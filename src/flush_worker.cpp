#include "kv/flush_worker.h"

#include "kv/sstable.h"
#include "kv/manifest.h"

#include <iostream>
#include <utility>

namespace kv {

FlushWorker::~FlushWorker() {
  stop();
}

void FlushWorker::start() {
  std::lock_guard<std::mutex> lock(mu_);

  if (started_) {
    return;
  }

  stopping_ = false;
  started_ = true;

  worker_ = std::thread(&FlushWorker::run, this);
}

void FlushWorker::stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);

    if (!started_) {
      return;
    }

    stopping_ = true;
  }

  cv_.notify_all();

  if (worker_.joinable()) {
    worker_.join();
  }

  std::lock_guard<std::mutex> lock(mu_);
  started_ = false;
}

bool FlushWorker::enqueue(
    std::string sstable_path,
    std::string manifest_path,
    MemTable::Entries entries) {
  {
    std::lock_guard<std::mutex> lock(mu_);

    if (!started_ || stopping_) {
      return false;
    }

    tasks_.push(FlushTask{
        std::move(sstable_path),
        std::move(manifest_path),
        std::move(entries)
    });
  }

  cv_.notify_one();
  return true;
}

std::size_t FlushWorker::pending_tasks() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tasks_.size();
}

void FlushWorker::run() {
  for (;;) {
    FlushTask task;

    {
      std::unique_lock<std::mutex> lock(mu_);

      cv_.wait(lock, [this] {
        return stopping_ || !tasks_.empty();
      });

      if (stopping_ && tasks_.empty()) {
        break;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    if (!SSTableWriter::write(
            task.sstable_path,
            task.entries)) {
    std::cerr
        << "[flush-worker] failed to write "
        << task.sstable_path
        << '\n';
    continue;
    }

    Manifest manifest;

    if (!manifest.open(task.manifest_path)) {
    std::cerr
        << "[flush-worker] failed to open manifest\n";
    continue;
    }

    if (!manifest.append_sstable(task.sstable_path)) {
    std::cerr
        << "[flush-worker] failed to append manifest\n";
    continue;
    }

    std::cout
        << "[flush-worker] wrote "
        << task.sstable_path
        << '\n';
    }
}

} // namespace kv