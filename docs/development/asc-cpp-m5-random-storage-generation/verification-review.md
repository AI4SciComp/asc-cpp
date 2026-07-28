# Milestone 5 Independent Verification Review

Status: Pass; no unresolved implementation defect

Date: 2026-07-28

Role: Independent verification agent

## Independence

`verification-design.md` froze the structural, sequence, state-advance,
failure-transaction, and allocation oracles before either Milestone 5
production header existed. Production was inspected only after that freeze.

No MdeCpp/deleted asc-cpp implementation, test, vector, table, prose, or
mechanical translation was inspected or copied. Test expectations are derived
from the approved contract and already-public project-owned
Philox/Uniform01/Core/Dense/Sparse behavior.

## Verdict

The two random storage facets conform to the frozen Milestone 5 contract in
the independently tested CPU scope:

- Dense consumes exact words in dimension-zero-fastest logical order,
  independent of physical layout, and returns the first unused offset.
- Sparse derives two-word priorities for every logical coordinate when the
  count is nonzero, selects exact-count unique coordinates by
  `(priority, ordinal)`, publishes canonical order, and generates values only
  by canonical stored position.
- Both APIs reject invalid metadata/address/placement before mutation or
  allocation, as applicable.
- Dense performs zero process allocations during the operation.
- Sparse performs exactly the coordinate builder's two output allocation
  calls and no workspace allocation; rollback and release are exactly once.
- The generation owner remains move-only.
- Facets are separately consumable and the aggregate consumer is usable with
  a no-component lookup.

No production change was requested after independent source review.

## Independent runtime evidence

### Dense

Covered:

- rank zero and zero extent;
- `LayoutLeft`, `LayoutRight`, and unique padded `LayoutStride`;
- exact `float` and `double` values from independent word-address formulas;
- untouched physical padding;
- deterministic rerun;
- whole/partition equivalence with checked partition offsets;
- independent objects on two threads;
- exact float/double next offsets;
- offset overflow before mutation;
- device-placement rejection before mutation;
- zero process allocations around the operation.

### Sparse

Covered:

- rank zero with count zero and one;
- zero extent with count zero and invalid nonzero counts;
- static, dynamic, and mixed extents;
- zero, partial, and full exact counts;
- an independent materialize/sort priority oracle, structurally different
  from production's repeated-scan implementation;
- exact uniqueness and lexicographic canonical order;
- exact float and double canonical stored-position value sequences;
- equal `(stream, subsequence)` domain rejection before allocation;
- deterministic rerun and independent objects on two threads;
- unchanged coordinates when only the value address changes;
- unchanged canonical value sequence when only the structure address changes;
- structure and value offset overflow before allocation;
- negative/excess count and non-host resource rejection before allocation;
- first- and second-allocation failure rollback;
- exactly-once release and zero unknown/double deallocation;
- exactly two declared successful output allocation calls and no process
  workspace allocation.

Explicit-zero retention uses an independently searched fixed address:

```text
stream:       191
subsequence:  193
offset:       35873479
raw word:     206
float value:  0
```

A scalar sparse generation with count one retains that stored zero and
publishes `nnz == 1`.

## Compile and package evidence

Compile contracts cover:

- both facet headers alone with exceptions enabled and disabled;
- positive float/double API concepts;
- rejection of integral and cv/const unsupported cases;
- move-only Sparse generation ownership;
- multi-TU ODR use of both facets;
- negative const Dense destination;
- negative integral Dense and Sparse scalars;
- negative generation-result copy;
- negative facet visibility through base `asc/random.h`.

Consumer projects check exact imported target kinds, direct edges, component
closures, forbidden sibling absence, version `0.5.0`, build-tree use, install,
relocation, paths with spaces, and no-component aggregate use.

## Exact independent commands and results

GCC 11.4 strict static Debug configure:

```sh
cmake -S . -B build/m5-verification-make \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
```

Result: pass.

```sh
cmake --build build/m5-verification-make --parallel 2
```

Result: pass, including every M5 compile/runtime/allocation/benchmark target.

```sh
ctest --test-dir build/m5-verification-make --output-on-failure \
  -R 'random_dense|random_sparse|m5_|random_storage'
```

