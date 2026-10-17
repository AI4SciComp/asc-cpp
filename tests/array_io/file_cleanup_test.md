# Portable file cleanup acceptance

`asc_cpp.dense.file_cleanup_test` and `asc_cpp.sparse.file_cleanup_test` compile
on every ordinary runtime lane, including shared libraries. Each supplies a
per-call opener to an unsupported private path-helper implementation. Public
helpers use that identical body with the default Core File opener. No public
test API, global registry, symbol interposition or second cleanup implementation
is introduced. Matrix Market public declarations and documentation are unchanged;
their existing bodies now reside in private helpers.

The test handle forwards actual reads, writes, flushes and closes to the linked
Core File and injects deterministic statuses after/before the specified boundary.
Its storage and fault state live within one call. Moves transfer its sole handle;
checks require one explicit close and no implicit destructor close. This is a
portable production-control-flow test, not proof that a particular OS can be
forced to make `fclose` fail. The retained Linux/static GNU-wrap installed tests
remain separate evidence for that actual libc failure path. Public save/load
controls execute without the injected opener in every portable profile.

There are 40 Dense and 68 Sparse profiles: real/complex-double owners, Dense
ranks0–4 (rank0 has no empty-array counterpart), COO ranks0–4 including an empty
rank0 structure, and empty/nonempty CSR/CSC. Native text/binary run throughout;
Matrix Market runs only its promised rank2 domain. The scalar-neutral cleanup
branch is grouped across scalar widths; the separate 62-case installed suite
retains all12 encodings and nonempty rank2 coverage. Structure/owner destruction
is tested separately for COO/CSR/CSC and real/complex storage. These finite
classes supplement the format/rank/scalar matrix; they do not remove its rows.

Each profile runs six saves (success, close-only, flush+close, write+close with
no attempted flush, post-open output-budget failure+close, flush-only), four
loads (success, close-only, read+close, resource rejection+close), two independent
malformed-header loads (with/without close failure), and two uninjected public
save/load controls. Actual zero-byte allocation requests remain observable; the
resource-rejection expectation is bound to requests made by the valid control.
No allocation is invented for a path that makes no request.

Assertions cover primary category and I/O native code, secondary close category,
byte consumption/progress, report phase/publication, close/flush attempts and
resource disposal before failure returns. Dense native early-Prepare failure
retains its original phase; the other existing load helpers record trailer on
close failure. The test does not impose a uniform phase policy absent from the
contract. Owner publication waits for checked close. Failed writes can leave a
prefix; no atomic replacement or durability claim is made.

CTest provides each module a separate scratch directory under its out-of-source
build. Run `ctest --test-dir <build> --no-tests=error --verbose -R
'^asc_cpp\.(dense|sparse)\.file_cleanup_test$'` after building both targets.
Platform/linkage credit requires that actual command's corresponding execution;
Linux success alone does not close Windows/macOS evidence.
