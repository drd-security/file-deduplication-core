# Test Suite and Performance Study

This directory contains automated tests for the file-deduplication core.

## Coverage

The suite exercises the public API:

- `FDInit`
- `FDCheck`
- `FDDump`

Representative scenarios include invalid arguments, missing files, unique files, exact duplicates, equal-size files with different content, empty files, large files, pending-file behavior, and multiple duplicate groups.

The tests are primarily black-box tests, with a few checks that also exercise implementation details such as deferred hashing and block-by-block comparison of large files.

## Structure

```text
../src/
  filededup.c
  filededup.h
./
  test_filededup.c
  Makefile
  README.md
```

## Run

```bash
make run
```

or from the repository root:

```bash
make test
```