Result: pass, 18/18.

```sh
ctest --test-dir build/m5-verification-make --output-on-failure \
  -R '^asc_cpp\.consumer\.cpp\.'
```

Result: pass, 2/2.

Clang 19.0 strict static Debug configure:

```sh
cmake -S . -B build/m5-verification-clang -G 'Unix Makefiles' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build build/m5-verification-clang --parallel 2
```

Result: pass.

The first selective Clang test invocation passed 16/18. Its two install tests
failed because the selective target build had intentionally not built the
unrelated Utilities archive that the whole-project install rule includes.
After the complete build above, the identical command passed:

```sh
ctest --test-dir build/m5-verification-clang --output-on-failure \
  -R 'random_dense|random_sparse|m5_|random_storage'
```

Result after complete build: pass, 18/18.

```sh
ctest --test-dir build/m5-verification-clang --output-on-failure \
  -R '^asc_cpp\.consumer\.cpp\.'
```

Result: pass, 2/2.

Formatting:

```sh
clang-format-19 --dry-run --Werror \
  tests/random_dense/*.cc tests/random_dense/*.h \
  tests/random_sparse/*.cc tests/random_sparse/*.h \
  tests/compile/m5_*.cc tests/compile/m5_*.h \
  tests/consumer/random_dense/*.cc \
  tests/consumer/random_sparse/*.cc \
  tests/consumer/cpp/*.cc \
  benchmarks/random_storage/*.cc benchmarks/random_storage/*.h
```

Result: pass.

## Performance and allocation observation

The benchmark is an observation, not a speed gate. Both compilers produced
identical checksums and offsets.

GCC 11.4 Debug:

```text
dense left:  128x128, 100 repetitions, 0 operation allocation calls,
             597588926 ns, next offset 16401,
             checksum 928521972971142723
dense right: 128x128, 100 repetitions, 0 operation allocation calls,
             630284063 ns, next offset 16401,
             checksum 10828367647995593407
sparse COO:  64x64, count 256, 10 repetitions,
             20 resource/process allocation calls, 20 deallocations,
             51200 allocated bytes, 0 live allocations,
             6693198470 ns, next offsets 8295/369,
             checksum 1516538221932305239
aggregate checksum: 8664949311000451886
```

Clang 19.0 Debug:

```text
dense left:  701204631 ns, same allocation/offset/checksum evidence
dense right: 815018362 ns, same allocation/offset/checksum evidence
sparse COO:  8860059257 ns, same allocation/offset/checksum evidence
aggregate checksum: 8664949311000451886
```

Elapsed time is environment-sensitive and establishes no regression
threshold.

## Findings and resolutions

1. The initial benchmark printed a literal Dense allocation claim without a
   process allocation probe. Resolved by adding an owned global allocation
   probe, measuring every Dense fill and Sparse generation, and making
   unexpected allocation counts fail the benchmark.
2. The thread reproducibility test initially passed an unparenthesized
   comma-containing `std::array` initializer to a test macro. Resolved by
   binding both coordinate pairs to local arrays before comparison.
3. Explicit-zero retention was not initially an executable case. Resolved
   with the fixed independently searched address recorded above.
4. Benchmark compiler reporting initially used unconditional `__VERSION__`.
   Resolved with guarded MSVC, Clang, GCC/compatible, and unknown branches.

All four resolutions are inside verification-owned files. No finding remains
open.

## Evidence limitation and remaining risk

The production APIs contain an explicit non-serial backend guard. A direct
non-serial facet invocation is not constructible through provider-free
Milestone 5 Core: `ExecutionContext::Create(Backend::kCuda, ...)` rejects the
unavailable provider before returning a context. Verification therefore
claims source/branch inspection plus public context-factory rejection and
runtime non-host placement rejection, not a non-serial facet runtime
invocation. Milestone 6 provider contexts are required to exercise that
branch through supported public construction.

The Sparse reference algorithm is intentionally
`O(exact_count * logical_size)`; benchmark timing is observational and does
not justify a scalability claim.

GPU evidence for Milestone 5 is exactly **skipped**.

