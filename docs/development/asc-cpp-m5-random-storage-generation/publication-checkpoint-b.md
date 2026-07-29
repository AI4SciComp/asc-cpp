# Milestone 5 Publication Checkpoint B

Status: complete local candidate; publication not authorized

Date: 2026-07-28

Branch: `feature/asc-cpp-m5-random-storage-generation`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the exact cumulative, intentionally uncommitted Milestones 0--4
candidate recorded at the Milestone 4 Publication Checkpoint B

Corrections: None

## Scope result

The frozen **Milestone 5 — random storage generation** contract is satisfied.
The candidate implements deterministic serial/host Dense uniform generation
and exact-count canonical Sparse generation for exactly unqualified `float`
and `double`, using the already-approved explicit Philox address and
`Uniform01` contracts.

The unreleased package candidate advances to 0.5.0. It adds the two functional
interface facets `ASC::random_dense` and `ASC::random_sparse`, and activates
the provider-free aggregate `ASC::cpp`. Their exact direct edges are:

```text
ASC::random_dense  -> ASC::random;ASC::dense
ASC::random_sparse -> ASC::random;ASC::sparse
ASC::cpp           -> ASC::core;ASC::utilities;ASC::expression;ASC::dense;
                      ASC::sparse;ASC::random;ASC::random_dense;
                      ASC::random_sparse
```

Separate production implementation, independent verification,
documentation/API, and portability/GPU/performance waves ran with disjoint
write scopes. The lead alone changed shared root CMake, package, manifest,
test registration, and integration paths. All accepted findings are closed,
and the stabilized post-review tree has been revalidated.

No base Random storage dependency, mutable generator, new distribution,
entropy source, density-mode Sparse generation, compressed generation,
provider, transfer, fallback, third-party dependency, compatibility layer,
Sobol/table work, or Milestone 6 API was implemented.

No commit, push, pull request, merge, tag, release, package-registry write, or
branch deletion occurred.

## 1. Changed files and reasons

