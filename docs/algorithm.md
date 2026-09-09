# Algorithm and performance notes

## Deferred hashing

The first file with a new size cannot yet have a duplicate. Instead of reading it immediately, the implementation stores its path in `pending_path`. Its contents are hashed only if a second file of the same size arrives. This saves I/O for workloads dominated by unique file sizes.

## Tabulation hashing

A fixed 32 x 256 table of pseudo-random 64-bit values is initialized once. For each byte, the current table row and the byte value select a 64-bit value, which is XORed into the running hash. The row index cycles through all 32 rows.

## Collision safety

Equal hash values are treated only as candidate matches. The incoming file is compared block-by-block against a representative member of the candidate group before it is accepted as a duplicate.

## Output

`FDDump` returns duplicate path groups in a flat C-string array with `NULL` separators. Groups containing only one file are omitted.
