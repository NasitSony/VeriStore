#include "kv/memtable.h"
#include "kv/sstable.h"
#include "kv/sstable_reader.h"

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <string>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

using Clock = std::chrono::steady_clock;

double elapsed_seconds(
    const Clock::time_point& start,
    const Clock::time_point& end) {
  return std::chrono::duration<double>(end - start).count();
}

void print_result(
    const std::string& name,
    std::size_t operations,
    double seconds) {

  const double ops_per_second =
      seconds > 0.0
          ? static_cast<double>(operations) / seconds
          : 0.0;

  const double average_microseconds =
      operations > 0
          ? (seconds * 1'000'000.0) /
                static_cast<double>(operations)
          : 0.0;

  std::cout
      << std::left
      << std::setw(30)
      << name
      << " operations="
      << operations
      << " total_ms="
      << std::fixed
      << std::setprecision(2)
      << seconds * 1000.0
      << " ops_per_sec="
      << std::setprecision(0)
      << ops_per_second
      << " avg_us="
      << std::setprecision(3)
      << average_microseconds
      << '\n';
}

std::string current_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm tm{};

#if defined(_WIN32)
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif

  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  return out.str();
}

void append_csv_result(
    const std::string& csv_path,
    const std::string& benchmark,
    std::size_t operations,
    double seconds,
    std::size_t verified,
    double false_positive_rate) {

  const bool exists =
      std::filesystem::exists(csv_path);

  std::ofstream out(csv_path, std::ios::app);

  if (!out) {
    std::cerr << "failed to open " << csv_path << '\n';
    return;
  }

  if (!exists) {
    out
        << "timestamp,"
        << "benchmark,"
        << "operations,"
        << "total_ms,"
        << "ops_per_sec,"
        << "avg_us,"
        << "verified,"
        << "false_positive_rate\n";
  }

  const double ops_per_second =
      seconds > 0.0
          ? static_cast<double>(operations) / seconds
          : 0.0;

  const double average_us =
      operations > 0
          ? seconds * 1'000'000.0 /
                static_cast<double>(operations)
          : 0.0;

  out
      << current_timestamp() << ','
      << benchmark << ','
      << operations << ','
      << std::fixed
      << std::setprecision(3)
      << seconds * 1000.0 << ','
      << ops_per_second << ','
      << average_us << ','
      << verified << ','
      << false_positive_rate
      << '\n';
}

} // namespace

int main() {

  constexpr std::size_t kOperations = 1'000'000;

  const std::string path =
      "/tmp/sstable-benchmark.sst";

  std::filesystem::create_directories("benchmarks");

  const std::string csv_path =
    "benchmarks/sstable_results.csv";

  std::remove(path.c_str());

  //--------------------------------------------------------
  // Build SSTable
  //--------------------------------------------------------

  kv::MemTable memtable;

  for (std::size_t i = 0;
       i < kOperations;
       ++i) {

    memtable.put(
        "key-" + std::to_string(i),
        i + 1,
        "value-" + std::to_string(i));
  }

  if (!kv::SSTableWriter::write(
          path,
          memtable.snapshot_entries())) {

    std::cerr << "Failed to write SSTable\n";
    return 1;
  }

  //--------------------------------------------------------
  // Reader
  //--------------------------------------------------------


   

    kv::SSTableReader reader(
        path,
        4,                  // sparse-index stride
        kOperations * 10,   // 10 bits per key
        7                   // hash functions
    );

    //--------------------------------------------------------
    // Bloom false-positive check
    //--------------------------------------------------------

    std::size_t bloom_false_positives = 0;

    for (std::size_t i = 0;
        i < kOperations;
        ++i) {
    if (reader.possibly_contains(
            "missing-" + std::to_string(i))) {
        ++bloom_false_positives;
    }
    }

    std::cout
        << "bloom_false_positives="
        << bloom_false_positives
        << " rate="
        << std::fixed
        << std::setprecision(3)
        << (100.0 *
            static_cast<double>(bloom_false_positives) /
            static_cast<double>(kOperations))
        << "%\n";

    const double false_positive_rate =
    100.0 *
    static_cast<double>(bloom_false_positives) /
    static_cast<double>(kOperations);


    //--------------------------------------------------------
    // Hit benchmark
    //--------------------------------------------------------

    std::size_t hit_count = 0;

    auto hit_start = Clock::now();

    for (std::size_t i = 0;
        i < kOperations;
        ++i) {

        auto result =
            reader.get_at(
                "key-" + std::to_string(i),
                i + 1);

        if (result.state ==
            kv::LookupState::Value) {
        ++hit_count;
        }
    }

    auto hit_end = Clock::now();

  //--------------------------------------------------------
  // Miss benchmark
  //--------------------------------------------------------

  std::size_t miss_count = 0;

  auto miss_start = Clock::now();

  for (std::size_t i = 0;
       i < kOperations;
       ++i) {

    auto result =
        reader.get_at(
            "missing-" + std::to_string(i),
            kOperations + 100);

    if (result.state ==
        kv::LookupState::NotFound) {
      ++miss_count;
    }
  }

  auto miss_end = Clock::now();

  //--------------------------------------------------------
  // Results
  //--------------------------------------------------------

  const double hit_seconds =
    elapsed_seconds(hit_start, hit_end);

  const double miss_seconds =
    elapsed_seconds(miss_start, miss_end);

  std::cout << "\n";
  std::cout << "SSTable Benchmark\n";
  std::cout << "-----------------\n";

  print_result(
      "SSTable Hit Lookup",
      kOperations,
      elapsed_seconds(hit_start,
                      hit_end));

  print_result(
      "SSTable Miss Lookup",
      kOperations,
      elapsed_seconds(miss_start,
                      miss_end));

  std::cout
      << "verified_hits="
      << hit_count
      << '\n';

  std::cout
      << "verified_misses="
      << miss_count
      << '\n';


  append_csv_result(
        csv_path,
        "sstable_get_hit",
        kOperations,
        hit_seconds,
        hit_count,
        false_positive_rate
    );

    append_csv_result(
        csv_path,
        "sstable_get_miss",
        kOperations,
        miss_seconds,
        miss_count,
        false_positive_rate
    );    

  return 0;
}