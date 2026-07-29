# Milestone 8 Post-Checkpoint Review Correction Integration

Status: integrated, locally validated, and independently accepted at corrected
Publication Checkpoint B; publication not authorized

Date: 2026-07-28

Milestone: **Milestone 8 — packaging/API/performance/downstream hardening**

## Repository and authority

- Repository: `git@github.com:AI4SciComp/asc-cpp.git`
- Active branch: `feature/asc-cpp-m8-hardening-downstream-r2`
- Base `HEAD`, `main`, and `origin/main`:
  `33b261ea33616a6395c4ad3b20646093103344f7`
- Candidate commit: none; the approved cumulative Milestones 0--8 candidate is
  intentionally uncommitted at this checkpoint.
- Tag at `HEAD`: none. The approved roadmap does not designate Milestone 8 as
  a release.
- Correction authority: resolve all 20 actionable risks from the independent
  read-only Milestone 8 review, within the frozen
  [`review-correction-contract.md`](review-correction-contract.md).

The worktree is intentionally dirty because it contains the preserved,
cumulative, approved Milestones 0--8 candidate. At corrected freeze it has
497 default porcelain entries, 731 entries with all untracked files expanded,
334 staged paths, 60 tracked-unstaged paths, and 397 untracked files. Unrelated
work was not reset, discarded, cleaned, or overwritten.

No commit, push, pull-request mutation, merge, tag, release, registry write,
branch deletion, or other remote/history action occurred.

## Scope and architecture result

The correction wave remains inside Milestone 8. It adds no module, target,
component, provider, public numerical operation, third-party dependency, or
later-milestone behavior.

No ADR was added, removed, superseded, or changed. The approved six-module
graph, nine provider-free targets, fifteen CUDA-enabled targets, source/header
ownership, and dependency policy remain frozen. The capability manifest and
backend matrix were updated only to record the corrected behavior and
evidence.

The M7 raw Random CUDA API remains:

```text
CudaFillPhilox4x32(ExecutionContext, MutableMemoryView, word_count,
                   stream, subsequence, offset)
```

No public function, type, target, component, or installed header was added or
renamed. The observable corrections are:

- failures after potentially enqueued CUDA work now drain the affected stream
  before returning the original provider error;
- Sparse alias rejection covers every values/coordinate/offset/index span
  actually read, without treating allocation gaps as aliases;
- integral `ReduceSum` overflow returns `ErrorCode::kOverflow` before
  publishing a wrapped result;
- `Timer` total/count/average arithmetic is checked, and result-less
  `Elapsed()` saturates rather than wrapping;
- optional CUDA package lookup remains nonfatal when only an optional CUDA
  component cannot resolve CUDAToolkit; and
- no-device CUDA tests use CTest skip code 77.

The CUDA fault seam and Timer access seam are private, non-installed test
headers. The CUDA fault seam is absent from ordinary builds and is rejected
unless CUDA and testing are both enabled.

## Risk resolutions

