#include "kv/kv_store.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;

double elapsed_seconds(
    const Clock::time_point& start,
    const Clock::time_point& end) {
  return std::chrono::duration<double>(end - start).count();
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

  out << std::put_time(
      &tm,
      "%Y-%m-%dT%H:%M:%S");

  return out.str();
}

void print_result(
    const std::string& name,
    std::size_t operations,
    double seconds) {

  const double ops_per_second =
      operations / seconds;

  const double average_us =
      seconds * 1000000.0 /
      operations;

  std::cout
      << std::left
      << std::setw(28)
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
      << average_us
      << '\n';
}

void append_csv_result(
    const std::string& csv_path,
    const std::string& benchmark,
    std::size_t operations,
    double seconds,
    std::size_t verified) {

  const bool exists =
      std::filesystem::exists(csv_path);

  std::ofstream out(
      csv_path,
      std::ios::app);

  if (!out) {
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
        << "verified\n";
  }

  const double ops_per_second =
      operations / seconds;

  const double average_us =
      seconds * 1000000.0 /
      operations;

  out
      << current_timestamp() << ','
      << benchmark << ','
      << operations << ','
      << std::fixed
      << std::setprecision(3)
      << seconds * 1000.0 << ','
      << ops_per_second << ','
      << average_us << ','
      << verified
      << '\n';
}

}  // namespace

int main() {

  constexpr std::size_t kOperations = 100000;

  std::filesystem::create_directories(
      "benchmarks");

  const std::string csv_path =
      "benchmarks/results.csv";

  const std::string wal_path =
      "/tmp/veristore-benchmark.wal";

  std::remove(wal_path.c_str());
  std::remove("/tmp/veristore-MANIFEST");

  kv::KVStore store;

  if (!store.open(wal_path)) {
    std::cerr
        << "Failed to open store\n";
    return 1;
  }

  store.set_group_commit_every(1000);

  //----------------------------------------------------
  // PUT benchmark
  //----------------------------------------------------

  auto put_start = Clock::now();

  for (std::size_t i = 0;
       i < kOperations;
       ++i) {

    store.put(
        "key-" + std::to_string(i),
        "value-" + std::to_string(i));
  }

  store.flush_wal();

  auto put_end = Clock::now();

  //----------------------------------------------------
  // Existing GET
  //----------------------------------------------------

  std::size_t found = 0;

  auto hit_start = Clock::now();

  for (std::size_t i = 0;
       i < kOperations;
       ++i) {

    auto value =
        store.get(
            "key-" +
            std::to_string(i));

    if (value.has_value()) {
      ++found;
    }
  }

  auto hit_end = Clock::now();

  //----------------------------------------------------
  // Missing GET
  //----------------------------------------------------

  std::size_t missing = 0;

  auto miss_start = Clock::now();

  for (std::size_t i = 0;
       i < kOperations;
       ++i) {

    auto value =
        store.get(
            "missing-" +
            std::to_string(i));

    if (!value.has_value()) {
      ++missing;
    }
  }

  auto miss_end = Clock::now();

  //----------------------------------------------------
  // Compute durations
  //----------------------------------------------------

  const double put_seconds =
      elapsed_seconds(
          put_start,
          put_end);

  const double hit_seconds =
      elapsed_seconds(
          hit_start,
          hit_end);

  const double miss_seconds =
      elapsed_seconds(
          miss_start,
          miss_end);

  //----------------------------------------------------
  // Print
  //----------------------------------------------------

  std::cout
      << "\nVeriStore Benchmark\n"
      << "-------------------\n";

  print_result(
      "Sequential PUT",
      kOperations,
      put_seconds);

  print_result(
      "Existing-key GET",
      kOperations,
      hit_seconds);

  print_result(
      "Missing-key GET",
      kOperations,
      miss_seconds);

  std::cout
      << "verified_hits="
      << found
      << '\n';

  std::cout
      << "verified_misses="
      << missing
      << '\n';

  //----------------------------------------------------
  // CSV
  //----------------------------------------------------

  append_csv_result(
      csv_path,
      "Sequential PUT",
      kOperations,
      put_seconds,
      kOperations);

  append_csv_result(
      csv_path,
      "Existing-key GET",
      kOperations,
      hit_seconds,
      found);

  append_csv_result(
      csv_path,
      "Missing-key GET",
      kOperations,
      miss_seconds,
      missing);

  return 0;
}