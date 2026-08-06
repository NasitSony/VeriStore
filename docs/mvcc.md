# MVCC in VeriStore

VeriStore implements a simplified form of Multi-Version Concurrency Control (MVCC) using monotonically increasing sequence numbers as logical timestamps.

Instead of storing only the latest value for each key, the MVCC layer retains a history of versions:

```text
key → [version₁, version₂, version₃, ...]
```

This enables historical reads such as:

```text
GET_AT(key, timestamp)
```

The implementation focuses on version visibility and durable recovery. It does not yet implement transactions, snapshot registration, isolation levels, or automatic version garbage collection.

---

## 1. Why MVCC Exists

A basic key-value store usually maintains one value per key:

```text
user:1 → Alice
```

Updating the key replaces the old value:

```text
user:1 → Bob
```

After replacement, the earlier value is lost.

MVCC retains both versions:

```text
user:1
├── ts=1 → Alice
└── ts=2 → Bob
```

This allows reads at different logical points in time:

```text
GET_AT(user:1, 1) → Alice
GET_AT(user:1, 2) → Bob
```

MVCC is useful for:

* snapshot reads;
* consistent transaction views;
* historical inspection;
* replication and recovery;
* deferred cleanup of old versions;
* avoiding destructive in-place updates.

---

## 2. Version Representation

VeriStore defines a logical timestamp as an unsigned 64-bit integer:

```cpp
using Timestamp = uint64_t;
```

Each version stores:

```cpp
struct Version {
  Timestamp timestamp;
  std::optional<std::string> value;

  bool is_tombstone() const noexcept {
    return !value.has_value();
  }
};
```

A normal value is represented as:

```text
Version {
  timestamp = 10,
  value = "hello"
}
```

A deletion is represented as:

```text
Version {
  timestamp = 11,
  value = null
}
```

The missing value is a tombstone.

---

## 3. Sequence Numbers as Logical Time

Every successful mutation receives a monotonically increasing sequence number:

```text
PUT a=1       → sequence 1
PUT a=2       → sequence 2
DELETE a      → sequence 3
PUT b=hello   → sequence 4
```

The sequence number serves as the version timestamp.

The store maintains:

```cpp
uint64_t seq_{0};
```

A write allocates a timestamp using:

```cpp
const Timestamp timestamp = ++seq_;
```

The WAL stores the same sequence number, ensuring that recovered versions receive the same timestamps after restart.

---

## 4. Version Chains

The MemTable stores versions using:

```text
sorted key → ordered version list
```

Conceptually:

```text
alpha
├── ts=1 → one
├── ts=2 → two
└── ts=5 → five

beta
├── ts=3 → three
└── ts=4 → tombstone
```

Within each key, versions are appended in sequence-number order.

This enables reverse iteration from newest to oldest.

---

## 5. Historical Read Rule

A historical read requests:

```text
GET_AT(key, read_timestamp)
```

The visibility rule is:

```text
Return the newest version whose timestamp is
less than or equal to read_timestamp.
```

Formally:

```text
visible_version =
  max(version.timestamp)
  where version.timestamp <= read_timestamp
```

Example:

```text
alpha@1 = one
alpha@2 = two
alpha@5 = five
```

Results:

```text
GET_AT(alpha, 0) → NotFound
GET_AT(alpha, 1) → one
GET_AT(alpha, 2) → two
GET_AT(alpha, 4) → two
GET_AT(alpha, 5) → five
GET_AT(alpha, 9) → five
```

---

## 6. Tombstone Visibility

Consider:

```text
beta@3 = three
beta@5 = tombstone
```

The results are:

```text
GET_AT(beta, 2) → NotFound
GET_AT(beta, 3) → three
GET_AT(beta, 4) → three
GET_AT(beta, 5) → deleted
GET_AT(beta, 8) → deleted
```

The tombstone is itself a visible version.

It does not mean that the key was never present. It means that the key was deleted at a particular timestamp.

---

## 7. Lookup States

A plain `std::optional<std::string>` cannot distinguish:

```text
key does not exist
```

from:

```text
key exists but its visible version is a tombstone
```

This distinction is critical when data may exist in multiple storage layers.

VeriStore therefore uses:

```cpp
enum class LookupState {
  NotFound,
  Value,
  Tombstone
};
```

and:

```cpp
struct LookupResult {
  LookupState state;
  std::optional<std::string> value;
};
```

The three states mean:

### `NotFound`

The layer contains no visible version for the requested key and timestamp.