| # | Review risk | Resolution and required regression |
| --- | --- | --- |
| 1 | post-enqueue CUDA failure could release or reuse storage before queued work completed | Core copies, custom kernels, cuBLAS calls, event create/record failures, and Dense clone rollback drain the affected stream while preserving the original status; dedicated real-device fault tests passed |
| 2 | Sparse evaluation/SpMV alias checks omitted structural reads | Coordinate values/coordinates and Compressed values/offsets/indices are checked as exact disjoint spans; SpMV explicitly checks its whole output span against CSR structure; exact and partial overlap tests pass under GCC and Clang |
| 3 | recursive test cleanup accepted unsafe paths | every destructive driver uses a guarded workspace helper; root, source/build/workspace roots and parents, traversal, direct symlink, and symlink-descendant escapes are rejected before removal with sentinels preserved |
| 4 | CUDA no-device cases were failures or false passes | all three Core CUDA and six Dense CUDA runtime executables plus the Dense CUDA benchmark return 77 through a pre-runtime deterministic seam; CTest classifies 10/10 as skipped |
| 5 | optional CUDA discovery could invalidate required CPU components | required CUDA uses `find_dependency(CUDAToolkit 12)`; optional-only CUDA uses quiet discovery and marks only the unavailable optional closure false; build-tree, install, relocation, static/shared, repeated lookup, and missing-toolkit cases pass |
| 6 | default asc-xde-shaped test depended on a mutable sibling checkout | default trials are synthetic and repository-independent; a real repository audit is explicit opt-in with an explicit expected commit and verifies commit/status byte-for-byte before and after |
| 7 | an ELF baseline could be enforced on a merely similar host | selection now requires exact OS/architecture, compiler/version, configuration, linkage, tools, glibc, libstdc++ identity/hash, and applicable CUDA host/compiler/architecture fields; mismatch tests produce non-enforcing observations |
| 8 | `SECURITY.md` described the obsolete Milestone 1 surface | security reporting now covers the unreleased 0.9.0 Milestone 8 provider-free, CUDA, numerical, lifetime, package/tooling, concurrency, and provenance boundaries |
| 9 | CPU GEMM lacked rectangular padded coverage for transpose pairs | `float` and `double`, 2x3 by 3x4, all four transpose pairs, distinct padded mappings, expected values, and untouched holes are covered |
| 10 | integral reduction overflow behavior lacked proof | signed positive/negative and unsigned overflow plus non-overflow controls require `kOverflow` and no wrapped publication |
| 11 | Random partition claims exceeded test coverage | irregular multiway, empty, reordered, threaded, two-context, float/double CPU/GPU partitions compare with whole-domain generation |
| 12 | Timer totals/counts/averages could wrap | checked interval/total/count/divisor paths are transactional; deterministic private-boundary tests cover overflow, state preservation, saturation, and clock regression |
| 13 | Sparse Random priority tie-breaking lacked collision evidence | CPU and real-GPU tests force equal 64-bit priorities and require ordinal order 1,2,5,7 using the exact private host/device comparator used by production |
| 14 | retained historical files called Milestone 0 current | exactly 20 banners are milestone-neutral and point to the live documentation and approved Stage A architecture; a consistency test enforces them |
| 15 | three hardening fixture cleanups bypassed the guarded workspace helper | copied-build, install, and relocation cleanup paths live below the dedicated test-workspace root and call one execution-time guarded removal helper; the recursive scan covers all test CMake lists/scripts and all recursive spellings |
| 16 | CUDA ELF selection used the project C++ identity instead of the actual NVCC host identity | configuration exposes CMake's actual CUDA host compiler ID/version; a nested GNU-C++/Clang-19-CUDA-host configure proves that no GCC-host CUDA baseline is enforced |
| 17 | the Dense CUDA benchmark treated enumeration failure as device absence | only successful zero-device enumeration returns 77; provider/enumeration error exits 2 and has an exact deterministic regression |
| 18 | benchmark rows lacked operation-specific independent oracles and a reproducible environment record | every CPU and Dense CUDA timing row now verifies its own independent expected result; Sparse/Random CUDA retain their independent numerical/bit/structure guards; the exact build-input digest, machine/toolchain, timing boundaries, and noise limits are recorded below |
| 19 | the exact CUDA-host selector integration assumed CMake 3.31 variables despite a CMake 3.25 minimum, and pre-3.31 cache injection could spoof those names | CMake 3.31+ runs the exact unlike-host identity integration; CMake 3.25--3.30 unconditionally ignores raw cache values, fails closed with an empty baseline, and runs unavailable-identity plus nested spoof-cache regressions; real CMake 3.25.3 and 3.30.9 CUDA configure/compile/test trials pass |
| 20 | the pre-3.31 nested spoof-cache regression rejected valid configurations where NVCC chose its default host | the driver treats an explicit CUDA host path as optional, omits the nested cache argument when absent, and passes a real CMake 3.30.9 default-host configure/selector trial with an empty integrated baseline |

An intermediate bounding-span Sparse fix was rejected because clean Clang
testing proved it falsely classified an unrelated allocation in a structural
gap as aliased. The final exact disjoint-span design resolves that regression.

## Changed files and reasons

This list is correction-specific; the original Milestone 8 file inventory
remains in [`publication-checkpoint-b.md`](publication-checkpoint-b.md).

### Added

