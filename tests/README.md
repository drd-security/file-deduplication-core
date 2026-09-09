# Test suite

The test suite exercises the public `FILEDEDUP` API through representative black-box scenarios and a few cases chosen specifically to stress internal design decisions.

## Coverage

- invalid/null arguments;
- inaccessible or missing files;
- no-duplicate inputs;
- two and three identical files;
- empty files;
- large files;
- same-size files with different content;
- different-size files;
- deferred/pending-file hashing cases;
- multiple independent duplicate groups.

## Run

```bash
make run
```

The Makefile compiles `test_filededup.c` together with the implementation in `../src` and executes both correctness tests and the included performance study.