This Milestone 5 layer is cumulative over the approved uncommitted
Milestones 0--4 predecessor. Shared files can therefore contain changes from
more than one milestone. The exact repository state is reproduced by:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
```

At checkpoint freeze, the repository reports 437 porcelain entries, 334
staged predecessor paths, 34 tracked unstaged paths, and 207 untracked files.
Milestone 5 added 43 untracked files without altering the predecessor's
staged or tracked-unstaged path counts. The cumulative predecessor work, the
owner's tracked `AGENTS.md` deletion, the ignored local build trees, and the
unrelated modified MdeCpp `Makefile` are preserved.

### Production API

```text
include/asc/random/dense.h
include/asc/random/sparse.h
```

These are the complete new production surface. They implement the approved
type/rank-dependent serial CPU reference behavior. No existing production
header or source implementation was changed for Milestone 5.

### Root build, package, architecture, and live documentation integration

```text
CHANGELOG.md
CMakeLists.txt
CMakePresets.json
README.md
cmake/ASCCppOptions.cmake
docs/README.md
docs/api.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/modules/core.md
docs/modules/dense.md
docs/modules/expression.md
docs/modules/random.md
docs/modules/sparse.md
docs/modules/utilities.md
src/random/CMakeLists.txt
```

These paths advance the candidate to 0.5.0, register/export the two facets and
aggregate, retain exact ASCCMake 0.1.0, expose all nine provider-free
components, document exact sequence/ownership/complexity boundaries, and
record the final runtime-tested serial CPU and skipped GPU evidence.

`Threads::Threads` is discovered only when repository tests are enabled. It
is linked only to the two concurrency test executables and is not a product,
exported-package, or consumer dependency.

### Architecture, compile, runtime, package, consumer, and performance evidence

```text
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/m5_negative_const_dense.cc
tests/compile/m5_negative_copy_generation.cc
tests/compile/m5_negative_integral_dense.cc
tests/compile/m5_negative_integral_sparse.cc
tests/compile/m5_negative_random_base_visibility.cc
tests/compile/m5_random_dense_header.cc
tests/compile/m5_random_facets_odr.h
tests/compile/m5_random_facets_odr_a.cc
tests/compile/m5_random_facets_odr_b.cc
tests/compile/m5_random_facets_odr_main.cc
tests/compile/m5_random_sparse_header.cc
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/cpp/CMakeLists.txt
tests/consumer/cpp/main.cc
tests/consumer/dense/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/random_dense/CMakeLists.txt
tests/consumer/random_dense/main.cc
tests/consumer/random_sparse/CMakeLists.txt
tests/consumer/random_sparse/main.cc
tests/consumer/run_component_consumer.cmake
tests/consumer/run_core_consumer.cmake
tests/consumer/sparse/CMakeLists.txt
tests/consumer/subproject/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/random_dense/CMakeLists.txt
tests/random_dense/allocation_test.cc
tests/random_dense/dense_generation_test.cc
tests/random_dense/test_support.h
tests/random_dense/thread_partition_test.cc
tests/random_sparse/CMakeLists.txt
tests/random_sparse/allocation_test.cc
tests/random_sparse/sparse_generation_test.cc
tests/random_sparse/test_support.h
tests/random_sparse/thread_reproducibility_test.cc
benchmarks/random_storage/allocation_probe.cc
benchmarks/random_storage/allocation_probe.h
benchmarks/random_storage/random_storage_benchmark.cc
```

These paths enforce the exact public-file and dependency boundaries; strict
headers with exceptions enabled/disabled; five expected compile failures;
multi-TU ODR use; independent sequence/priority/value oracles; failure
transactions; deterministic concurrency; exact operation/allocation
evidence; all isolated base/facet/aggregate consumers; no-component lookup;
copied build-tree packages; installation/relocation; and threshold-free
performance observations.

### Contract, provenance, ownership, and independent reviews

```text
docs/development/asc-cpp-m5-random-storage-generation/dependency-audit.md
docs/development/asc-cpp-m5-random-storage-generation/documentation-api-review.md
docs/development/asc-cpp-m5-random-storage-generation/milestone-contract.md
docs/development/asc-cpp-m5-random-storage-generation/ownership.md
docs/development/asc-cpp-m5-random-storage-generation/portability-review.md
docs/development/asc-cpp-m5-random-storage-generation/preflight.md
docs/development/asc-cpp-m5-random-storage-generation/production-self-review.md
docs/development/asc-cpp-m5-random-storage-generation/provenance-record.md
docs/development/asc-cpp-m5-random-storage-generation/publication-checkpoint-b.md
docs/development/asc-cpp-m5-random-storage-generation/verification-design.md
docs/development/asc-cpp-m5-random-storage-generation/verification-review.md
```

These freeze the approved contract, disjoint ownership ledger, predecessor
audit, clean-room boundary, independent oracle design, API/dependency review,
portability and performance findings, resolutions, remaining risks, and this
final local evidence.

## 2. APIs, targets, and direct dependency changes

### Dense generation API

```text
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>
```

The destination element is exactly unqualified `float` or `double`. Float
consumes one Philox word per logical element and double consumes two,
high-word then low-word. Logical dimension zero varies fastest regardless of
physical layout/strides; padding is untouched. Rank zero writes one value and
a zero extent writes/consumes nothing. All context, placement, size,
word-count, and returned-offset checks precede mutation. The operation is
synchronous, borrows the view, and allocates no storage or workspace.

### Sparse generation API

```text
SparseUniform01Generation<Element, ExtentsType>
  array
  next_structure_offset
  next_value_offset

GenerateSparseUniform01<Element>(
    context, extents, exact_count, resource,
    structure_stream, structure_subsequence, structure_offset,
    value_stream, value_subsequence, value_offset)
    -> Result<SparseUniform01Generation<Element, ExtentsType>>
```

The move-only result owns one canonical `CoordinateArray`. Structure and
value must have distinct `(stream, subsequence)` domains. For nonzero count,
two structure words form each logical coordinate's priority; selection is by
`(priority, ordinal)` without replacement and output is canonical. Values use
only canonical stored position and the value address. Explicit zeros are
kept. The reference selection is
`O(exact_count * logical_size)` with `O(rank)` local computational storage.
The only successful allocations are the coordinate builder's two declared
output allocations.

### Targets, package, and dependencies

```text
build target        consumer target     kind       direct ASC dependencies
asc_random_dense    ASC::random_dense   interface  ASC::random;ASC::dense
asc_random_sparse   ASC::random_sparse  interface  ASC::random;ASC::sparse
asc_cpp             ASC::cpp            interface  all six modules and both facets
```

Available components are exactly:

```text
core utilities expression dense sparse random random_dense random_sparse cpp
```

`find_package(ASCCpp 0.5 REQUIRED)` without components requests `cpp`.
Required facet closures contain only their transitive base requirements;
unknown required components fail and unknown optional components do not
corrupt package state.

Direct compile/link/runtime dependencies added by Milestone 5: none.
Configure-time dependency remains exact released ASCCMake 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7`. The test-only standard CMake
`Threads::Threads` edge is not exported.