```text
docs/development/asc-cpp-m8-hardening-downstream/review-correction-contract.md
docs/development/asc-cpp-m8-hardening-downstream/review-correction-ownership.md
docs/development/asc-cpp-m8-hardening-downstream/review-correction-integration.md
docs/development/asc-cpp-m8-hardening-downstream/review-correction-verification.md
docs/development/asc-cpp-m8-hardening-downstream/review-correction-documentation.md
docs/development/asc-cpp-m8-hardening-downstream/review-correction-portability.md
src/core/cuda/runtime_test_internal.h
src/random/cuda/sparse_kernels_internal.h
src/utilities/timer_internal.h
tests/cmake/PrepareTestWorkspace.cmake
tests/cmake/RemoveTestWorkspace.cmake
tests/cmake/ExpectExit.cmake
tests/cmake/test_workspace_guard_probe.cmake
tests/cmake/test_workspace_guard_test.cmake
tests/hardening/cuda_host_elf_selector_integration_test.cmake
tests/hardening/cuda_host_elf_selector_unavailable_test.cmake
tests/hardening/cuda_host_elf_selector_untrusted_cache_integration_test.cmake
tests/hardening/SelectElfBaseline.cmake
tests/hardening/documentation_consistency_test.cmake
tests/hardening/select_elf_baseline_test.cmake
tests/random_sparse_cuda/priority_ordinal_collision_test.cu
```

These freeze the correction boundary and reviews, provide private fault/
arithmetic/comparator seams, guard destructive fixtures, select exact ABI
baselines, enforce documentation consistency, and exercise a real GPU
priority collision.

### Modified production and package integration

```text
CMakeLists.txt
cmake/ASCCppConfig.cmake.in
include/asc/dense/evaluate.h
include/asc/sparse/compressed.h
include/asc/sparse/coordinate.h
include/asc/sparse/linalg.h
include/asc/utilities/timer.h
src/core/CMakeLists.txt
src/core/cuda/runtime.cc
src/dense/cuda/operations.cc
src/random/cuda/sparse_kernels.cu
src/utilities/timer.cc
```

These implement stream draining, precise structural aliasing, checked Timer
arithmetic, the shared priority comparator, optional-CUDA package resolution,
and private hook isolation.

### Modified test and hardening integration

```text
benchmarks/dense/dense_benchmark.cc
benchmarks/dense_cuda/benchmark.cc
benchmarks/random_storage/random_storage_benchmark.cc
benchmarks/sparse/sparse_benchmark.cc
tests/CMakeLists.txt
tests/compile/m2_dependency_check.cmake
tests/consumer/CMakeLists.txt
tests/consumer/run_component_consumer.cmake
tests/consumer/run_core_consumer.cmake
tests/consumer/run_subproject_consumer.cmake
tests/core_cuda/CMakeLists.txt
tests/core_cuda/core_cuda_native_state_test.cc
tests/core_cuda/core_cuda_runtime_test.cc
tests/core_cuda/core_cuda_validation_test.cc
tests/core_cuda/test_support.h
tests/dense/array_evaluate_test.cc
tests/dense/linalg_test.cc
tests/dense_cuda/CMakeLists.txt
tests/dense_cuda/dense_cuda_concurrency_test.cc
tests/dense_cuda/dense_cuda_evaluate_test.cc
tests/dense_cuda/dense_cuda_linalg_test.cc
tests/dense_cuda/dense_cuda_move_assignment_test.cc
tests/dense_cuda/dense_cuda_owner_test.cc
tests/dense_cuda/dense_cuda_scalar_release_regression_test.cc
tests/dense_cuda/test_support.h
tests/downstream/CMakeLists.txt
tests/downstream/run_asc_xde_trial.cmake
tests/hardening/CMakeLists.txt
tests/hardening/header_manifest_test.cmake
tests/hardening/package_metadata_version_test.cmake
tests/package/CMakeLists.txt
tests/package/check_cuda_disabled_isolation.cmake
tests/package/component_unavailable/CMakeLists.txt
tests/package/expect_cuda_unavailable.cmake
tests/package/package_test.cmake
tests/package/repeated_components/CMakeLists.txt
tests/random_cuda/random_cuda_test.cc
tests/random_dense/dense_generation_test.cc
tests/random_dense/thread_partition_test.cc
tests/random_dense_cuda/random_dense_cuda_test.cc
tests/random_sparse/sparse_generation_test.cc
tests/random_sparse/CMakeLists.txt
tests/random_sparse_cuda/CMakeLists.txt
tests/sparse/compressed_conversion_test.cc
tests/sparse/coordinate_test.cc
tests/sparse/evaluate_test.cc
tests/sparse/linalg_test.cc
tests/utilities/timer_test.cc
```

These register and implement the fault, alias, numerical, partition,
no-device, workspace, package, downstream, baseline, and collision
regressions. The compile inventory was updated for the new private Timer
source header.

### Modified ABI, security, manifests, and documentation

