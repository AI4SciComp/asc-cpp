# Milestone 5 Independent Verification Review

Status: Complete; accepted after two resolved integration findings

Date: 2026-07-26

Role: independent verification engineer

Scope: **Milestone 5 — random storage generation**

## Independence and method

The verifier read the full runbook, frozen milestone contract and ownership
ledger, accepted ADRs, architecture testing strategy, and existing public
core/random/dense/sparse contracts. The structural, sequence, state-advance,
transaction, allocation, and concurrency oracles were frozen in
`verification-design.md` before either new production facet header or another
agent report was inspected.

No MdeCpp or user-deleted asc-cpp random implementation, test, vector, table,
or prose was inspected or copied. Expected storage mappings are calculated
from the public Philox word and `Uniform01` APIs using verifier-owned ordinal,
priority, coordinate, and canonical-order logic. Existing Milestone 2 tests
remain the independent evidence for the engine and scalar transforms
themselves.

The verifier modified no production, CMake, package, general documentation,
manifest, or provider file.

## Verification-owned files

```text
tests/random_dense/
  allocation_counter.cc
  allocation_counter.h
  random_dense_test.cc
  test_support.h

tests/random_sparse/
  random_sparse_test.cc
  test_support.h

tests/compile/
  m5_random_contract.cc
  m5_random_multi_tu.h
  m5_random_multi_tu_a.cc
  m5_random_multi_tu_b.cc
  m5_random_multi_tu_main.cc
  m5_random_negative_const_dense.cc
  m5_random_negative_missing_dense_facet_header.cc
  m5_random_negative_missing_sparse_facet_header.cc
  m5_random_negative_result_copy.cc
  m5_random_negative_unsupported_dense_scalar.cc
  m5_random_negative_unsupported_sparse_scalar.cc

tests/consumer/random_dense/main.cc
tests/consumer/random_sparse/main.cc
tests/consumer/cpp/main.cc

benchmarks/random_storage/
  allocation_counter.cc
  allocation_counter.h
  random_storage_benchmark.cc

docs/development/asc-cpp-m5-random-storage-generation/
  verification-design.md
  verification-review.md
```

Lead-owned CMake files register these sources.

## Findings and resolutions

### M5-VERIFY-01: sparse public-header ODR violation

Initial severity: release blocking

Initial evidence: the first reviewed `include/asc/random/sparse.h` snapshot
defined the ordinary non-template
`internal_random_sparse::StructurePriority(...)` function in a public header
without `inline`. Multiple translation units including the facet would
provide multiple external definitions.

Resolution: production made the helper explicitly `inline`. The verifier's
multi-TU regression includes `<asc/random/sparse.h>` in both A and B
translation units; A instantiates dense fill and B instantiates sparse
generation. Strict GCC 11 and Clang 19 compilation, linking, and execution now
pass, including exceptions-disabled GCC builds.

Status: resolved.

### M5-VERIFY-02: required no-component package case had a missing argument

Initial severity: release blocking integration-test failure

Reproduction:

```text
ctest --test-dir /tmp/asc-m5-verifier-cmake-gcc.vsmVZ7 \
  -L milestone-5 --output-on-failure
```

The first run reached 38/39 passing tests but
`asc_cpp.package.build_tree_components` failed at
`tests/package/package_test.cmake:276`; the lead-owned
`required-no-component` invocation lacked its `expect_core_target` Boolean
argument.

Resolution: the lead supplied the missing argument. Independent rerun:

```text
ctest --test-dir /tmp/asc-m5-verifier-cmake-gcc.vsmVZ7 \
  -R '^asc_cpp\.package\.build_tree_components$' --output-on-failure
```

Result: 1/1 passed in 117.77 seconds. The install/relocation package test had
already passed in the same build. Thus every one of the 39 GCC M5-selected
tests has current passing evidence after the correction.

Status: resolved.

### Verifier harness correction

The initial positive contract used nonexistent
`std::copy_assignable`. It was corrected to the C++20 trait
`std::is_copy_assignable_v`. This was a test-only compile defect, not a
production finding. Strict GCC and Clang positive and negative matrices pass
after correction.

## Dense evidence

The dense runtime suite verifies:

- rank-zero `double` consumes two words and writes the exact public-transform
  oracle;
- a zero extent writes nothing and returns even the maximum input offset
  unchanged;
- `LayoutLeft`, `LayoutRight`, and unique padded `LayoutStride` receive the
  same logical values;
- padding retains its sentinel values;
- logical ordinal-to-word mapping is dimension-zero-fastest;
- whole generation equals three explicitly addressed logical partitions for
  both `float` and `double`;
- reruns with the same addresses are identical;
- next offsets advance by exactly one word per `float` and two per `double`;
- device placement and offset overflow fail before mutation;
- a successful measured call makes zero general allocations; and
- independent destinations generated concurrently equal their serial public
  word oracles.

## Sparse evidence

The sparse runtime suite verifies:

- rank zero with count zero, one, and invalid two;
- zero-extent count zero and invalid nonzero count;
- count-zero maximum offsets remain unchanged;
- static rank two and dynamic rank three;
- zero, partial, and full exact counts;
- negative and excessive count rejection before resource allocation;
- independently calculated high/low 64-bit priorities;
- ordinal tie comparison, exact selected coordinates, canonical
  lexicographic order, and uniqueness;
- exact canonical-position `float` and `double` value sequences;
- exact structure and value next offsets;
- same-address deterministic reruns;
- rejection of equal structure/value `(stream, subsequence)` pairs before
  allocation;
- coordinate invariance under value-domain changes;
- canonical value-sequence invariance under structure-domain changes;
- retention of an exactly generated `0.0F` entry;
- structure and value offset overflow before allocation;
- non-host resource rejection before allocation;
- first-allocation failure with no live allocation;
- second-allocation failure with the first allocation released exactly once;
- exactly two successful nonempty builder allocations, no third workspace
  allocation, and exactly-once final release; and
- concurrent independent resources/results equal serial runs.

The explicit-zero address was derived locally by a deterministic scan of the
public Philox/transform APIs. The suite repeats the bounded scan, prints the
test location on failure, and verifies that the first found address is
`6156830` for stream `0x7250`, subsequence `0x3812`.

## Compile, dependency, and consumer evidence

Both new public headers compile alone with exceptions enabled and disabled.
The positive contract proves exact result types, accepted `float`/`double`
operations, rejected const/integral participation, and move-only sparse
ownership.

All six negative sources fail compilation for their intended contract:

```text
const dense destination
base random umbrella without dense facet header
base random umbrella without sparse facet header
sparse result copying
integral dense scalar
integral sparse scalar
```

Mechanical source and CMake audits plus architecture tests confirm:

```text
ASC::random_dense  -> ASC::random;ASC::dense
ASC::random_sparse -> ASC::random;ASC::sparse
ASC::random        -> no storage target
dense/sparse       -> no random include or target
```

No provider SDK include or provider type appears in either facet header.

Isolated package consumers passed for random base, random-dense only,
random-sparse only, and aggregate `cpp`. The M5 GCC selection also passed
build-tree, copied-build-tree, install, relocation, paths with spaces,
subproject, static package, unknown/unavailable component behavior, and
package-registry preservation. Dense-only facet imports no sparse facet, and
sparse-only facet imports no dense facet.

## Exact validation evidence

### Strict direct GCC 11.4

Directory: `/tmp/asc-m5-verifier-direct.HhApWv`

Flags:

```text
-std=c++20 -Wall -Wextra -Wpedantic -Werror
```

Results:

- dense runtime: passed;
- sparse runtime: passed;
- positive API contract: passed;
- two-facet multi-TU compile/link/run: passed;
- exceptions-disabled contract and multi-TU: passed;
- six negative cases: 6/6 expected compile failures.

### Strict direct Clang 19

Directory: `/tmp/asc-m5-verifier-clang-direct.bgD8hQ`

Results with the corresponding strict flags:

- dense runtime: passed;
- sparse runtime: passed;
- positive API contract: passed;
- two-facet multi-TU compile/link/run: passed.

### Fresh CMake GCC Debug/static/install-enabled

Directory: `/tmp/asc-m5-verifier-cmake-gcc.vsmVZ7`

Configuration:

```text
cmake -S . -B /tmp/asc-m5-verifier-cmake-gcc.vsmVZ7 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build /tmp/asc-m5-verifier-cmake-gcc.vsmVZ7 --parallel 4
```

Configure and complete build passed. The selected M5 run has passing evidence
for all 39 tests after the one corrected package rerun. This includes 17
consumer tests, both package actions, six negative cases, both runtime suites,
the benchmark, header/no-exception checks, multi-TU, and architecture audits.

### Fresh CMake Clang 19 Debug/shared/install-enabled

Directory: `/tmp/asc-m5-verifier-cmake-clang.hvXgt6`

Configuration used `CMAKE_CXX_COMPILER=/usr/bin/clang++-19`,
`BUILD_SHARED_LIBS=ON`, warnings as errors, tests, and install enabled.
Configure and complete build passed.

Command:

```text
ctest --test-dir /tmp/asc-m5-verifier-cmake-clang.hvXgt6 \
  -L milestone-5 -LE 'package|consumer' --output-on-failure
```

Result: 19/19 passed. This contains the runtime, positive/negative compile,
header/no-exception, multi-TU, benchmark, and architecture/dependency cases.

### Sanitizers

Clang 19 ASan+UBSan final directory:
`/tmp/asc-m5-verifier-clang-sanitize-final.mETcPI`

Both complete verifier-owned dense and sparse runtime suites passed with:

```text
-fsanitize=address,undefined -fno-omit-frame-pointer
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
UBSAN_OPTIONS=halt_on_error=1
```

No sanitizer finding occurred.

Clang 19 TSan evidence:

- complete sparse adversarial runtime suite passed at
  `/tmp/asc-m5-verifier-clang-tsan-sparse-final.ilCgzH`;
- a dedicated dense independent-destination concurrency executable passed at
  `/tmp/asc-m5-verifier-clang-tsan-dense.9wj8i2`.

The complete dense adversarial executable is not TSan-linkable because its
deliberate global `new`/`delete` allocation-count hooks conflict with Clang
TSan's own allocation interceptors. This is a test-harness limitation, not a
production race finding; the dense concurrency path itself has TSan runtime
evidence.

### Formatting

`clang-format-19 --dry-run --Werror` passed for all verifier-owned C++ sources
and headers. `git diff --check` passed.

## Performance smoke evidence

Direct GCC 11 Release-like benchmark:

```text
compiler=GCC 11.4.0
configuration=Release-like
backend=serial-reference
dense_shape=64x64
dense_layout=unique-padded-stride
dense_iterations=500
dense_allocations_in_operations=0
dense_elapsed_us=28456
dense_next_offset=2048000
dense_checksum=2046.81
sparse_shape=32x32
sparse_count=64
sparse_iterations=20
sparse_resource_allocation_calls=40
sparse_resource_deallocation_calls=40
sparse_elapsed_us=32329
sparse_next_structure_offset=40960
sparse_next_value_offset=2560
sparse_checksum=39724
```

This is allocation/state-advance smoke evidence only. It establishes no speed
guarantee or stable regression threshold.

## Provider and GPU classification

The serial CPU reference path is runtime-tested with GCC and Clang.

GPU evidence for Milestone 5 is exactly **skipped**. The facet has no GPU
option, target, source, provider discovery, device compilation, execution, or
CPU/GPU parity claim. Local toolkit or hardware inventory is not
configure-tested, compile-tested, runtime-tested, or parity-tested M5
evidence.

## Remaining gaps and risks

- MSVC and AppleClang were not locally available to this verifier. Lead CI and
  portability review own those remaining platform claims.
- No constructible non-serial `ExecutionContext` exists in the CPU-only M5
  product, so unavailable CUDA context creation is tested but the operation's
  non-serial rejection branch cannot be invoked through a valid current public
  object.
- Sparse selection/finalization are intentionally quadratic reference paths;
  performance evidence is smoke-only.
- Statistical subset uniformity is explicitly not claimed or tested.
- Distinct random addresses are not required to produce distinct results.
- Exact CPU/GPU bit identity is not claimed.
- Pre-1.0 source/ABI and sequence compatibility remain governed by ADR 0018.

No unresolved release-blocking verification finding remains.