## 3. Exact commands and pass/fail/skip results

### Repository, branch, and predecessor preflight

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m4-sparse-cpu \
  feature/asc-cpp-m5-random-storage-generation
git status --porcelain=v1
git diff --cached --name-only
git diff --name-only
git ls-files --others --exclude-standard
git worktree list --porcelain
git -C ../asc-cmake status --short --branch
git -C ../asc-cmake rev-parse HEAD
git -C ../asc-cmake describe --tags --exact-match HEAD
git -C /home/yicai/repo/MdeRepo/MdeCpp status --short --branch
git -C /home/yicai/repo/MdeRepo/MdeCpp rev-parse HEAD
```

Result: pass. The active branch is
`feature/asc-cpp-m5-random-storage-generation`. HEAD, `main`, `origin/main`,
M4, and M5 are exactly
`33b261ea33616a6395c4ad3b20646093103344f7`. There is one ASCCpp worktree.
The predecessor manifest was unchanged before the safe branch switch: 420
porcelain entries, 334 staged paths, 34 tracked unstaged paths, and 164
untracked files.

ASCCMake is clean/current `main`, exact tag `v0.1.0`, at `8a7dcba`. MdeCpp is
current `main` at `f6294e9`; its pre-existing modified `Makefile` is untouched.

Repository `main:AGENTS.md`, the architecture package, ADRs 0001--0018,
dependency/capability manifests, backend matrix, previous milestone report,
repository audit, implementation/testing/CI/ASCCMake/provenance guidance,
roadmap, and supplied runbook were read before the contract was frozen. No
material owner decision remained unresolved.

### Final GCC Debug/static

```sh
cmake -S . -B /tmp/asc-cpp-m5-final-gcc-debug \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m5-final-gcc-debug --parallel 4
ctest --test-dir /tmp/asc-cpp-m5-final-gcc-debug \
  -C Debug --output-on-failure -j 4
```

Result: pass with CMake 4.1.2 and GCC 11.4, **161/161**, zero failed.

### Final Clang Release/shared

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m5-final-clang-release-shared \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m5-final-clang-release-shared --parallel 4
ctest --test-dir /tmp/asc-cpp-m5-final-clang-release-shared \
  -C Release --output-on-failure -j 4
```

Result: pass with CMake 4.1.2 and Clang 19.0.0, **161/161**, zero failed.

### Minimum CMake GCC Release/static

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . -B /tmp/asc-cpp-m5-final-cmake325-gcc-release \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build /tmp/asc-cpp-m5-final-cmake325-gcc-release --parallel 4
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir /tmp/asc-cpp-m5-final-cmake325-gcc-release \
  -C Release --output-on-failure -j 4