```text
CHANGELOG.md
README.md
SECURITY.md
abi/linux-x86_64-clang19-cpu-shared.txt
abi/linux-x86_64-gcc11-cpu-shared.txt
abi/linux-x86_64-gcc11-cuda12-shared.txt
abi/public-headers.sha256
docs/README.md
docs/api-compatibility.md
docs/architecture.md
docs/build-system.md
docs/design/architecture_blueprint_v1.md
docs/design/architecture_review_v1.md
docs/design/array_design.md
docs/design/core_design.md
docs/design/linalg_design.md
docs/design/random_design.md
docs/design/utilities_design.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/downstream-integration.md
docs/extension-guide.md
docs/migration/array.md
docs/migration/core.md
docs/migration/handoff.md
docs/migration/inventory.md
docs/migration/linalg.md
docs/migration/random.md
docs/migration/utilities.md
docs/modules/array.md
docs/modules/dense.md
docs/modules/linalg.md
docs/modules/sparse.md
docs/modules/utilities.md
docs/optional-backends.md
docs/package-capabilities.md
docs/performance.md
docs/support-matrix.md
docs/testing.md
```

These record the corrected contracts, exact-environment ABI selector, changed
Clang STL weak-symbol observation, current security surface, live API/
package/performance behavior, neutral retained-history banners, and exact
evidence. No file was moved or removed by this correction wave.

## Targets and exact direct dependencies

No target, component, or direct edge changed:

```text
ASC::core                 -> none
ASC::utilities            -> ASC::core
ASC::expression           -> ASC::core
ASC::dense                -> ASC::core, ASC::expression
ASC::sparse               -> ASC::core, ASC::expression
ASC::random               -> ASC::core
ASC::random_dense         -> ASC::random, ASC::dense
ASC::random_sparse        -> ASC::random, ASC::sparse
ASC::cpp                  -> all provider-free targets
ASC::core_cuda            -> ASC::core; private CUDA::cudart
ASC::dense_cuda           -> ASC::dense, ASC::core_cuda; private CUDA::cublas
ASC::sparse_cuda          -> ASC::sparse, ASC::core_cuda; private CUDA::cusparse
ASC::random_cuda          -> ASC::random, ASC::core_cuda
ASC::random_dense_cuda    -> ASC::random_dense, ASC::random_cuda,
                             ASC::core_cuda
ASC::random_sparse_cuda   -> ASC::random_sparse, ASC::random_cuda,
                             ASC::core_cuda
```

ASCCMake 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7` remains the exact build-only
dependency. No installed consumer discovers ASCCMake. CUDA Runtime, cuBLAS,
and cuSPARSE remain the only provider implementation dependencies. No new
dependency was introduced.

## Exact validation commands and results

All clean builds are rooted at:

```text
/tmp/asc-cpp-m8-correction-final.s2jUoW
```

The common CPU configure was:

```sh
cmake -S . -B <build> -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=<compiler> \
  -DBUILD_SHARED_LIBS=<ON-or-OFF> \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build <build> --parallel 4
ctest --test-dir <build> --output-on-failure
```

Exact results:

| Configuration | Result |
| --- | --- |
| GCC 11.4 Debug/static | 193/193 passed, 0 failed, 129.71 s |
| GCC 11.4 Debug/shared | 195/195 passed, 0 failed, 132.96 s |
| Clang 19.0 Debug/shared | 194/194 passed, 0 failed, 150.54 s |

The first GCC-static, Clang-shared, and ASan+UBSan passes exposed the new Timer
private header missing from the source inventory; the first Clang pass also
exposed the rejected Sparse bounding-span design and the expected Clang weak
STL-symbol observation change. Those findings were resolved, their exact
baselines/inventory updated, and the quoted results are the clean final
reruns.

Sanitizer configurations added one of:

```sh
-DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
-DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON

-DASC_CPP_ENABLE_LEAK_SANITIZER=ON

-DASC_CPP_ENABLE_THREAD_SANITIZER=ON
```

Results:

| Sanitizer configuration | Selection and result |
| --- | --- |
| GCC 11.4 ASan+UBSan Debug/static | sanitizer-safe suite excluding package/consumer/downstream/performance/ABI: 139/139 passed; no sanitizer diagnostic; 2.08 s |
| Clang 19 LSan Debug/static | bounded subset: 12/12 passed; no leak diagnostic; 0.13 s |
| Clang 19 TSan Debug/static | bounded subset: 12/12 passed; no race diagnostic; 0.29 s |

Whole-tree LSan/TSan runs are `skipped`: deliberate allocation-interposition
fixtures conflict with sanitizer allocator interceptors. The bounded results
are not represented as whole-suite evidence.

The ordinary CUDA configuration was:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build \
  --parallel 4
ctest --test-dir \
  /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build \
  --output-on-failure
```

