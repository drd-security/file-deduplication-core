# File Deduplication Core

A C library implementing an efficient exact-file deduplication ADT using a **three-stage filter: file size -> 64-bit tabulation hash -> byte-for-byte verification**.

## Why this design?

Comparing every pair of files is unnecessarily expensive. The implementation eliminates candidates as early as possible:

1. Files with different sizes cannot be duplicates.
2. Files with different hashes cannot be duplicates.
3. Only equal-size, equal-hash candidates require an exact byte comparison.

The final comparison guarantees correctness even if a hash collision occurs.

## Public API

```c
FILEDEDUP FDInit(void);
int FDCheck(FILEDEDUP fd, char *filepath);
char **FDDump(FILEDEDUP fd, int *length);
```

## Data structures

- First-level hash table: buckets files by exact size.
- Second-level hash table: groups candidates by 64-bit tabulation hash.
- Duplicate groups: linked lists of paths sharing verified content.
- Tail pointers: append new duplicate paths in constant time.
- `pending_path`: delays hashing the first file of a previously unseen size until a second same-size file appears.

## Complexity

For a file of length `L`, normal insertion is dominated by reading/hash computation and is approximately `O(L)` on average. Hash-table lookups are expected `O(1)`, while `FDDump` is linear in stored/output information.

## Build and tests

```bash
cd tests
make run
```

The test suite covers invalid input, missing files, single files, exact duplicates, same-size different-content files, different sizes, empty files, large files, deferred hashing, and multiple duplicate groups.

## Academic context and contribution

Two-person programming-techniques project. I completed the majority of the implementation and integration; my teammate also contributed. The design emphasized algorithms/data structures, asymptotic behavior, performance, code quality, and a test plan.

## Relationship to the Java project

This project is the low-level predecessor of [`file-deduplication-service`](../file-deduplication-service/), where the same product idea is revisited as an extensible Java service with exact and similar-image engines.

## Publication status

The original report, submission metadata, and archive are excluded. See [NOTICE.md](NOTICE.md).