```

Result: pass with CMake 3.25.0 and GCC 11.4, **161/161**, zero failed.

Each final matrix includes five M5 configure-time expected compile failures;
all five sources failed compilation as required. Independent strict facet
instantiation also passed GCC 11.4, Clang 19, and icpx 2024.2, normally and
with exceptions/RTTI disabled: **6/6**.

### Formatting, dependency, provenance, and static analysis

```sh
clang-format-19 --dry-run --Werror \
  include/asc/random/dense.h include/asc/random/sparse.h \
  tests/random_dense/*.h tests/random_dense/*.cc \
  tests/random_sparse/*.h tests/random_sparse/*.cc \
  tests/compile/m5_*.h tests/compile/m5_*.cc \
  tests/consumer/random_dense/*.cc \
  tests/consumer/random_sparse/*.cc tests/consumer/cpp/*.cc \
  benchmarks/random_storage/*.h benchmarks/random_storage/*.cc
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/dependency_check.cmake
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m4_dependency_check.cmake
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m3_dependency_check.cmake
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m2_dependency_check.cmake
git diff --check
sha256sum LICENSE
command -v clang-tidy-19 || command -v clang-tidy
```

Result: formatting, dependency inventories/policies, patch whitespace, and
license hash pass. The license hash is
`c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4`.
Bounded M5 source/test/consumer/benchmark scans find no MdeCpp, Random123,
provider, or optional SDK token. No local clang-tidy executable exists, so
clang-tidy is **skipped** and remains a hosted-CI gate.

### Resolved integration failures

Before the final matrices:

1. the Sparse thread test passed a comma-containing `std::array` initializer
   directly to a macro; verification bound the values to locals;
2. the first integrated CTest run failed six package/architecture cases
   because the capability threshold, subproject fixture, aggregate executable
   identity, and one no-component helper call still reflected M4; the lead
   corrected those exact registrations, and the focused rerun passed 6/6;
3. test-only thread executables relied on host-default linkage; the lead added
   `Threads::Threads`; and
4. placing `find_package(Threads)` in the test subdirectory initially hid the
   imported target from the deferred root target-inventory scan; discovery
   moved to the root testing block.

All were resolved before the three fresh 161/161 matrices.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Clang ASan and UBSan

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m5-final-clang-asan-ubsan \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m5-final-clang-asan-ubsan --parallel 4
env ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/asc-cpp-m5-final-clang-asan-ubsan \
    -C Debug --output-on-failure \
    --label-exclude 'package|consumer' -j 4
```

Result: **139/139** compatible architecture, compile, CPU runtime,
allocation, concurrency, and benchmark tests pass with zero address,
undefined-behavior, or leak diagnostic. The 22 separately configured
package/consumer subprocess tests are excluded because sanitizer development
link flags are not installed usage requirements; they pass in every
uninstrumented full matrix.

### Standalone LSan

The initial standalone LSan command attempted all seven M5 runtime/allocation/
benchmark targets:

```sh
cmake -S . -B /tmp/asc-cpp-m5-final-clang-lsan \
  -G 'Unix Makefiles' \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_LEAK_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m5-final-clang-lsan --parallel 4 \
  --target asc_random_dense_dense_generation_test \
           asc_random_dense_thread_partition_test \
           asc_random_dense_allocation_test \
           asc_random_sparse_sparse_generation_test \
           asc_random_sparse_thread_reproducibility_test \
           asc_random_sparse_allocation_test \
           asc_random_storage_benchmark
```

Result: configure passed; the allocation-test link **failed as expected for
this incompatible instrumentation combination** because standalone Clang
LSan and the test's deliberate process-wide allocation probe both define the
global `new`/`delete` interceptors. This is not a product link or runtime
failure. The three interposing allocation/benchmark targets are therefore
**skipped under standalone LSan** and remain fully covered by ASan with leak
detection.

The compatible rerun was:

```sh
cmake --build /tmp/asc-cpp-m5-final-clang-lsan --parallel 4 \
  --target asc_random_dense_dense_generation_test \
           asc_random_dense_thread_partition_test \
           asc_random_sparse_sparse_generation_test \
           asc_random_sparse_thread_reproducibility_test
env LSAN_OPTIONS=exitcode=23:report_objects=1 \
  ctest --test-dir /tmp/asc-cpp-m5-final-clang-lsan \
    -R 'asc_cpp\.random_dense\.(dense_generation|thread_partition)_test|asc_cpp\.random_sparse\.(sparse_generation|thread_reproducibility)_test' \
    --output-on-failure -j 4
```

Result: **4/4** pass with zero leak diagnostic.

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

Result: **2/2** pass with zero race diagnostic. Normal ASLR-enabled launch on
this host fails before test execution with GCC TSan's `unexpected memory
mapping`; the ASLR-disabled child is the documented environmental workaround.

### Package, relocation, and isolated consumers

Tests 140--161 in each complete matrix cover:

- nine build-tree/installed isolated component surfaces, including both
  random facets and the aggregate;
- all installed consumers relocated through prefixes containing spaces;
- the cumulative foundations subproject;
- copied build-tree component packages;
- installed component selection and relocation;
- required, optional, no-component, and unknown-component behavior; and
- package-registry preservation.

Result: **22/22** pass in each of the three complete matrices. The 19 tests
carrying the consumer label and the three package-only cases all pass.

Installed export inspection reports:

```text
ASC::random_dense:
  INTERFACE_LINK_LIBRARIES "ASC::random;ASC::dense"
ASC::random_sparse:
  INTERFACE_LINK_LIBRARIES "ASC::random;ASC::sparse"
ASC::cpp:
  INTERFACE_LINK_LIBRARIES "ASC::core;ASC::utilities;ASC::expression;
                            ASC::dense;ASC::sparse;ASC::random;
                            ASC::random_dense;ASC::random_sparse"
```

The facet/aggregate targets are interface-only and add no library artifact.
Relocated `libasc_random.so`, `libasc_dense.so`, and `libasc_sparse.so` each
retain `RUNPATH [$ORIGIN]`; their dynamic needs contain `libasc_core.so` and
platform C++ runtimes only, with no new external dependency.

## 5. CPU/GPU provider evidence

The serial CPU provider-free behavior is **runtime-tested**:

- Dense rank-zero/zero-extent, left/right/padded layouts, exact float/double
  values, untouched padding, whole/partition equivalence, offset advance,
  transactional rejection, zero operation allocations, and concurrency;
- Sparse rank-zero/zero-extent/static/dynamic/mixed shapes, zero/full/partial
  exact counts, independent priority oracle, canonical uniqueness,
  structure/value independence, exact float/double values, retained explicit
  zero, checked offsets, rollback, exactly two declared allocations, and
  concurrency; and
- package-isolated provider-free consumers under static/shared builds.

Provider/SDK scans find no CUDA, HIP, SYCL, OpenMP, TBB, Eigen, BLAS/LAPACK,
oneMKL, cuRAND, or other provider dependency. Unsupported backend and
non-host placement fail rather than transferring or falling back.

GPU evidence is exactly **skipped**.

No GPU configure, provider compile, device runtime, or CPU/GPU parity
operation was performed or claimed.

## 6. Review findings and resolutions

| Finding | Resolution |
| --- | --- |
| Benchmark allocation evidence was initially a literal claim. | Verification added a process-wide allocation probe and executable count assertions. |
| Sparse thread test used a comma-containing macro argument. | Verification bound both coordinate arrays to locals before comparison. |
| Executable explicit-zero retention was absent. | Verification added an independently searched fixed raw-word address that produces and retains float zero. |
| Benchmark compiler identity used unconditional `__VERSION__`. | Guarded MSVC, Clang, GNU-compatible, and unknown branches were added. |
| The random guide example relied on a transitive Dense owner declaration. | Documentation now directly includes `<asc/dense.h>`; strict GCC/Clang example builds pass. |
| M5 C++ files initially failed repository formatting. | The lead formatted only bounded M5 C++ files; the strict dry run passes. |
| Capability checker retained the pre-M5 proposed threshold. | The lead advanced the runtime-tested threshold through M5. |
| Cumulative subproject still rejected the new facets. | It now requires all nine provider-free components and links `ASC::cpp`. |
| Aggregate consumer runner expected the generic executable name. | Registration now supplies `asc_cpp_aggregate_consumer`. |
| Required no-component package helper omitted one argument. | The missing expected-Core-target Boolean was added. |
| Thread tests relied on host-default pthread linkage. | Test-only `Threads::Threads` links were added without changing product/package dependencies. |
| Subdirectory-scoped `FindThreads` was invisible to deferred root inventory. | Thread discovery moved to the root testing block. |

Production self-review, independent verification, documentation/API review,
and portability/GPU/performance review report no unresolved release blocker.

## 7. Performance evidence

The benchmark is an observation, not a speed gate. It uses Dense `float`
128x128 left/right layouts for 100 fills and Sparse `float` 64x64 with count
256 for 10 generations. It records compiler/configuration, allocations,
elapsed time, next offsets, and checksums.

Final GCC 11.4 Debug/static:

```text
dense left:  allocation_calls=0 elapsed_ns=738203938
             next_offset=16401 checksum=928521972971142723
dense right: allocation_calls=0 elapsed_ns=651071775
             next_offset=16401 checksum=10828367647995593407
sparse:      allocation_calls=20 process_allocation_calls=20
             deallocation_calls=20 allocated_bytes=51200 live_allocations=0
             elapsed_ns=6912306201 next_structure_offset=8295
             next_value_offset=369 checksum=1516538221932305239
aggregate checksum=8664949311000451886
```

Final Clang 19 Release/shared:

```text
dense left:  allocation_calls=0 elapsed_ns=27913926
             next_offset=16401 checksum=928521972971142723
dense right: allocation_calls=0 elapsed_ns=28009657
             next_offset=16401 checksum=10828367647995593407
sparse:      allocation_calls=20 process_allocation_calls=20
             deallocation_calls=20 allocated_bytes=51200 live_allocations=0
             elapsed_ns=354073631 next_structure_offset=8295
             next_value_offset=369 checksum=1516538221932305239
aggregate checksum=8664949311000451886
```

Three independent Clang Release/shared runs retained all offsets, allocation
counts, and checksums. Dense elapsed ranges were
27,610,181--29,354,828 ns and 28,181,595--30,457,651 ns; Sparse ranged
298,134,753--352,218,485 ns. Sparse cost is consistent with the frozen
repeated-scan reference algorithm. No stable throughput, cross-configuration
comparison, or regression threshold is claimed.

## 8. Remaining risks

- Native MSVC, AppleClang, 32-bit, big-endian, and multi-config generator
  compilation/runtime are unavailable on this workstation. Source review and
  strict GCC/Clang/icpx evidence do not replace those hosted gates.
- No local clang-tidy executable exists; clang-tidy remains a hosted-CI gate.
- Direct non-serial facet invocation cannot be runtime-tested until an
  approved non-serial `ExecutionContext` can be constructed. Current Core
  rejects the unavailable provider at context creation.
- External raw views/resources remain caller-owned and must truthfully
  provide allocation size, alignment, lifetime, placement, and thread safety.
- Sparse exact-count selection deliberately has
  `O(exact_count * logical_size)` reference cost. The timing evidence is not a
  statistical performance study.
- GCC ThreadSanitizer requires an ASLR-disabled child on this host.
- Standalone Clang LSan cannot link the deliberate global-allocation
  interposition tests; the compatible 4/4 subset and full ASan leak-detection
  matrix are clean.
- GPU evidence is exactly **skipped**.
- The candidate is cumulative and uncommitted. Publication must audit the
  exact 334 staged, 34 tracked-unstaged, and 207 untracked-file manifest
  before creating its first commit. No remote M5 branch currently exists.

## 9. Exact proposed remote and branch-cleanup actions

None of the following commands has been run. They require separate owner
publication approval.

Because the validated candidate is cumulative over Milestones 0--4, first
freeze and inspect the exact intended commit:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
git add -A -- .
git diff --cached --check
git diff --cached --stat
git commit -m "Add asc-cpp Milestone 5 random storage generation"
```

Publish only the approved feature branch and open the pull request:

```sh
git push --set-upstream origin \
  feature/asc-cpp-m5-random-storage-generation
gh pr create \
  --base main \
  --head feature/asc-cpp-m5-random-storage-generation \
  --title "Milestone 5: Random storage generation" \
  --body-file \
  docs/development/asc-cpp-m5-random-storage-generation/publication-checkpoint-b.md
```

After all required CI and independent review pass, inspect and merge without a
force push or direct push to `main`:

```sh
gh pr checks <PR_NUMBER> --watch
gh pr view <PR_NUMBER> --json reviewDecision,statusCheckRollup
gh pr merge <PR_NUMBER> --merge --delete-branch=false
```

Only after merge, verify ancestry and worktree safety before deleting the M5
branch individually:

```sh
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor \
  feature/asc-cpp-m5-random-storage-generation main
git worktree list --porcelain
git branch -d feature/asc-cpp-m5-random-storage-generation
git push origin --delete feature/asc-cpp-m5-random-storage-generation
```

No other local or remote branch is proposed for deletion. Earlier roadmap
bookmarks, later-milestone bookmarks, recovery branches, and any branch with
unique or worktree-active commits require separate ancestry/unique-commit/
worktree audits and are outside this checkpoint.

The roadmap identifies Milestone 5 as an unreleased 0.5.0 candidate, not a
release boundary. No tag or GitHub release action is proposed.