Result: 262 registered; 252 passed; ten forced-no-device tests were correctly
`skipped` through return code 77; zero failed; 1483.00 s. The two added
registrations independently prove the unlike CUDA-host ELF selection and
that a device-enumeration provider failure exits 2 rather than being skipped.

The CMake-minimum compatibility trials downloaded the official Kitware
CMake 3.25.3 and 3.30.9 Linux x86-64 archives into a temporary validation
directory, configured the same CUDA Release/shared architecture-86 candidate,
built `asc_core_cuda`, and ran:

```sh
<cmake-3.25.3-or-3.30.9> -S . -B <build> -G 'Unix Makefiles' \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
<cmake> --build <build> --target asc_core_cuda --parallel 4
<ctest> --test-dir <build> --output-on-failure \
  -R '^asc_cpp\.hardening\.(elf_baseline_selection|cuda_host_elf_selector_unavailable|cuda_host_elf_selector_untrusted_cache_integration)$'
```

Both CMake 3.25.3 and 3.30.9 configured CUDA 12.9.86 with GNU 11.4 host,
compiled `ASC::core_cuda`, and passed 3/3 selector tests (29.29 s and 29.94 s,
respectively). Their generated
`elf_abi` registrations contain an empty
`ASC_CPP_HARDENING_BASELINE:FILEPATH=`, proving fail-closed, non-enforcing
behavior. Archive SHA-256 values were
`d4d2ba83301b215857d3b6590cd4434a414fa151c5807693abe587bd6c03581e`
for 3.25.3 and
`9114e33358a9efc93d6ea658805280fc3201b882b944a4d946edd9472fd1eec7`
for 3.30.9. CMake 4.1.2 retained the exact unlike-host test, which passed
2/2 with the selector unit test in 35.53 s after this compatibility gate.
A later lead run overlapped the final reviewer running the same nested
workspace: workspace guard and selector unit passed, while the unlike-host
integration failed its nested `try_compile` (2/3 passed in 32.21 s). The
reviewer confirmed and interrupted its concurrent process. With the workspace
clear, the final serial selection (workspace guard, selector unit, and
unlike-host integration) passed 3/3 in 36.86 s. The overlap is not
represented as product or single-CTest evidence.

Independent verification then injected the plausible pre-3.31 cache values
`CMAKE_CUDA_HOST_COMPILER_ID=GNU` and
`CMAKE_CUDA_HOST_COMPILER_VERSION=11.4.0` and proved that the initial gate
still trusted those raw names. The final implementation uses separate trusted
variables that are populated only by CMake 3.31+ detection. A real CMake
3.30.9 spoofed-cache configure now passes its selector/unavailable pair 2/2,
prints that it is ignoring the untrusted values, and records an empty
`elf_abi` baseline. Each normal 3.25/3.30 run also executes a nested spoofed
cache configure, so this adversarial case is a permanent regression. The
final CMake 4.1.2 guard/unit/unlike-host selection passes 3/3 in 36.39 s and
the current suite remains 262 registrations.

The first nested spoof-cache test incorrectly required an explicit
`CMAKE_CUDA_HOST_COMPILER`, even though a supported pre-3.31 configuration may
leave that cache entry empty and let NVCC choose its default. A real CMake
3.30.9 configure that omitted the host override reproduced the test-only
failure (selector and unavailable tests passed; nested spoof test failed,
2/3). The corrected driver omits the nested host argument when it is absent.
The final default-host trial configured, built `ASC::core_cuda`, recorded
`CMAKE_CUDA_HOST_COMPILER ""`, selected an empty ELF baseline, and passed all
three selector tests in 30.53 s. Independent portability rerun of the same
selection passed 3/3 in 32.37 s.

The private fault build used the same GCC/NVCC 12.9.86 architecture-86
configuration with Debug/static, install disabled, and:

```sh
-DASC_CPP_INTERNAL_ENABLE_CUDA_RUNTIME_TEST_HOOKS=ON
```

Its Core runtime, Dense linalg, Dense owner, and Sparse Random collision
selection passed 4/4 in 1.24 s. A negative configure with the hook enabled
outside CUDA/testing failed as required. The ordinary shared libraries contain
zero hook symbols.

The exact no-device classification rerun was:

```sh
ctest --test-dir \
  /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build \
  --output-on-failure -R 'forced_no_device$'
```

