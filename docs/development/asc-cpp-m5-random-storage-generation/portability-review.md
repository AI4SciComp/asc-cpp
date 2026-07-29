# Milestone 5 Portability, GPU, and Performance Review

Status: complete on 2026-07-28

Role: independent review-only portability/GPU/performance wave

## Scope and authority

This review covers only the frozen Milestone 5 CPU random-storage facets and
their implementation, verification, package integration, and benchmark. It
was performed against:

- the Milestone 5 contract, ownership ledger, and provenance record;
- the six-module architecture, ADRs 0003, 0004, 0007--0009, 0011, 0012,
  0015, 0017, and 0018;
- the dependency and capability manifests and backend matrix;
- both public facet headers and their exact target/package definitions;
- the independent verification design, tests, consumers, and benchmark; and
- the resolved lead integration and Clang shared-build package matrix.

No production, verification, CMake, manifest, or prior documentation file was
edited by this role. No later milestone or provider was treated as
implemented.

## Review result

No unresolved release-blocking portability, undefined-behavior, provider, or
performance-evidence finding remains.

### C++20 and source policy

Both public headers:

- are self-contained C++20 `.h` files with full-path guards;
- put public names directly in `namespace asc`;
- use only `internal_random_dense` and `internal_random_sparse` for
  implementation helpers;
- directly include their Core, owning-storage, and Random requirements;
- contain no optional SDK, provider, third-party, entropy, mutable-global, or
  thread-local dependency; and
- pass the repository's Google-derived `clang-format-19` configuration.

The concepts accept exactly unqualified `float` and `double`. Const dense
destinations and unsupported scalar types fail at the public template
boundary. GCC, Clang, and icpx instantiated both facets with strict conversion
warnings and with exceptions/RTTI disabled.

### Arithmetic and undefined-behavior audit

Dense generation converts the already validated nonnegative logical size to
`uint64_t`, checks the complete scalar-dependent word count, and checks the
returned offset before entering the write loop. A `DenseLayout` has already
proved its maximum nonnegative physical offset and representable
`size_t` span. Recomputed coordinate, stride, and physical offsets therefore
remain within that proof. The final double word's `offset + 1` is covered by
the full preflight advance.

A zero extent yields zero loop iterations, so no null data pointer or zero
extent is dereferenced or used as a divisor. Rank zero skips coordinate
decoding and writes its one scalar at physical offset zero.

Sparse generation validates count, host placement, address-domain separation,
both word-count multiplications, and both returned offsets before allocation.
Nonzero count implies every extent used as an ordinal divisor is positive.
The repeated priority scan uses only unsigned arithmetic. Its inner
`structure_offset + 2 * ordinal` and the value word offsets are bounded by the
preflight advances.

Signed `nnz_t` and logical sizes are checked before unsigned conversion.
Builder allocation byte counts and alignment are checked by the existing
Sparse/Core contracts. Rank-zero coordinate buffers legitimately request zero
bytes, retain a valid resource association, and expose only zero-length
coordinate spans. No uninitialized priority or coordinate is observed:
`exact_count <= logical_size` guarantees a next ordered candidate for each
selection step.

No strict-aliasing, lifetime-extension, data-race, out-of-bounds, signed
overflow, invalid shift, hidden narrowing, or division-by-zero path was found
in the approved input domain. Clang ASan+UBSan runtime evidence was clean.

### Transactions, ownership, and concurrency

Dense context, placement, size, count, and offset failures occur before the
first destination write. Success borrows the view synchronously and performs
no allocation, transfer, packing, synchronization, dispatch, or fallback.

Sparse failures before builder creation make no allocation. Failure of the
second builder allocation releases the first exactly once. Every later
operation is non-failing under the completed preflight and builder invariants;
if an invariant-produced `Result` nevertheless fails, local move-only owners
still release their buffers before returning. A successful result retains the
caller resource association and releases both buffers exactly once.

