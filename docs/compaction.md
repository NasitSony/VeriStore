# SSTable Compaction in VeriStore

Compaction is the background process that merges multiple SSTables into a smaller set of larger SSTables.

Without compaction, every MemTable flush creates another SSTable, increasing the number of files that every lookup must search.

VeriStore currently implements a simplified Level-0 style compaction that preserves MVCC history while reducing the number of SSTables.

---

# 1. Why Compaction Exists

Every flush creates a new immutable SSTable.

```text
Flush 1
   │
   ▼
SSTable-0

Flush 2
   │
   ▼
SSTable-1

Flush 3
   │
   ▼
SSTable-2
```

Without compaction:

* more files accumulate;
* read amplification increases;
* startup becomes slower;
* more Bloom filters must be checked;
* more sparse indexes must be searched.

Compaction periodically merges these files into fewer SSTables.

---

# 2. Current Compaction Policy

The current implementation uses a simple trigger:

```text
if SSTable count >= threshold
    run compaction
```

The threshold is intentionally fixed to keep the implementation understandable.

Future versions may use adaptive scheduling.

---

# 3. Current Merge Strategy

VeriStore currently selects the two oldest SSTables.

```text
SSTable-0
SSTable-1
      │
      ▼
 Compactor
      │
      ▼
Compacted SSTable
```

The resulting file replaces both inputs.

---

# 4. Merge Algorithm

The compactor performs the following steps:

```text
Read SSTable A
        │
        ▼
Read SSTable B
        │
        ▼
Group records by key
        │
        ▼
Sort versions by timestamp
        │
        ▼
Remove duplicate timestamps
        │
        ▼
Write new SSTable
        │
        ▼
Update manifest
        │
        ▼
Delete old files
```

The ordering is critical for correctness.

---

# 5. MVCC Preservation

Suppose the inputs contain:

```text
SSTable A

alpha@1 = one
alpha@2 = two
beta@3 = three
```

```text
SSTable B

alpha@4 = four
beta@5 = tombstone
gamma@6 = six
```

The merged output becomes:

```text
alpha@1 = one
alpha@2 = two
alpha@4 = four

beta@3 = three
beta@5 = tombstone

gamma@6 = six
```

Historical versions are preserved.

Tombstones are preserved.

Timestamp ordering is preserved.

---

# 6. Why Old Versions Are Not Removed

Although several versions may appear obsolete:

```text
alpha@1
alpha@2
alpha@4
```

older snapshots may still require them.

Removing historical versions without tracking active snapshots could break historical reads.

Current policy:

```text
Preserve every version.
```

Future policy:

```text
Track oldest active snapshot.

↓

Remove unreachable versions.
```

---

# 7. Manifest Update

The manifest always contains the current set of live SSTables.

Before compaction:

```text
SSTable-0
SSTable-1
SSTable-2
```

After compaction:

```text
Compacted-4
SSTable-2
```

The manifest is rewritten atomically.

---

# 8. Correct File Lifecycle

The correct order is:

```text
Write new SSTable
        │
        ▼
Update manifest
        │
        ▼
Delete old SSTables
```

Deleting the old files first could lose data if writing the new SSTable fails.

---

# 9. Temporary Output File

The compactor first creates:

```text
veristore-compacted.tmp
```

After successful completion:

```text
rename()

↓

veristore-compacted-4.sst
```

This prevents partially written files from appearing in the manifest.

---

# 10. Recovering SSTable IDs

Originally, VeriStore generated new filenames using:

```text
next_id = live_sstable_count
```

Example:

```text
3 SSTables

↓

next_id = 3
```

After compaction:

```text
2 SSTables

↓

next_id = 2
```

This reused an existing filename.

The current implementation instead scans existing SSTables:

```text
maximum existing id

↓

next_id = max + 1
```

preventing filename collisions.

---

# 11. Duplicate Path Protection

During development another issue appeared:

```text
Compacting

SSTable-0

with

SSTable-0
```

instead of:

```text
SSTable-0

+

SSTable-1
```

The compactor now verifies:

```text
first != second
```

before beginning a merge.

---

# 12. Startup Compaction

When VeriStore starts:

```text
Open manifest
        │
        ▼
Load SSTables
        │
        ▼
Recover next id
        │
        ▼
Run compaction if threshold exceeded
```

This prevents unnecessary file accumulation across restarts.

---

# 13. Current Limitations

Current compaction intentionally omits:

* multiple levels;
* size-tiered scheduling;
* leveled compaction;
* compaction priorities;
* throttling;
* parallel compaction;
* snapshot-aware garbage collection;
* compression;
* checksum validation.

These are planned extensions.

---

# 14. Future Directions

Potential improvements include:

### Multi-level LSM

```text
L0
 ↓
L1
 ↓
L2
```

instead of one SSTable collection.

### Scheduler-aware compaction

Schedule compaction according to:

* write rate;
* CPU utilization;
* read amplification;
* memory pressure.

### Adaptive compaction

Prioritize SSTables that contribute most to lookup latency.

### Snapshot-aware cleanup

Safely remove obsolete MVCC versions once no active snapshot requires them.

---

# 15. Summary

Current compaction guarantees:

* preserves historical MVCC versions;
* preserves tombstones;
* prevents duplicate timestamps;
* updates the manifest before deleting inputs;
* avoids filename reuse;
* reduces SSTable count.

Although intentionally simplified, the implementation captures the core correctness requirements of LSM compaction while remaining compact enough to understand from first principles.