Result: 10/10 correctly `skipped` via code 77, zero failed, 0.54 s. Actual
`CUDA_VISIBLE_DEVICES=-1` and empty-value enumeration are not claimed:
CUDA 12.9 `libcudart` independently aborts with a libc double-free on this
host before ASCCpp can classify the result.

Final package/tooling selections were:

```sh
ctest --test-dir \
  /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-debug-static/build \
  --output-on-failure \
  -R 'asc_cpp\\.(package|consumer|downstream|hardening\\.(workspace|package|header|public|documentation|select_elf))'

ctest --test-dir \
  /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build \
  --output-on-failure \
  -R '^asc_cpp\\.package\\.(build_tree_components|install_and_relocate_components)$'
```

Results:

- CPU package/tooling: 25/25 passed, 83.84 s.
- CUDA build-tree component aggregate: passed, 305.07 s.
- CUDA installed/relocated component aggregate: passed, 296.75 s.
- Final CUDA package aggregates: 2/2 passed, 601.82 s.
- Build-tree, copied build-tree, installed, relocated, path-with-spaces,
  static/shared, repeated lookup, required/optional/unavailable components,
  version metadata, registry isolation, and provider-free CUDAToolkit
  isolation are covered.

The explicit real-repository audit was configured with:

```sh
-DASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT=ON
-DASC_CPP_ASC_XDE_REPOSITORY=/home/yicai/AI4SciComp/asc-xde
-DASC_CPP_ASC_XDE_EXPECTED_COMMIT=abcb29b51f22f40afd7f174707b7ccf83c32d4bf
```

Result: 1/1 passed in 2.89 s; the exact commit and worktree-status bytes were
unchanged. Default synthetic downstream tests do not inspect that checkout.

The final architecture selection passed 8/8 in 0.20 s. Public header/source
inventory, target graph, package metadata, provider isolation, and exact ELF
baselines passed. The final exact environment included:

```text
Ubuntu 22.04.5 LTS; Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
11th Gen Intel Core i7-11800H @ 2.30 GHz
1 socket; 8 cores; 16 hardware threads
L1d 384 KiB; L1i 256 KiB; L2 10 MiB; L3 24 MiB
MemTotal 16,244,380 kB
ldd (Ubuntu GLIBC 2.35-0ubuntu3.13) 2.35
libstdc++.so.6.0.30
SHA-256 ff0825e113603c3866680d5d52216bc6d8eedf3a59f52a0aef67ff01994db128
GNU binutils identities recorded in the baseline
CMake 4.1.2; GNU Make 4.3
GCC 11.4.0; Release/shared; -O3 -DNDEBUG; C++20
NVCC 12.9.86; architecture 86; CUDA host GCC 11.4.0
NVIDIA GeForce RTX 3060 Laptop GPU; 6144 MiB; driver 576.83
```

Because the cumulative candidate is intentionally uncommitted, the measured
build, product, test, benchmark, and ABI inputs are identified by base commit
`33b261ea33616a6395c4ad3b20646093103344f7` plus this reproducible digest:

```sh
{
  git diff --binary main -- \
    CMakeLists.txt cmake include src tests benchmarks abi
  git ls-files --others --exclude-standard -- \
    CMakeLists.txt cmake include src tests benchmarks abi |
    LC_ALL=C sort |
    while IFS= read -r candidate_file; do
      sha256sum "${candidate_file}"
    done
} | sha256sum
```

Result:
`e7feae4784079c3edf331940b1ea8373f9c0748d4e20ad49ae0b2cbcae401f69`.
Documentation and evidence reports are deliberately excluded from this
self-independent build-input identity.

Compute Sanitizer 2025.2.1 `memcheck --leak-check full` ran the 13 applicable
Core, Dense, Sparse, raw Random, Dense Random, Sparse Random, and new
priority-collision executables. Result: 13/13 passed with zero reported
errors and zero leaks. The native-state impossible-allocation case and
`racecheck`, `initcheck`, and `synccheck` are `skipped`.

Final repository checks:

```sh
clang-format-19 --dry-run --Werror <all include/src/tests/benchmarks C++/CUDA>
git diff --check
git diff --cached --check
cmake --list-presets
```

Result: all passed. A first formatting check found one Dense CUDA benchmark
line; it was formatted and the full check then passed.

## CPU/GPU provider evidence

Hardware/toolchain:

