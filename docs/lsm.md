# LSM Tree in VeriStore

VeriStore is built around a simplified Log-Structured Merge (LSM) tree.

Instead of updating data in-place on disk, writes are first accumulated in memory and later flushed to immutable SSTables by a background worker.

This design minimizes random disk writes while supporting high write throughput and crash recovery.

---

# 1. Motivation

Updating a disk file for every write is expensive because each operation may require:

* seeking to the record;
* overwriting existing bytes;
* updating indexes;
* synchronizing metadata.

An LSM tree avoids these costs by writing sequentially.

Instead of:

```text
PUT
 ↓
Update disk page
 ↓
Rewrite index
```

VeriStore performs:

```text
PUT
 ↓
Append WAL
 ↓
Update MemTable
 ↓
Background flush
 ↓
Sequential SSTable write
```

---

# 2. High-Level Architecture

```text
                 Client
                    │
                    ▼
            Write-Ahead Log
                    │
                    ▼
              Active MemTable
                    │
           Flush threshold reached
                    │
                    ▼
          Immutable MemTable
                    │
                    ▼
         Background Flush Worker
                    │
                    ▼
                SSTable (L0)
                    │
                    ▼
              Background Compaction
```

---

# 3. Active MemTable

The active MemTable receives every write after the WAL append succeeds.

Internally it stores:

```text
key
    ↓
ordered Version list
```

Example:

```text
alpha
 ├── ts=1 → one
 └── ts=2 → two

beta
 ├── ts=3 → three
```

Because the MemTable is sorted by key, it can later be written directly into an SSTable.

---

# 4. Flush Threshold

The MemTable grows until it exceeds a configured threshold.

```text
Current size
        │
        ▼
Threshold exceeded
        │
        ▼
Flush required
```

The threshold is currently measured in bytes.

No writes are sent directly to disk during normal operation.

---

# 5. MemTable Rotation

Once the threshold is exceeded:

```text
Active MemTable
        │
take_entries()
        ▼
Immutable MemTable
```

A new empty MemTable immediately becomes active.

```text
Before

Active
  │
  ▼
Writes

After

Immutable
      │
      ▼
Flush Worker

Active
      │
      ▼
New Writes
```

This allows writes to continue while the previous MemTable is flushed asynchronously.

---

# 6. Flush Queue

The immutable MemTable is packaged into a flush task.

Each task contains:

```text
SSTable output path
Manifest path
Version entries
```

The task is pushed onto the worker queue.

```text
Producer
   │
   ▼
Task Queue
   │
   ▼
Flush Worker
```

---

# 7. Background Flush

The worker thread repeatedly performs:

```text
Wait
 │
 ▼
Receive task
 │
 ▼
Write SSTable
 │
 ▼
Update Manifest
 │
 ▼
Publish completion
```

The worker never modifies the active MemTable.

---

# 8. Immutable Lifetime

The immutable MemTable is intentionally retained until the flush completion is consumed.

```text
Immutable
     │
     ▼
Flush in progress
     │
     ▼
Flush complete
     │
     ▼
Register SSTable
     │
     ▼
Destroy immutable
```

This prevents temporary visibility gaps.

---

# 9. SSTable Generation

The worker writes one immutable MemTable into one SSTable.

Example:

```text
alpha 1 P one
alpha 2 P two
beta  3 P three
beta  4 D
```

Records are sorted by:

1. key
2. timestamp

The SSTable never changes after creation.

---

# 10. Manifest Registration

Every completed SSTable is added to the manifest.

Example:

```text
/tmp/veristore-0.sst
/tmp/veristore-1.sst
/tmp/veristore-2.sst
```

The manifest represents the set of live SSTables.

---

# 11. Read Path

Reads search:

```text
Active MemTable
        │
        ▼
Immutable MemTable
        │
        ▼
Newest SSTable
        │
        ▼
Older SSTables
```

The newest visible value always wins.

---

# 12. Background Compaction

As SSTables accumulate, lookup cost increases.

Compaction merges multiple SSTables into one.

```text
SST1
SST2
   │
   ▼
Compactor
   │
   ▼
Merged SSTable
```

The current implementation merges the two oldest SSTables.

---

# 13. Startup Recovery

During startup VeriStore performs:

```text
Load snapshot
      │
      ▼
Replay WAL
      │
      ▼
Load Manifest
      │
      ▼
Recover SSTable IDs
      │
      ▼
Start Flush Worker
```

The manifest ensures existing SSTables are immediately available after restart.

---

# 14. Current Simplifications

The current LSM implementation intentionally uses:

* one active MemTable;
* one immutable MemTable;
* one flush worker;
* one compaction policy;
* one SSTable level;
* text-based SSTables;
* in-memory sparse indexes;
* in-memory Bloom filters.

These choices keep the implementation understandable while preserving the core LSM architecture.

---

# 15. Future Work

Potential extensions include:

```text
Multiple immutable MemTables
Multiple flush workers
Level-based compaction
Binary SSTables
Persisted Bloom filters
Compression
Block cache
Range iterators
Adaptive compaction scheduling
```

These additions would move VeriStore closer to production storage engines while preserving the existing architecture.

---

# Summary

The current write lifecycle is:

```text
Client
   │
   ▼
WAL
   │
   ▼
Active MemTable
   │
   ▼
Immutable MemTable
   │
   ▼
Flush Queue
   │
   ▼
Background Flush Worker
   │
   ▼
SSTable
   │
   ▼
Manifest
   │
   ▼
Compaction
```

The design keeps writes sequential, separates foreground and background work, and provides a clear progression from volatile memory to durable on-disk storage.