The facets and base engine have no mutable shared state. Independent dense
buffers and independent sparse resources reproduced identical sequences in
parallel tests. Caller synchronization remains required for overlapping
storage, shared non-thread-safe resources, and owner/resource destruction.
Focused GCC ThreadSanitizer runs passed both thread tests when launched with
ASLR disabled to work around this host's ThreadSanitizer mapping conflict.

The current public Core cannot construct a usable non-serial context: an
unsupported provider request fails before returning `ExecutionContext`.
Consequently, Milestone 5 can runtime-test host-placement rejection but cannot
invoke a facet with a non-serial public context. Both facet implementations
still perform the required backend check first. Direct non-serial invocation
evidence remains properly deferred until such a context is approved.

### Static, shared, DLL, and hosted-toolchain audit

The facets are functional interface targets because all new behavior is
rank/type-dependent template code. They add no compiled symbol or facet export
macro. Their exact direct links supply the existing compiled Random and
Dense/Sparse symbols and their visibility/import definitions:

```text
ASC::random_dense  -> ASC::random;ASC::dense
ASC::random_sparse -> ASC::random;ASC::sparse
```

The base compiled targets retain their static-definition and Windows
`dllimport`/`dllexport` handling. The facet templates and non-template
internal helpers are inline where required, and representative multi-TU
instantiation links without an ODR conflict.

Clang 19 Release/shared compilation, build-tree consumption, install,
relocation through paths containing spaces, and the aggregate package all
passed. Classic MSVC and AppleClang are not installed on this host. Source
review found no production reliance on GNU-only language extensions or
`__VERSION__`; the benchmark now reports compiler identity through guarded
MSVC, Clang, GNU-compatible, or unknown branches. Its aligned allocation probe
uses matched `_aligned_malloc`/`_aligned_free` on MSVC.

The two test executables using `std::thread` now link through CMake's built-in
`Threads::Threads` target. This supplies portable platform thread compile/link
flags without changing any ASCCpp product or exported-package dependency.

### Provider isolation and GPU classification

The production headers, target links, manifests, package components, and
source scans contain no CUDA, HIP, SYCL, OpenMP, TBB, Eigen, BLAS/LAPACK,
oneMKL, cuRAND, or other provider/SDK dependency. A base Random consumer
imports no storage target, and each facet imports no sibling storage/facet.

GPU evidence for Milestone 5 is exactly **skipped**. No GPU configure,
provider compile, device runtime, or CPU/GPU parity operation was performed or
claimed.

## Allocation and performance evidence

The benchmark is an observation, not a speed gate. It records compiler,
configuration, scalar, shape/count, layouts, repetitions, allocation counts,
elapsed nanoseconds, next offsets, and stable checksums. Its timed region also
includes checksum traversal, so the elapsed values are end-to-end observations
and not isolated kernel timings.

Three consecutive Clang 19 Release/shared runs produced:

| Case | Work per run | Allocation evidence | Elapsed range |
| --- | --- | --- | --- |
| Dense left | `float`, 128x128, 100 fills | zero operation allocations | 27,610,181--29,354,828 ns |
| Dense right | `float`, 128x128, 100 fills | zero operation allocations | 28,181,595--30,457,651 ns |
| Sparse coordinate | `float`, 64x64, count 256, 10 generations | 20 declared allocations, 51,200 bytes, 20 releases, zero live | 298,134,753--352,218,485 ns |

Every run reported:

```text
dense next_offset=16401
sparse next_structure_offset=8295
sparse next_value_offset=369
left checksum=928521972971142723
right checksum=10828367647995593407
sparse checksum=1516538221932305239
aggregate_checksum=8664949311000451886
```

The allocation probe confirms that sparse process allocations equal the
coordinate builder's two declared allocations per generation. No hidden
selection workspace appears. The observed sparse cost is consistent with the
frozen `O(exact_count * logical_size)` repeated-scan reference algorithm; no
optimized-provider or stable throughput claim is made.

