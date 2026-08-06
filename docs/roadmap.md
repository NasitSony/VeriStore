# VeriStore Roadmap

VeriStore began as an educational storage engine for learning durability, recovery, and write-ahead logging.

As the project evolved, it expanded into a modern LSM-based storage engine with MVCC, background flushing, SSTables, sparse indexing, Bloom filters, manifest management, and background compaction.

The long-term goal is not simply to reproduce existing storage engines, but to build a platform for experimenting with storage-system design, workload-aware optimization, and distributed storage research.

---

# Vision

The roadmap is divided into four phases.

```text
Phase 1
Foundation
        │
        ▼
Phase 2
Production Features
        │
        ▼
Phase 3
Distributed Storage
        │
        ▼
Phase 4
Research and AI Infrastructure
```

Each phase builds upon the previous one.

---

# Phase 1 — Foundation ✅

Current implementation.

Completed features include:

* Write-Ahead Log
* Crash Recovery
* Group Commit
* MVCC
* Active MemTable
* Immutable MemTable
* Background Flush Worker
* SSTables
* Sparse Index
* Bloom Filter
* Manifest
* Background Compaction
* Benchmark Suite
* Documentation

The goal of Phase 1 is correctness.

---

# Phase 2 — Production Storage Features

The next step is to move closer to the architecture of production LSM engines.

## Multi-Level LSM

Current implementation:

```text
Level 0
```

Future:

```text
L0

↓

L1

↓

L2

↓

L3
```

This reduces read amplification while improving lookup scalability.

---

## Block Cache

Introduce an in-memory cache for frequently accessed SSTable blocks.

Goals:

* reduce disk reads;
* improve repeated lookups;
* support configurable cache policies.

Potential replacement policies:

* LRU
* CLOCK
* ARC

---

## Binary SSTable Format

Current SSTables are text-based.

Future SSTables may contain:

```text
Header

↓

Data Blocks

↓

Block Index

↓

Bloom Filter

↓

Footer
```

Benefits:

* lower parsing cost;
* smaller storage footprint;
* improved sequential scanning.

---

## Compression

Potential algorithms:

* Snappy
* Zstd
* LZ4

Goals:

* reduce storage usage;
* reduce write amplification;
* improve cache efficiency.

---

## WAL Improvements

Potential additions:

* segmented WAL;
* WAL recycling;
* asynchronous fsync;
* configurable durability modes.

---

# Phase 3 — Distributed Storage

The long-term architecture extends beyond a single storage node.

---

## Replication

Potential protocols:

* Raft
* Multi-Paxos

Goals:

* replicated durability;
* leader failover;
* log consistency.

---

## Distributed Transactions

Potential additions:

* two-phase commit;
* optimistic concurrency control;
* distributed MVCC.

---

## Sharding

Current:

```text
Single node
```

Future:

```text
Shard 1

Shard 2

Shard 3
```

Goals:

* horizontal scalability;
* distributed metadata;
* shard-aware routing.

---

## Distributed Metadata Service

Potential responsibilities:

* shard placement;
* tablet metadata;
* replica membership;
* cluster configuration.

---

# Phase 4 — Research Directions

This phase explores ideas beyond conventional storage-engine implementations.

---

## Scheduler-Aware Compaction

Current systems often compact using fixed thresholds.

A future VeriStore policy could instead consider:

* write rate;
* read amplification;
* memory pressure;
* CPU utilization;
* background workload.

Possible objective:

```text
Minimize user-visible latency
```

instead of simply minimizing SSTable count.

---

## Adaptive Bloom Filters

Instead of assigning the same Bloom-filter size to every SSTable:

```text
10 bits/key
```

future versions may allocate memory dynamically.

Possible inputs:

* lookup frequency;
* false-positive history;
* SSTable age.

Goals:

* lower memory usage;
* maintain lookup performance.

---

## AI-Aware Storage

Potential workload:

```text
Embedding ingestion

↓

Vector retrieval

↓

RAG systems
```

Potential optimizations:

* adaptive flush scheduling;
* hot-data prioritization;
* cache-aware compaction.

---

## Learned Storage Policies

Instead of fixed heuristics:

```text
Flush every X MB
```

or

```text
Compact after N SSTables
```

future versions may explore workload-driven decision making.

Possible inputs:

* write bursts;
* read locality;
* historical access patterns.

---

# Benchmark Roadmap

Future benchmark categories include:

## Storage

* write throughput;
* read throughput;
* recovery time.

## LSM

* flush latency;
* compaction latency;
* write amplification;
* read amplification.

## MVCC

* snapshot reads;
* version chain length;
* garbage collection.

## Distributed

* replication latency;
* leader failover;
* shard rebalancing.

---

# Documentation Roadmap

Future documentation may include:

* binary SSTable format;
* cache subsystem;
* replication architecture;
* sharding architecture;
* transaction protocol;
* scheduler-aware compaction.

---

# Long-Term Goal

The objective of VeriStore is not to compete directly with mature storage engines such as RocksDB or LevelDB.

Instead, the project serves three complementary purposes:

* understand production storage-engine architecture by building it from first principles;
* provide a modular platform for experimentation with storage-system ideas;
* explore workload-aware optimizations for future distributed and AI infrastructure.

As the implementation evolves, the emphasis will gradually shift from reproducing established techniques toward evaluating original scheduling, caching, and storage policies through reproducible experiments.

---

# Current Status

```text
✔ Durable WAL

✔ MVCC

✔ LSM Write Path

✔ Background Flush

✔ SSTables

✔ Sparse Index

✔ Bloom Filter

✔ Compaction

✔ Benchmarks

↓

Next

Multi-Level LSM

↓

Block Cache

↓

Distributed Storage

↓

Research-Oriented Optimizations
```

---

# Closing Remarks

VeriStore is intended to grow incrementally.

Each feature is added only after the previous layer has been validated through testing and benchmarking.

This incremental approach keeps the implementation understandable while providing a foundation for exploring more advanced storage-system and distributed-systems research in future iterations.