The read may continue to an older storage layer.

### `Value`

The layer contains a visible value.

The read returns immediately.

### `Tombstone`

The layer contains a visible deletion.

The read returns deleted immediately and must not continue to an older SSTable.

---

## 8. Why Tombstones Must Stop Fallback

Suppose the storage layers contain:

```text
Active MemTable:
  user@10 = tombstone

Older SSTable:
  user@4 = Alice
```

A read at timestamp `12` checks the active MemTable first.

If tombstone and missing key were both represented as `nullopt`, the read could incorrectly continue to the SSTable and return:

```text
Alice
```

That would resurrect deleted data.

The correct behavior is:

```text
Active MemTable returns Tombstone
→ stop lookup
→ return deleted
```

---

## 9. MemTable MVCC Lookup

The active MemTable lookup searches versions in reverse order:

```text
newest → oldest
```

Conceptually:

```cpp
for (auto it = versions.rbegin();
     it != versions.rend();
     ++it) {
  if (it->timestamp <= read_timestamp) {
    if (it->is_tombstone()) {
      return Tombstone;
    }

    return Value;
  }
}

return NotFound;
```

Reverse iteration makes the first visible version the correct result.

---

## 10. Read Order Across Storage Layers

VeriStore searches layers from newest to oldest:

```text
1. Active MemTable
2. Immutable MemTable
3. SSTables, newest to oldest
4. NotFound
```

The read path is:

```text
Active MemTable
      │
      ├── Value ───────► return value
      ├── Tombstone ───► return deleted
      └── NotFound
             │
             ▼
Immutable MemTable
      │
      ├── Value ───────► return value
      ├── Tombstone ───► return deleted
      └── NotFound
             │
             ▼
Newest SSTable
             │
             ▼
Older SSTables
             │
             ▼
NotFound
```

This ordering ensures that newer versions override older versions.

---

## 11. MVCC in SSTables

SSTables preserve all versions written by the MemTable.

The current text format is:

```text
key<TAB>timestamp<TAB>P<TAB>value
key<TAB>timestamp<TAB>D
```

Example:

```text
alpha	1	P	one
alpha	2	P	two
alpha	4	P	four
beta	3	P	three
beta	5	D
```

`P` represents a value record.

`D` represents a deletion tombstone.

---

## 12. Sparse Index and Version Groups

The sparse SSTable index stores:

```text
key → byte offset
```

A key may have multiple adjacent versions:

```text
beta@3
beta@5
```

The index must point to the first version of the key group.

Incorrect:

```text
beta → offset of beta@5
```

Correct:

```text
beta → offset of beta@3
```

Otherwise:

```text
GET_AT(beta, 3)
```

could seek directly to `beta@5`, skip it because timestamp 5 is too new, and incorrectly return `NotFound`.

This bug was discovered through the compaction demo and fixed by tracking each key group’s starting offset.

---

## 13. MVCC and Bloom Filters

The Bloom filter stores only user keys:

```text
alpha
beta
gamma
```

It does not store each timestamp separately.

For example:

```text
alpha@1
alpha@2
alpha@4
```

adds `alpha` to the filter.

The Bloom filter answers:

```text
Could any version of this key exist in this SSTable?
```

It does not answer:

```text
Is there a version visible at this timestamp?
```

Timestamp visibility is resolved after the Bloom check by scanning the key’s version group.

---

## 14. MVCC and Compaction

The current compactor merges version chains from multiple SSTables.

Example input:

```text
SSTable 1:
alpha@1 = one
alpha@2 = two
beta@3 = three

SSTable 2:
alpha@4 = four
beta@5 = tombstone
gamma@6 = six
```

Compacted output:

```text
alpha@1 = one
alpha@2 = two
alpha@4 = four
beta@3 = three
beta@5 = tombstone
gamma@6 = six
```

The compactor:

* groups records by key;
* sorts versions by timestamp;
* removes duplicate timestamps;
* preserves values;
* preserves tombstones.

It does not currently remove old versions.

---

## 15. Why Old Versions Are Not Yet Removed

Deleting an old version may be unsafe if a reader still depends on it.

Suppose:

```text
account@10 = 100
account@20 = 200
```

A snapshot at timestamp `15` must see:

```text
100
```

If compaction removes `account@10`, that snapshot becomes invalid.

A production MVCC engine tracks the oldest active snapshot:

```text
oldest_active_snapshot
```

Only versions that cannot be observed by any active snapshot may be garbage collected.

VeriStore does not yet track active snapshots, so compaction conservatively preserves version history.