## Findings and resolutions

### P1: M5 C++ sources were not repository-formatted

Initial severity: release blocking.

The first `clang-format-19 --dry-run --Werror` found drift in both production
headers and the verification, consumer, and benchmark sources.

Resolution: the lead mechanically formatted only the bounded M5 C++ files.
The repeated dry run and `git diff --check` pass with zero diagnostics. Closed.

### P2: benchmark registration preceded its allocation-probe files

Initial severity: transient integration blocker.

During the agent handoff, benchmark CMake referred to
`benchmarks/random_storage/allocation_probe.cc` before the verifier's owned
file appeared.

Resolution: the verifier completed the approved
`allocation_probe.{h,cc}` pair, registration regenerated, and both ordinary
and sanitizer benchmark targets compile and run. Closed.

### P3: benchmark compiler reporting was GNU-specific

Initial severity: medium portability defect.

The initial benchmark used `__VERSION__` unconditionally, which classic MSVC
does not guarantee.

Resolution: compiler reporting now has guarded `_MSC_FULL_VER`,
`__clang_version__`, `__VERSION__`, and unknown branches. Strict Clang
compilation and runtime passed; MSVC remains a source-audited hosted risk.
Closed.

### P4: the architecture capability checker retained the pre-M5 threshold

Initial severity: integration blocker.

The first Clang/shared run rejected the correct `runtime-tested` M5 capability
manifest because the checker still expected `proposed`.

Resolution: the lead advanced the runtime-tested threshold through Milestone
5. The architecture tests pass 3/3. Closed.

### P5: the subproject fixture rejected newly approved facets

Initial severity: integration blocker.

The cumulative subproject fixture still treated `ASC::random_dense`,
`ASC::random_sparse`, and `ASC::cpp` as forbidden.

Resolution: the fixture now requires all three and links its consumer through
`ASC::cpp`. The regenerated subproject test passes. Closed.

### P6: aggregate consumer executable identity was inconsistent

Initial severity: integration blocker.

The generic runner searched for `asc_cpp_cpp_consumer`, while the aggregate
fixture builds `asc_cpp_aggregate_consumer`.

Resolution: component registration now passes the aggregate's actual
executable name for `cpp`. Build-tree and relocated aggregate consumers pass.
Closed.

### P7: required no-component package case omitted one argument

Initial severity: integration blocker.

The Milestone 5 package matrix invoked `_configure_consumer` without the
required expected-Core-target argument for the required no-component case.

Resolution: the missing `TRUE` argument was added. Build-tree and
install/relocation package matrices pass. Closed.

### P8: thread tests relied on host-default pthread linkage

Initial severity: medium test-only portability risk.

The two `std::thread` tests passed on this glibc host but did not request the
portable CMake thread flags needed by some POSIX toolchains.

Resolution: test-only CMake now discovers `Threads` and links only those two
executables to `Threads::Threads`. No production or package dependency
changed. Closed.

## Independent commands and results

### Formatting and source scans

```sh
clang-format-19 --dry-run --Werror \
  include/asc/random/dense.h include/asc/random/sparse.h \
  tests/random_dense/*.h tests/random_dense/*.cc \
  tests/random_sparse/*.h tests/random_sparse/*.cc \
  tests/compile/m5_*.h tests/compile/m5_*.cc \
  tests/consumer/random_dense/*.cc \
  tests/consumer/random_sparse/*.cc tests/consumer/cpp/*.cc \
  benchmarks/random_storage/*.h benchmarks/random_storage/*.cc
git diff --check -- \
  include/asc/random/dense.h include/asc/random/sparse.h \
  tests/random_dense tests/random_sparse tests/compile \
  tests/consumer/random_dense tests/consumer/random_sparse \
  tests/consumer/cpp benchmarks/random_storage
```

Result: pass with zero diagnostics after P1 resolution.

