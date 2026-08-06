# VeriStore Benchmarks

This document summarizes the current VeriStore benchmark methodology and measured results.

The purpose of these benchmarks is not to claim parity with production storage engines. VeriStore is an educational, correctness-first implementation with a simplified text SSTable format, in-memory metadata structures, and a single-node execution model.

The benchmarks are intended to:

* validate architectural changes;
* compare in-memory and SSTable read paths;
* measure the impact of Bloom-filter sizing;
* detect regressions;
* provide reproducible performance evidence.

---

## 1. Benchmark Environment

Record the environment used for every published result.

```text
Machine: Mac mini
Operating system: macOS
Compiler: AppleClang 17
Language standard: C++17 or current CMake configuration
Build command: cmake --build build
Storage: local disk
```

Before publishing final numbers, add the exact Mac mini model, processor, memory size, macOS version, and CMake build type.

For example:

```text
Machine: Mac mini M4
Memory: <fill in>
Operating system: macOS <fill in>
Compiler: AppleClang 17
Build type: <Debug or Release>
```

Build type is especially important because unoptimized debug builds may be substantially slower than release builds.

---

## 2. Benchmark Programs

VeriStore currently contains two main benchmark executables.

### `storage_benchmark`

Measures:

* sequential `PUT`;
* existing-key in-memory `GET`;
* missing-key in-memory `GET`.

The `GET` measurements use VeriStore’s latest-value compatibility map.

They represent an in-memory lookup baseline rather than the complete SSTable read path.

### `sstable_benchmark`

Measures:

* indexed SSTable hit lookups;
* Bloom-filter-assisted SSTable misses;
* Bloom-filter false-positive rate.

The benchmark uses:

```text
Dataset size: 1,000,000 keys
Sparse-index stride: 4 records
Bloom-filter size: 10 bits per key
Bloom hash functions: 7
```

The `SSTableReader` is constructed once and retains its file stream across lookups.

---

## 3. Running the Benchmarks

Build the project:

```bash
cmake -S . -B build
cmake --build build
```

Run the in-memory storage benchmark:

```bash
./build/storage_benchmark
```

Run the SSTable benchmark:

```bash
./build/sstable_benchmark
```

Run the storage benchmark five times:

```bash
for i in {1..5}; do
  ./build/storage_benchmark
done
```

Run the SSTable benchmark five times:

```bash
for i in {1..5}; do
  ./build/sstable_benchmark
done
```

Results are appended to:

```text
benchmarks/results.csv
benchmarks/sstable_results.csv
```

---

## 4. In-Memory Storage Results

The storage benchmark performs 100,000 operations per test.

Six recorded runs produced the following approximate median throughput.

| Benchmark        | Median throughput | Approximate average latency |
| ---------------- | ----------------: | --------------------------: |
| Sequential PUT   |     ~245K ops/sec |                     ~4.1 μs |
| Existing-key GET |     ~5.4M ops/sec |                    ~0.19 μs |
| Missing-key GET  |     ~7.0M ops/sec |                    ~0.14 μs |

All runs verified:

```text
100,000 successful existing-key lookups
100,000 successful missing-key checks
```

### Interpretation

The in-memory GET path uses the latest-value compatibility map.

These numbers should therefore be interpreted as:

```text
hash-map-backed latest-value lookup performance
```

They do not represent:

* SSTable lookup performance;
* full MVCC lookup performance;
* multi-SSTable read amplification;
* persisted Bloom-filter performance;
* distributed or replicated performance.

The sequential PUT result includes:

* WAL append;
* in-memory map update;
* MVCC MemTable insertion;
* size accounting;
* group-commit behavior;
* at least one background MemTable flush when the threshold is crossed.

---

## 5. SSTable Hit Results

Five repeated runs were performed against an SSTable containing 1,000,000 keys.

| Run |      Throughput | Average latency |
| --: | --------------: | --------------: |
|   1 | 144,497 ops/sec |        6.921 μs |
|   2 | 156,525 ops/sec |        6.389 μs |
|   3 | 154,972 ops/sec |        6.453 μs |
|   4 | 154,186 ops/sec |        6.486 μs |
|   5 | 149,987 ops/sec |        6.667 μs |

Approximate median:

```text
Throughput: ~154K lookups/sec
Latency:    ~6.49 μs
```

### Read path

Each hit performs:

```text
Bloom-filter check
        │
        ▼
Sparse-index lookup
        │
        ▼
Seek persistent input stream
        │
        ▼
Scan matching key versions
        │
        ▼
Return newest visible MVCC version
```

The SSTable format is currently line-based text, so parsing and stream operations remain significant parts of the lookup cost.

---

## 6. SSTable Miss Results

Five repeated missing-key runs produced:

| Run |    Throughput | Average latency |
| --: | ------------: | --------------: |
|   1 | 5.27M ops/sec |        0.190 μs |
|   2 | 4.44M ops/sec |        0.225 μs |
|   3 | 5.21M ops/sec |        0.192 μs |
|   4 | 5.01M ops/sec |        0.200 μs |
|   5 | 5.07M ops/sec |        0.197 μs |

Approximate median:

```text
Throughput: ~5.07M lookups/sec
Latency:    ~0.197 μs
```

All five runs verified 1,000,000 missing-key results.

The measured Bloom-filter false-positive rate remained:

```text
0.808%
```

---

## 7. Bloom-Filter Evaluation

The initial benchmark used the default filter size:

```text
8,192 bits
```

for:

```text
1,000,000 keys
```

This was severely undersized.