```text
CMake 4.1.2; GNU Make 4.3
GCC 11.4; Clang 19.0
NVCC/CUDAToolkit 12.9.86; driver 576.83
NVIDIA GeForce RTX 3060 Laptop GPU; 6144 MiB; compute capability 8.6
```

| Facet | Corrected evidence |
| --- | --- |
| `core_cuda` | `configure-tested`; `compile-tested`; `runtime-tested` |
| `dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |

Core CUDA is not labeled `parity-tested`. Trusted device CSC, actual
masked-device enumeration, multi-GPU/peer access/MIG, other CUDA
toolkit/driver/host-compiler pairs, non-Linux, other compute capabilities,
hosted GPU CI, HIP/SYCL/other providers are `skipped`.

## Numerical and performance evidence

Numerical regressions pass for rectangular padded GEMM across every transpose
pair and both floating types; signed and unsigned reduction overflow; exact
Sparse structural aliasing; irregular CPU/GPU Random partitions; and forced
priority collisions. GPU parity uses independent bit, structure, or numerical
oracles as appropriate.

The threshold-free release/shared observation command was:

```sh
ctest --test-dir \
  /tmp/asc-cpp-m8-correction-final.s2jUoW/gcc-cuda-release-shared/build \
  -V \
  -R '^asc_cpp\\.(dense\\.benchmark|sparse\\.benchmark|random_storage\\.benchmark|dense_cuda\\.benchmark|sparse_cuda\\.benchmark|random_cuda\\.benchmark)$'