---

## 16. WAL Recovery of MVCC Versions

Each WAL record contains the original sequence number.

During replay:

```text
PUT record
→ restore latest-value map
→ append version to MemTable

DELETE record
→ remove latest-value entry
→ append tombstone to MemTable
```

Example WAL:

```text
seq=1 PUT alpha one
seq=2 PUT alpha two
seq=3 DEL alpha
```

After recovery:

```text
alpha
├── ts=1 → one
├── ts=2 → two
└── ts=3 → tombstone
```

The recovered sequence counter is set to the highest replayed sequence:

```text
seq_ = 3
```

The next write receives:

```text
sequence 4
```

This preserves timestamp monotonicity across restarts.

---

## 17. Snapshot Persistence

Snapshots persist complete version history rather than only the latest value.

A value version is stored as:

```text
key<TAB>timestamp<TAB>P<TAB>value
```

A tombstone is stored as:

```text
key<TAB>timestamp<TAB>D
```

During snapshot loading:

```text
P → restore value version
D → restore tombstone
```

The latest-value compatibility map is reconstructed alongside the version history.

This allows historical reads to continue working after snapshot recovery.

---

## 18. Current API Behavior

VeriStore currently has two relevant read paths.

### `get(key)`

Returns the latest value from the compatibility map.

This is optimized for simple latest-value lookup.

### `get_at(key, timestamp)`

Uses the MVCC-aware storage path:

```text
Active MemTable
→ Immutable MemTable
→ SSTables
```

This returns the newest visible version at or before the requested timestamp.

---

## 19. Current MVCC Guarantees

VeriStore currently guarantees:

### Ordered timestamps

Each mutation receives a monotonically increasing sequence number.

### Historical visibility

`GET_AT` returns the newest version visible at the requested timestamp.

### Durable version recovery

WAL replay restores both values and tombstones with their original timestamps.

### Tombstone correctness

A visible tombstone prevents fallback to older storage layers.

### Compaction preservation

Basic compaction preserves historical values and tombstones.

---

## 20. Current Limitations

The current MVCC implementation does not yet provide:

* transactions;
* snapshot registration;
* snapshot isolation;
* serializable isolation;
* conflict detection;
* multi-key atomicity;
* read-your-own-write transaction state;
* active snapshot tracking;
* automatic version garbage collection;
* range snapshots;
* transaction rollback;
* distributed timestamps;
* hybrid logical clocks.

The sequence number is local to one VeriStore instance.

---

## 21. Planned MVCC Extensions

Potential future work includes:

### Snapshot handles

```text
snapshot = store.create_snapshot()
store.get_at(key, snapshot.timestamp)
store.release_snapshot(snapshot)
```

### Oldest active snapshot tracking

```text
oldest_active_timestamp =
  minimum timestamp of active snapshots
```

### Version garbage collection

Remove versions that are older than the oldest active snapshot and no longer required for visibility.

### Tombstone cleanup

Drop tombstones only when all older versions have been removed and no lower storage level can contain the deleted key.

### Transaction support

Add:

```text
begin
read
write
commit
abort
```

### Distributed timestamp integration

Coordinate MVCC versions with:

* Raft log indexes;
* hybrid logical clocks;
* transaction timestamps;
* replicated commit positions.

---

## 22. Example

Given:

```text
PUT item v1
PUT item v2
DELETE item
```

with timestamps:

```text
item@1 = v1
item@2 = v2
item@3 = tombstone
```

Expected reads:

```text
GET_AT(item, 0) → NotFound
GET_AT(item, 1) → v1
GET_AT(item, 2) → v2
GET_AT(item, 3) → Tombstone
GET_AT(item, 4) → Tombstone
```

After restart, WAL replay must produce the same results.

After SSTable flush, the same results must remain visible.

After compaction, the same results must remain visible.

This invariant is exercised by the MVCC and compaction demos.

---

## 23. Summary

VeriStore’s MVCC layer is based on four core ideas:

```text
monotonic sequence numbers
        │
        ▼
version chains per key
        │
        ▼
newest visible version <= read timestamp
        │
        ▼
explicit Value / Tombstone / NotFound states
```

These rules are applied consistently across:

* active MemTables;
* immutable MemTables;
* WAL recovery;
* snapshots;
* SSTables;
* sparse-index lookups;
* Bloom-filter-assisted reads;
* compaction.

The current implementation provides a foundation for future snapshot tracking, transaction isolation, and safe MVCC garbage collection.
