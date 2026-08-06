# 🚀 VeriStore

> A modern LSM-based storage engine built in C++ to explore the core techniques behind production databases.

VeriStore is a correctness-first storage engine that incrementally implements many of the building blocks found in modern key-value databases. The project focuses on understanding how durable storage systems are designed, from write-ahead logging and crash recovery to MVCC, immutable MemTables, SSTables, Bloom filters, and background compaction.

Rather than optimizing for every workload, VeriStore emphasizes **correctness, recoverability, and clean system design**, making it a practical exploration of storage engine internals.

---

## Highlights

- Write-Ahead Logging (WAL) with crash recovery
- Multi-Version Concurrency Control (MVCC)
- MemTable → Immutable MemTable lifecycle
- Background flush worker
- SSTable storage format
- Sparse index for faster lookups
- Bloom filters for negative lookup acceleration
- Manifest-based SSTable discovery
- Background compaction
- Reproducible benchmark suite

---

## Architecture

```text
                     Client
                        │
                        ▼
                Write-Ahead Log
                        │
                        ▼
                   MemTable
                        │
            Flush Threshold Reached
                        │
                        ▼
              Immutable MemTable
                        │
             Background Flush Worker
                        │
                        ▼
                   SSTables (L0)
               ┌────────┴────────┐
               ▼                 ▼
         Sparse Index      Bloom Filter
               │                 │
               └────────┬────────┘
                        ▼
                   Read Path
                        │
                        ▼
              Background Compaction
```

---

## Implemented Components

| Component | Status |
|-----------|:------:|
| Write-Ahead Log (WAL) | ✅ |
| Crash Recovery | ✅ |
| Group Commit | ✅ |
| MVCC | ✅ |
| MemTable | ✅ |
| Immutable MemTable | ✅ |
| Background Flush | ✅ |
| SSTables | ✅ |
| Sparse Index | ✅ |
| Bloom Filter | ✅ |
| Manifest | ✅ |
| Background Compaction | ✅ |
| Benchmark Suite | ✅ |

---

## Performance Snapshot

### In-memory operations

| Benchmark | Median Result |
|-----------|--------------:|
| Sequential PUT | ~245K ops/sec |
| Existing-key GET | ~5.4M ops/sec |
| Missing-key GET | ~7.0M ops/sec |

### SSTable operations

| Benchmark | Median Result |
|-----------|--------------:|
| SSTable hit | ~154K ops/sec |
| SSTable miss | ~5.1M ops/sec |
| Bloom filter false-positive rate | 0.808% |

Correctly sizing the Bloom filter reduced missing-key lookup latency from approximately **7.5 μs** to **0.2 μs**, providing roughly a **38× improvement** for negative lookups while maintaining a false-positive rate below 1%.

---

## Repository Structure

```text
include/
src/
benchmarks/
docs/

storage_benchmark.cpp
sstable_benchmark.cpp
```

---

## Building

```bash
cmake -S . -B build
cmake --build build
```

---

## Running

### Storage benchmark

```bash
./build/storage_benchmark
```

### SSTable benchmark

```bash
./build/sstable_benchmark
```

---

## Documentation

Additional implementation details are available in the `docs/` directory:

- Architecture - [Architecture](docs/architecture.md)
- LSM Tree - [Benchmarks](docs/benchmarks.md)
- MVCC - [MVCC and historical reads](docs/mvcc.md)
- Compaction - [LSM Tree lifecycle](docs/lsm.md)
- Benchmarks - [Compaction](docs/compaction.md)
- Roadmap - [Roadmap](docs/roadmap.md)

---

## Future Work

- Multi-level LSM tree
- Block cache
- Compression
- Adaptive Bloom filters
- Scheduler-aware compaction
- Distributed transactions
- Replication beyond a single node

---

## Project Goals

VeriStore is intended to be an educational yet practical storage engine that demonstrates how modern databases manage durability, versioning, indexing, and on-disk organization. The emphasis is on building these mechanisms from first principles and validating them through reproducible benchmarks.

---

## License

MIT License