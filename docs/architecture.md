# VeriStore Architecture

VeriStore is a correctness-first LSM-based key-value storage engine written in C++. It combines durability, multi-version reads, asynchronous persistence, indexed SSTables, Bloom filters, and compaction in a modular architecture.

The implementation is intentionally simplified compared with production systems such as RocksDB or LevelDB, but it preserves the core responsibilities and ordering constraints of an LSM storage engine.

---

## 1. System Overview

```text
                         Client
                PUT / GET / GET_AT / DEL
                           │
                           ▼
                     ┌───────────┐
                     │  KVStore  │
                     └─────┬─────┘
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
       Write-Ahead Log             Latest-Value Map
       durability path             compatibility view
              │
              ▼
        Active MemTable
        MVCC versions
              │
        size threshold
              ▼
      Immutable MemTable
              │
              ▼
     Background Flush Worker
              │
              ▼
           SSTables
      ┌────────┼─────────┐
      ▼        ▼         ▼
 Sparse Index Bloom    Manifest
              Filter
      └────────┬─────────┘
               ▼
          Point Reads
               │
               ▼
          Compaction
```

The central `KVStore` coordinates the write path, read path, WAL, MemTable lifecycle, background flush worker, SSTable registry, and compaction.

---

## 2. Main Components

### KVStore

`KVStore` is the top-level storage-engine interface.

Its responsibilities include:

* accepting `PUT`, `GET`, `GET_AT`, and `DEL` operations;
* assigning monotonically increasing sequence numbers;
* appending mutations to the WAL;
* maintaining the latest-value view;
* storing MVCC versions in the active MemTable;
* rotating a full MemTable into immutable state;
* submitting immutable data to the background flush worker;
* registering completed SSTables;
* searching active, immutable, and on-disk state;
* loading SSTables from the manifest during startup;
* triggering basic compaction.

### Write-Ahead Log

The WAL protects mutations before they are considered part of the in-memory state.

Each record contains:

```text
magic
version
operation type
key length
value length
sequence number
key
value
CRC
```

The CRC detects corrupted or partially written records.

During recovery, VeriStore:

1. seeks to the beginning of the WAL;
2. reads records in sequence;
3. validates record structure and CRC;
4. applies valid mutations without logging them again;
5. stops at the first incomplete or corrupted record;
6. restores the highest sequence number observed.

---

## 3. Write Path

A normal `PUT` follows this path:

```text
Client PUT
    │
    ▼
Acquire KVStore write lock
    │
    ▼
Allocate sequence number
    │
    ▼
Append mutation to WAL buffer
    │
    ▼
Update latest-value map
    │
    ▼
Append Version to active MemTable
    │
    ▼
Check MemTable size
    │
    ├── below threshold ──► return
    │
    ▼
Move active entries into immutable state
    │
    ▼
Queue background SSTable flush
    │
    ▼
Continue accepting writes
```

The mutation is appended to the WAL before it is applied to the in-memory state.

The active MemTable stores all versions in timestamp order:

```cpp
Version {
  timestamp,
  optional<value>
}
```

A delete is represented as a version whose value is absent:

```text
Version{timestamp, tombstone}
```

---

## 4. Group Commit

WAL records are initially buffered.

Instead of calling `fsync()` after every mutation, VeriStore flushes the WAL periodically according to the configured group-commit boundary.

```text
PUT 1 ─┐
PUT 2  ├── buffered
PUT 3  ┤
PUT 4  ┤
PUT 5 ─┘
         │
         ▼
       fsync
```

This reduces synchronization overhead and improves throughput, while defining a clear durability boundary.

An explicit WAL flush is also supported.

---

## 5. MVCC

VeriStore uses sequence numbers as logical timestamps.

For a key with multiple updates:

```text
name@1 = "version-1"
name@2 = "version-2"
name@3 = tombstone
```

A historical read selects the newest version whose timestamp is less than or equal to the requested read timestamp.

```text
GET_AT(name, 1) → version-1
GET_AT(name, 2) → version-2
GET_AT(name, 3) → deleted
```

The visibility rule is:

```text
visible version =
  newest version where version.timestamp <= read_timestamp
```

Tombstones must remain distinguishable from missing keys.

VeriStore therefore uses three lookup states:

```text
NotFound
Value
Tombstone
```

This prevents an older SSTable value from reappearing after a newer MemTable tombstone is found.

---

## 6. MemTable Lifecycle

The active MemTable is implemented as a sorted map:

```text
key → ordered list of Version objects
```

The sorted key order is important because SSTables are written in key order.

The lifecycle is:

```text
Active MemTable
      │
      │ threshold reached
      ▼
take_entries()
      │
      ▼
Immutable MemTable
      │
      ▼
Background flush queue
```

`take_entries()` moves the active entries into an immutable batch and resets the active MemTable’s size accounting.

While the immutable batch is being flushed, new writes continue in a fresh active MemTable.

The current implementation allows one immutable MemTable at a time.

---

## 7. Background Flush Worker

The flush worker owns:

```text
worker thread
task queue
mutex
condition variable
completion queue
```

A flush task contains:

```text
SSTable output path
manifest path
immutable MemTable entries
```

The worker performs:

```text
Dequeue task
    │
    ▼
Write SSTable
    │
    ▼
Append SSTable path to manifest
    │
    ▼
Publish flush completion
```

The immutable MemTable is retained until the completion is consumed.

This prevents a temporary visibility gap where data would exist in neither memory nor a registered SSTable.

During shutdown, the worker completes queued tasks before joining its thread.

---

## 8. SSTable Format

The current SSTable format is text-based for readability and experimentation.

Each line stores one MVCC version:

```text
key<TAB>timestamp<TAB>P<TAB>value
key<TAB>timestamp<TAB>D
```

Example:

```text
alpha	1	P	one
alpha	2	P	two
beta	3	P	three
beta	4	D
```

Records are sorted by:

1. key;
2. timestamp within each key.

SSTables are immutable after creation.

The text format is intentionally simple. A future version may use binary blocks, checksums, compression, a footer, and persisted index metadata.

---

## 9. Sparse Index

Without an index, every lookup would scan an SSTable from byte zero.

The sparse index stores periodic mappings:

```text
indexed key → byte offset
```

Example:

```text
alpha → 0
delta → 27
gamma → 60
```

For a target key, the reader:

1. finds the nearest indexed key less than or equal to the target;
2. seeks to its byte offset;
3. scans forward;
4. stops when it passes the target key.

Because a key may have several MVCC versions, the index must point to the beginning of the key’s version group.

For example:

```text
beta@3
beta@5
```

The index for `beta` must point to `beta@3`, not `beta@5`. Otherwise, a historical read at timestamp `3` could skip the visible version.

---

## 10. Bloom Filter

Each `SSTableReader` builds an in-memory Bloom filter containing all keys in the SSTable.

The lookup flow is:

```text
Bloom filter
    │
    ├── definitely absent ──► return NotFound
    │
    ▼
possibly present
    │
    ▼
Sparse-index seek
    │
    ▼
Read matching key versions
```

Bloom filters may return false positives, but they must not return false negatives.

The filter is sized using:

```text
bits per key
number of hash functions
```

For the benchmark dataset:

```text
1,000,000 keys
10 bits per key
7 hash functions
```

the measured false-positive rate was approximately:

```text
0.808%
```

Persisting Bloom-filter metadata inside each SSTable is future work. The current reader rebuilds the filter when it is constructed.

---

## 11. Persistent SSTable Reader

`SSTableReader` keeps its input stream open across lookups.

The read path reuses the same file handle:

```text
Bloom check
    │
    ▼
Clear stream flags
    │
    ▼
Seek to sparse-index offset
    │
    ▼
Read nearby versions
```

The persistent stream avoids reopening the file for every request.

Because the stream cursor is shared mutable state, access is protected by a mutex.

This optimization improved measured SSTable hit throughput from approximately:

```text
80K lookups/sec
```

to approximately:

```text
154K lookups/sec
```

---

## 12. Read Path

A historical lookup follows this order:

```text
1. Active MemTable
2. Immutable MemTable
3. SSTables, newest to oldest
4. NotFound
```

The read stops immediately when it finds:

* a visible value;
* a visible tombstone.

A tombstone prevents fallback to an older SSTable value.

For SSTable lookup:

```text
Bloom filter
    │
    ▼
Sparse index
    │
    ▼
Persistent file seek
    │
    ▼
Newest visible version
```