```

Result: 6/6 passed in 11.04 s. Selected raw observations:

- Dense CPU double 32x32 evaluate: 58,445.4 ns/iteration; GEMM:
  18,160.8 ns/iteration; zero computational allocations.
- Sparse CPU CSR 128x256, 512 nonzeros: evaluation 144,669
  ns/iteration; SpMV 6,350.32 ns/iteration; zero computational allocations.
- Random storage CPU: left/right 128x128 float Dense fills took 24,205,012 ns
  and 25,822,548 ns over 100 repetitions; exact-count coordinate generation
  took 298,307,733 ns over 10 repetitions with the declared two result
  allocations per generation.
- Dense CUDA: H2D 6.999 GB/s, D2H 7.381 GB/s, D2D 79.689 GB/s, evaluation
  6.053 GB/s, Axpy 30.607 GFLOP/s, GEMV 25.196 GFLOP/s, GEMM 2.469 TFLOP/s;
  zero ASCCpp operation allocation calls.
- Sparse CUDA CSR SpMV, 1024x1024 and 5120 nonzeros: float 119.649 million
  nonzeros/s with 704 workspace bytes; double 112.434 million nonzeros/s with
  752 workspace bytes; zero operation allocations.
- Random CUDA: raw Philox 15.377 billion logical items/s; Dense float
  `Uniform01` 11.639 billion items/s; reference Sparse `Uniform01` 26,808.3
  items/s with the documented two canonical output allocations.

CPU Dense, CPU Sparse, CPU Random storage, and every Dense CUDA row now
checks an operation-specific independent expected result after the timed
region and before reporting. Sparse and Random CUDA retain their independent
numerical, deterministic-bit, structure, allocation, and checksum guards.
CUDA timings use three warmups where printed, wait for the completion event
on every measured repetition, and exclude setup, verification copies, and
oracle computation; the Sparse Random row explicitly includes its two
canonical result allocations.

These are one-run local observations, not thresholds, speedups, regressions,
cross-machine guarantees, or optimization claims. CPU/GPU dynamic frequency,
thermal state, WSL2 scheduling, background contention, core affinity, and
timer noise were not controlled, so the values are evidence that the bounded
probes execute correctly with the stated allocation behavior, not stable
performance promises.

## Security, license, and provenance

The security boundary now covers all six modules, storage facets, aggregate,
CUDA facets, numerical/alias/lifetime/concurrency concerns, package tooling,
and evidence limitations. Checked deletion paths and post-enqueue CUDA
lifetime are directly tested.

The code remains Apache-2.0. No third-party source, data, generated payload,
dependency, license, or notice obligation was added. `THIRD_PARTY_NOTICES` is
intentionally absent after the architecture provenance audit. The correction
uses clean-room project code plus existing build-only ASCCMake and optional
CUDA provider APIs. The asc-xde repository was read-only and unchanged.

## Independent review status

- Production implementation: complete; focused GCC, Clang, and real-device
  checks accepted.
- Independent verification: complete; it found and caused removal of the
  imprecise Sparse bounding-span attempt. Its final CPU, CUDA, no-device,
  numerical, partition, collision, and formatting selections pass.
- Documentation/API/security review: complete; it identified three
  lead-owned wording gaps in Utilities Timer, Dense reduction overflow, and
  synthetic-vs-opt-in downstream behavior. All three were corrected and
  re-audited and accepted findings 15--20 with no actionable residual.
- Final portability/GPU/performance review: its first pass identified four
  additional bounded findings (15--18). All four were accepted into the
  correction contract, implemented, independently verification-reviewed, and
  freshly validated. Its compatibility re-reviews identified findings 19 and
  20, which are also contracted, implemented, and validated on real CMake
  3.25.3, 3.30.9, and 4.1.2. Its final re-review accepts all 20 findings and
  Publication Checkpoint B with no actionable residual.

## Remaining risks and deferred work

All 20 actionable review risks are resolved and independently accepted. The
remaining items are environmental evidence limits or approved future work,
not open correction defects:

- no Windows/MSVC, macOS/AppleClang, other CPU architecture/standard library,
  Ninja, hosted-CI, or other compiler/configuration/linkage matrix;
- no other CUDA toolkit/driver/compute-capability, multi-GPU, MIG, peer-access,
  or Clang CUDA-host matrix;
- actual masked-device enumeration is unavailable because the host CUDA
  runtime aborts independently; deterministic code-77 classification is
  proven instead;
- whole-suite LSan/TSan conflicts with deliberate allocator interposition;
- native-state impossible-allocation and non-memcheck Compute Sanitizer tools
  are skipped;
- trusted device CSC has no approved producer;
- ABI observations are exact-environment pre-1.0 evidence, not a cross-minor
  ABI promise; and
- Milestone 9/1.0 readiness, expanded providers/operations, optimization, and
  publication remain out of scope.

## Publication, branch retention, and proposed remote actions

The corrected R2 branch has no PR URL, merge URL, tag URL, or release URL
because publication is not authorized. Historical draft
[pull request 1](https://github.com/AI4SciComp/asc-cpp/pull/1) remains open
against `main`; it belongs to `feature/asc-cpp-m8-hardening-downstream` at
`d611aa876576ab949c2c213977f628d2844539ba` and was not mutated. Its merge
commit and merge URL are absent. Milestone 8 is not a roadmap release, so no
tag or GitHub release is proposed.

No branch was deleted. Retained branches:

```text
main
feature/asc-cpp-m0-foundation
feature/asc-cpp-m1-core
feature/asc-cpp-m2-independent-foundations
feature/asc-cpp-m3-dense-cpu
feature/asc-cpp-m4-sparse-cpu
feature/asc-cpp-m5-random-storage-generation
feature/asc-cpp-m6-gpu-core-dense
feature/asc-cpp-m7-gpu-sparse-random
feature/asc-cpp-m8-hardening-downstream
feature/asc-cpp-m8-hardening-downstream-r2
origin/main
origin/feature/asc-cpp-m8-hardening-downstream
```

After explicit publication approval, the exact proposed sequence is:

```sh
# Stage and commit only the approved cumulative candidate after reviewing the
# exact index/worktree inventory.
git push --set-upstream origin \
  feature/asc-cpp-m8-hardening-downstream-r2

# Open a pull request:
# head: feature/asc-cpp-m8-hardening-downstream-r2
# base: main
# then inspect required CI and independent review; fix on the same branch;
# merge only when both pass. Never force-push or push directly to main.
```

After merge, audit each branch separately for ancestry, unique commits, and
worktree use. The local M0--M7 branches currently equal `main` and use no
worktree, so each may then be deleted individually with `git branch -d`.
Retain the active R2 branch until merge/cleanup. Preserve the old M8 local and
remote branch until its unique `d611aa8` commit is explicitly inspected and
preserved; only then may its local and remote refs be deleted individually.
Never delete `main`, a tag, an active-worktree branch, a recovery branch, or
unpreserved work.

## Exact downstream consumption

```cmake
cmake_minimum_required(VERSION 3.25)
project(consumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)

add_executable(app main.cc)
target_link_libraries(app PRIVATE ASC::dense)
target_compile_features(app PRIVATE cxx_std_20)
```

Configure with:

```sh
cmake -S . -B build \
  -DASCCpp_DIR=<prefix>/lib/cmake/ASCCpp \
  -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON \
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
  -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF
cmake --build build
```

This provider-free consumer does not discover CUDAToolkit or ASCCMake.