Provider/SDK, hidden-state/allocation, forbidden sibling-include, and namespace
source scans returned no production match.

### Strict compiler instantiation

Both facet templates were instantiated from standard input with:

```text
-std=c++20 -pedantic-errors -Wall -Wextra -Werror
-Wconversion -Wsign-conversion -Wshadow -Wundef
```

Compilers:

```text
g++ 11.4.0
clang++ 19.0.0
icpx 2024.2.0
```

Each compiler passed once normally and once with
`-fno-exceptions -fno-rtti`: 6/6 configurations. A Clang `-Weverything`
instantiation also passed after excluding compatibility, padding,
raw-buffer-policy, static-lifetime, and the already-approved Sparse
explicit-zero floating comparison diagnostics.

### Clang Release/shared/package matrix

```sh
cmake -S . -B build/m5-portability-clang-shared \
  -G 'Unix Makefiles' \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build build/m5-portability-clang-shared --parallel 4
ctest --test-dir build/m5-portability-clang-shared \
  -L milestone-5 --output-on-failure
```

Result after P4--P7 resolution: pass, 39/39. This includes M5 architecture,
strict/disabled-exception headers, positive concepts, multi-TU use, runtime
and allocation suites, benchmark, subproject, build-tree and relocated
component consumers, aggregate, package component matrix, and registry
preservation.

After P8, the same build tree was regenerated through CMake's `FindThreads`,
both thread-test targets were rebuilt, and the focused runtime recheck passed:

```sh
cmake --build build/m5-portability-clang-shared --parallel 4 \
  --target asc_random_dense_thread_partition_test \
           asc_random_sparse_thread_reproducibility_test
ctest --test-dir build/m5-portability-clang-shared \
  -R 'thread_(partition|reproducibility)' --output-on-failure
```

Result: pass, 2/2.

### Clang ASan+UBSan

```sh
cmake -S . -B build/m5-portability-clang-asan-ubsan \
  -G 'Unix Makefiles' \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
env ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/m5-portability-clang-asan-ubsan \
    -R 'asc_cpp\.random_(dense|sparse)|asc_cpp\.random_storage' \
    --output-on-failure
```

Result: pass, 7/7; no address, undefined-behavior, or leak diagnostic.

### GCC ThreadSanitizer

```sh
cmake -S . -B build/m5-portability-gcc-tsan \
  -G 'Unix Makefiles' \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_THREAD_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build build/m5-portability-gcc-tsan --parallel 4 \
  --target asc_random_dense_thread_partition_test \
           asc_random_sparse_thread_reproducibility_test
setarch "$(uname -m)" -R \
  env TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
  ./build/m5-portability-gcc-tsan/tests/random_dense/\
asc_random_dense_thread_partition_test
setarch "$(uname -m)" -R \
  env TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
  ./build/m5-portability-gcc-tsan/tests/random_sparse/\
asc_random_sparse_thread_reproducibility_test
```

Result: pass, 2/2 with no race diagnostic. Ordinary ASLR-enabled invocation
cannot start this host's GCC ThreadSanitizer and reports `unexpected memory
mapping`; disabling ASLR for the child process avoids that environmental
runtime conflict.

## Remaining risks

- Classic MSVC, AppleClang, 32-bit, big-endian, and multi-config generator
  compilation/runtime were not available in this independent local wave.
- External raw-pointer views cannot validate allocation size or lifetime.
  Caller-selected address ranges and resource/thread safety remain caller
  responsibilities.
- Direct non-serial facet invocation cannot be tested until an approved
  non-serial `ExecutionContext` can be constructed.
- The sparse repeated-scan and builder-finalization algorithms are deliberately
  slow reference paths. The benchmark is not a statistical performance study
  and establishes no stable throughput threshold.
- ThreadSanitizer requires an ASLR-disabled child on this host; the clean result
  does not replace broader scheduler/platform concurrency testing.
- GPU evidence is exactly **skipped**.