The regular `GET` API currently uses the latest-value compatibility map.

The `GET_AT` API exercises the MVCC LSM read path.

---

## 13. Manifest

The manifest is a text file containing the set of live SSTables.

Example:

```text
/tmp/veristore-compacted-4.sst
/tmp/veristore-2.sst
/tmp/veristore-3.sst
```

During startup:

```text
Open manifest
    │
    ▼
Load live SSTable paths
    │
    ▼
Recover next file identifier
    │
    ▼
Resume reads and background work
```

The next SSTable identifier is recovered from existing filenames rather than from the number of live files.

Using the live-file count as an identifier caused collisions after compaction reduced the number of files. Recovering the highest existing identifier prevents filename reuse.

---

## 14. Compaction

The current compaction policy triggers when the number of SSTables reaches a configured threshold.

The basic algorithm:

```text
Select two oldest SSTables
        │
        ▼
Load all MVCC versions
        │
        ▼
Merge by key
        │
        ▼
Sort versions by timestamp
        │
        ▼
Remove exact timestamp duplicates
        │
        ▼
Write compacted temporary SSTable
        │
        ▼
Rename to final path
        │
        ▼
Atomically replace manifest
        │
        ▼
Delete old input files
```

The ordering is correctness-critical:

```text
write output
→ publish new manifest
→ remove old inputs
```

Deleting the old files first could make data unavailable if the new output or manifest update failed.

The current compactor preserves:

* historical versions;
* tombstones;
* timestamp ordering.

Safe MVCC garbage collection requires tracking active snapshots and is future work.

---

## 15. Startup and Recovery

Startup currently performs:

```text
Load snapshot
    │
    ▼
Open WAL
    │
    ▼
Replay valid WAL records
    │
    ▼
Restore sequence number
    │
    ▼
Open manifest
    │
    ▼
Load SSTable paths
    │
    ▼
Recover next SSTable identifier
    │
    ▼
Run startup compaction if required
    │
    ▼
Start background flush worker
```

The store is marked open only after initialization succeeds.

---

## 16. Concurrency Model

`KVStore` uses a shared mutex to protect top-level storage state.

The MemTable owns its own shared mutex.

The flush worker owns a separate mutex for:

* queued tasks;
* completion events;
* lifecycle state.

The persistent SSTable input stream is protected by a mutex because `seekg()` and `getline()` mutate shared stream state.

The current lock order is:

```text
KVStore lock
    │
    ▼
MemTable lock
```

Maintaining a consistent lock order reduces deadlock risk.

---

## 17. Current Simplifications

VeriStore intentionally simplifies several production concerns:

* one active and one immutable MemTable;
* one background flush worker;
* text-based SSTable format;
* in-memory sparse indexes;
* in-memory Bloom filters;
* single compaction tier;
* no MVCC garbage collection;
* no block cache;
* no compression;
* no persisted SSTable footer;
* no checksums for individual SSTable blocks;
* no fully atomic WAL/SSTable lifecycle.

These choices keep the implementation understandable while preserving the main architectural relationships.

---

## 18. Design Principles

VeriStore follows several design principles.

### Correctness before optimization

The implementation first establishes correct visibility, durability, and file-lifecycle ordering. Performance optimizations are added only after correctness tests pass.

### Explicit state transitions

Important transitions are represented directly:

```text
active → immutable → queued → persisted → registered
```

### Modular components

The WAL, MemTable, SSTable writer, SSTable reader, sparse index, Bloom filter, manifest, compactor, and flush worker can be tested independently.

### Measured optimization

Changes such as persistent file reuse and Bloom-filter sizing are validated with repeatable benchmarks rather than assumed to help.

---

## 19. Future Architecture

Potential future additions include:

```text
Multiple immutable MemTables
Multiple flush workers
Level-based compaction
Block-based binary SSTables
Persisted Bloom filters
Block cache
Checksums
Compression
Range iterators
Snapshot tracking
MVCC garbage collection
WAL segmentation and recycling
Scheduler-aware background maintenance
Adaptive Bloom-filter allocation
```

The research-oriented direction is to explore workload-aware scheduling policies for flush, compaction, and memory allocation rather than relying only on fixed thresholds.