The filter became highly saturated, causing most missing keys to pass the Bloom check and continue into the SSTable read path.

Initial missing-key performance was approximately:

```text
132K lookups/sec
7.55 μs average latency
```

The benchmark was then changed to:

```text
10 bits per key
7 hash functions
```

For 1,000,000 keys, this produced:

```text
10,000,000 filter bits
approximately 1.25 MB
0.808% false-positive rate
```

After correct sizing:

```text
~5.07M lookups/sec
~0.197 μs average latency
```

This corresponds to an approximate latency improvement of:

```text
7.55 μs / 0.197 μs ≈ 38×
```

### Key finding

Bloom-filter effectiveness depends on sizing relative to the number of inserted keys.

A fixed filter size may be correct in the sense that it produces no false negatives, while still being practically ineffective because of excessive false positives.

---

## 8. Persistent SSTable Reader Evaluation

The first `SSTableReader` implementation opened a new file stream for every lookup.

Measured SSTable hit performance was approximately:

```text
80,887 lookups/sec
12.36 μs average latency
```

The reader was changed to retain a persistent input stream.

After the change:

```text
~154K lookups/sec median
~6.49 μs average latency
```

This produced approximately:

```text
1.9× higher hit throughput
```

The persistent stream requires synchronization because `seekg()`, stream flags, and the current file cursor are mutable shared state.

---

## 9. Why Hits Are Slower Than Bloom-Rejected Misses

A Bloom-rejected miss returns after:

```text
hash key
→ inspect Bloom-filter bits
→ return NotFound
```

A hit continues through:

```text
Bloom-filter check
→ sparse-index lookup
→ mutex acquisition
→ stream clear
→ file seek
→ line parsing
→ MVCC visibility selection
```

Therefore:

```text
Bloom-rejected miss: ~0.20 μs
SSTable hit:         ~6.49 μs
```

This difference is expected.

---

## 10. CSV Output

The storage benchmark writes rows with:

```text
timestamp
benchmark
operations
total_ms
ops_per_sec
avg_us
verified
```

Example:

```csv
timestamp,benchmark,operations,total_ms,ops_per_sec,avg_us,verified
2026-08-05T13:12:16,Sequential PUT,100000,438.901,227841.576,4.389,100000
```

The SSTable benchmark additionally records:

```text
false_positive_rate
```

These files allow results to be aggregated independently rather than relying on a single terminal run.

---

## 11. Reproducibility Guidelines

For more trustworthy results:

1. Use a release build.
2. Record the exact machine and compiler.
3. Close CPU-intensive applications.
4. Run every benchmark at least five times.
5. Report medians rather than only the fastest run.
6. Preserve raw CSV output.
7. Use the same dataset size and Bloom-filter parameters.
8. Clean generated benchmark state before each independent experiment.
9. Separate in-memory and on-disk benchmark claims.
10. Avoid comparing VeriStore directly with production engines unless the methodology is equivalent.

A release build can be generated with:

```bash
cmake -S . -B build-release \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-release
```

Then run:

```bash
./build-release/storage_benchmark
./build-release/sstable_benchmark
```

Final README results should ideally come from the release build.

---

## 12. Current Limitations

The current benchmarks do not yet measure:

* p50, p95, or p99 latency;
* concurrent client throughput;
* range scans;
* write amplification;
* read amplification across many SSTables;
* compaction duration;
* flush latency distribution;
* recovery duration;
* WAL replay throughput;
* storage-space amplification;
* Bloom-filter construction cost;
* sparse-index memory use;
* cache behavior;
* replicated performance.

The text SSTable format also adds parsing overhead that would not exist in the same form in a binary block-based design.

---

## 13. Planned Benchmark Extensions

Future experiments may include:

### Durability modes

Compare:

```text
fsync every write
group commit every 5 writes
group commit every 100 writes
group commit every 1,000 writes
```

### Bloom-filter sizing

Compare:

```text
4 bits/key
6 bits/key
8 bits/key
10 bits/key
12 bits/key
```

Measure:

* false-positive rate;
* memory use;
* missing-key latency.

### Sparse-index stride

Compare:

```text
stride 1
stride 4
stride 16
stride 64
```

Measure:

* index memory use;
* hit latency;
* records scanned per lookup.

### Compaction

Measure:

* input bytes;
* output bytes;
* compaction duration;
* SSTable count before and after;
* lookup latency before and after compaction.

### Recovery

Measure startup time for:

```text
WAL-only recovery
snapshot + WAL recovery
manifest + SSTable discovery
```

### Concurrency

Measure throughput and latency with:

```text
1 reader
2 readers
4 readers
8 readers
```

The persistent reader currently serializes access to one file stream. A future implementation may use positional reads or separate reader handles to improve concurrent lookup scalability.

---

## 14. Summary

The current benchmarks demonstrate three main findings.

### Grouped write processing

VeriStore sustains approximately:

```text
245K sequential PUT operations/sec
```

for the current 100,000-operation workload and group-commit configuration.

### Persistent file reuse

Keeping the SSTable stream open improved hit throughput by approximately:

```text
1.9×
```

compared with reopening the file for every lookup.

### Correctly sized Bloom filters

Using 10 bits per key and 7 hash functions produced:

```text
0.808% false-positive rate
~5.07M missing-key lookups/sec
~0.197 μs median average latency
```

This reduced negative lookup latency by approximately:

```text
38×
```

relative to the undersized-filter configuration.

These results show why storage-engine optimizations must be measured under realistic sizing assumptions rather than evaluated only for functional correctness.
