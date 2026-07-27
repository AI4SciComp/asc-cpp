# Milestone 8 Publication Checkpoint B

Status: Reached locally; ready for owner review with explicit skips and no
open product defect

Date: 2026-07-27

Approved milestone: **Milestone 8 — packaging/API/performance/downstream
hardening**

Corrections: **No corrections.**

Branch: `feature/asc-cpp-m8-hardening-downstream`

Unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Candidate package version: unreleased `0.9.0`

Remote: `origin = git@github.com:AI4SciComp/asc-cpp.git`

This checkpoint covers only the frozen Milestone 8 contract. The working tree
still contains the intentionally cumulative, unpublished Milestones 0--8
restart and the owner's pre-existing deletions over the unchanged historical
baseline. A baseline `git diff` therefore cannot mechanically attribute the
whole dirty tree to M8; the frozen contract and ownership ledger define the
logical boundary below. Unrelated work was not restored, reset, stashed,
committed, or discarded.

Four distinct agents performed production hardening, independent verification,
documentation/API review, and portability/GPU/performance review under
disjoint final write scopes. The lead alone integrated root/shared CMake,
package, test registration, architecture records, and this checkpoint.

No commit, push, pull request, merge, tag, release, branch deletion, or other
history-changing or remote mutation occurred.

## 1. Changed files and reasons

### Frozen governance, audit, and role-separated reviews

Every file in
`docs/development/asc-cpp-m8-hardening-downstream/` is M8-owned:

```text
milestone-contract.md
ownership.md
preflight.md
dependency-audit.md
provenance-record.md
production-self-review.md
verification-design.md
verification-review.md
documentation-api-review.md
portability-review.md
publication-checkpoint-b.md
```

They freeze the approved milestone/branch/version/exclusions and disjoint
ownership, record the unchanged dependency and provenance boundaries, preserve
the four independent work products and findings, and provide this checkpoint.

### Production hardening tools and baselines

```text
tools/hardening/CheckPublicSurface.cmake
tools/hardening/InspectElfAbi.cmake
tools/hardening/ProjectTargetInventoryHook.cmake
tools/hardening/TargetInventory.cmake
tools/hardening/README.md
abi/README.md
abi/header-owners.txt
abi/public-headers.sha256
abi/targets-and-components.txt
abi/linux-x86_64-gcc11-cpu-shared.txt
abi/linux-x86_64-clang19-cpu-shared.txt
abi/linux-x86_64-gcc11-cuda12-shared.txt
```

These add deterministic, read-only checks for the exact public file sets,
header ownership, component and configured-target inventory, direct link
interfaces, and bounded local ELF observations. The baselines cover 49 public
headers, 15 product targets/components, five GCC CPU shared libraries, five
Clang CPU shared libraries, and eleven CUDA-enabled shared libraries. They add
no product or consumer dependency.

### Independent verification fixtures and performance probes

```text
benchmarks/hardening/public_header_compile_probe.cc
benchmarks/hardening/run_compile_object_probe.cmake
tests/hardening/check_installed_headers.cmake
tests/hardening/installed_api/CMakeLists.txt
tests/hardening/installed_api/installed_api_contract.cc
tests/hardening/package_metadata/CMakeLists.txt
tests/hardening/package_metadata/core_smoke.cc
tests/downstream/asc_xde_trial/CMakeLists.txt
tests/downstream/asc_xde_trial/asc_xde_trial.cc
tests/downstream/run_asc_xde_trial.cmake
tests/package/repeated_components/CMakeLists.txt
tests/package/repeated_components/main.cc
```

These independently test package version and component metadata, exact
installed headers, the installed public API, repeated cumulative component
lookups, compile/object observations, and an asc-xde-shaped ODE/diffusion
consumer. The real asc-xde repository remains read-only and clean.

### Lead-owned package and validation integration

```text
CMakeLists.txt
CMakePresets.json
cmake/ASCCppComponents.cmake
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
tests/CMakeLists.txt
tests/hardening/CMakeLists.txt
tests/downstream/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_public_file_policy.cmake
```

These set the unreleased package candidate to `0.9.0`, derive component
closures/export names from the frozen component metadata, preserve
request-conditional CUDA discovery, register every M8 hardening/downstream
check, and update current architecture/package assertions without rewriting
historical milestone evidence.

The current isolated consumers were updated from the prior candidate request
to `ASCCpp 0.9`:

```text
tests/consumer/core/CMakeLists.txt
tests/consumer/core_cuda/CMakeLists.txt
tests/consumer/cpp/CMakeLists.txt
tests/consumer/dense/CMakeLists.txt
tests/consumer/dense_cuda/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/random_cuda/CMakeLists.txt
tests/consumer/random_dense/CMakeLists.txt
tests/consumer/random_dense_cuda/CMakeLists.txt
tests/consumer/random_sparse/CMakeLists.txt
tests/consumer/random_sparse_cuda/CMakeLists.txt
tests/consumer/sparse/CMakeLists.txt
tests/consumer/sparse_cuda/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
```

No product header or production `.cc`/`.cu` file changed for M8.

### Documentation and approved architecture status

```text
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/api-compatibility.md
docs/downstream-integration.md
docs/extension-guide.md
docs/package-capabilities.md
docs/performance.md
docs/support-matrix.md
docs/modules/core.md
docs/modules/utilities.md
docs/modules/expression.md
docs/modules/dense.md
docs/modules/sparse.md
docs/modules/random.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/ci-strategy.md
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/development/asc-cpp-architecture/release-roadmap.md
docs/development/asc-cpp-architecture/testing-strategy.md
```

These consistently describe the unreleased `0.9.0` package, exact component
and compatibility semantics, supported extension/lifetime boundaries,
downstream usage, local performance protocol, platform/provider support and
skips, and the reached local checkpoint. The CUDA package guide explicitly
distinguishes exact ASCCpp link interfaces from unrelated `CUDA::` targets
that CMake's `FindCUDAToolkit` may define.

## 2. APIs, targets, and direct dependency changes

### Public C++ API

No public C++ declaration, header inventory entry, numerical operation,
provider, scalar family, ownership rule, or supported extension point was
added or changed. The mechanically checked inventory remains exactly 49
headers.

### Product targets and components

No target or component was added, removed, or renamed. The set remains:

```text
ASC::core
ASC::utilities
ASC::expression
ASC::dense
ASC::sparse
ASC::random
ASC::random_dense
ASC::random_sparse
ASC::cpp
ASC::core_cuda
ASC::dense_cuda
ASC::sparse_cuda
ASC::random_cuda
ASC::random_dense_cuda
ASC::random_sparse_cuda
```

Package metadata now has hardened, tested semantics for
`ASCCpp_VERSION`, `ASCCpp_KNOWN_COMPONENTS`,
`ASCCpp_AVAILABLE_COMPONENTS`, and `ASCCpp_<component>_FOUND`. Repeated
`find_package` calls accumulate only the exact requested ASC closure. This is
package hardening, not a new product target or C++ API.

### Direct dependency graph

No direct dependency changed:

| Target | Direct ASC dependencies | Direct external implementation dependency |
| --- | --- | --- |
| `ASC::core` | none | none |
| `ASC::utilities` | `ASC::core` | none |
| `ASC::expression` | `ASC::core` | none |
| `ASC::dense` | `ASC::core`, `ASC::expression` | none |
| `ASC::sparse` | `ASC::core`, `ASC::expression` | none |
| `ASC::random` | `ASC::core` | none |
| `ASC::random_dense` | `ASC::random`, `ASC::dense` | none |
| `ASC::random_sparse` | `ASC::random`, `ASC::sparse` | none |
| `ASC::cpp` | all eight provider-free components | none |
| `ASC::core_cuda` | `ASC::core` | private `CUDA::cudart` |
| `ASC::dense_cuda` | `ASC::dense`, `ASC::core_cuda` | private `CUDA::cublas` |
| `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` | private `CUDA::cusparse` |
| `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | none added |
| `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | none added |
| `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | none added |

The only required build dependency remains released ASCCMake 0.1.0 at exact
commit `8a7dcbad3a97267cce59810aff24de800a3497a7`. No fetched content,
unapproved library, test framework, runtime, provider SDK, or later-milestone
dependency was added.

## 3. Exact commands and pass/fail/skip results

### Toolchain and formatting

```sh
cmake --version
/usr/bin/cmake --version
g++-11 --version
clang++-19 --version
nvcc --version
nvidia-smi --query-gpu=name,driver_version,compute_cap --format=csv,noheader
clang-format-19 --dry-run --Werror \
  $(rg --files benchmarks/hardening tests/hardening tests/downstream \
      tests/package/repeated_components | rg '\.(cc|h)$')
git diff --check
```

Results: CMake 4.1.2, system CMake 3.22.1, GCC 11.4.0, Clang 19.0.0,
nvcc 12.9.86, RTX 3060 Laptop GPU/driver 576.83/compute capability 8.6;
format and diff whitespace passed. `clang-tidy-19` and Ninja were unavailable
and are `skipped`.

The minimum-version negative was:

```sh
m8_min_build=$(mktemp -d /tmp/asc-cpp-m8-min-cmake.XXXXXX)
/usr/bin/cmake -S . -B "$m8_min_build" \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
```

Result: expected failure pass; CMake 3.22.1 reported that 3.25 or higher is
required. An exact CMake 3.25 executable was unavailable locally; positive
coverage used 4.1.2.

### CPU compiler/build/linkage matrix

Each literal build directory was configured with:

```sh
cmake -S . -B <build> \
  -DCMAKE_CXX_COMPILER=<compiler> \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> \
  -DBUILD_SHARED_LIBS=<ON-or-OFF> \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build <build> --parallel 8
```

Full commands then used `ctest --test-dir <build> --output-on-failure`.
Complementary cross-product commands used:

```sh
ctest --test-dir <build> --output-on-failure \
  -LE 'package|consumer|downstream' -j 8
```

| Compiler/configuration/linkage | Build directory | Result |
| --- | --- | --- |
| GCC 11 Release static | `/tmp/asc-cpp-m8-integration` | full 187/187 pass, then repeated lookup 2/2 pass |
| GCC 11 Debug shared | `/tmp/asc-cpp-m8-gcc-debug-shared-final` | full 188/188 pass |
| Clang 19 Release static | `/tmp/asc-cpp-m8-clang-release-static-final` | full 187/187 pass |
| Clang 19 Debug shared | `/tmp/asc-cpp-m8-clang-debug-shared-final` | full 188/188 pass |
| GCC 11 Debug static | `/tmp/asc-cpp-m8-gcc-debug-static-final` | focused 139/139 pass |
| GCC 11 Release shared | `/tmp/asc-cpp-m8-gcc-release-shared-final` | focused 139/139 pass |
| Clang 19 Debug static | `/tmp/asc-cpp-m8-clang-debug-static-final` | focused 139/139 pass |
| Clang 19 Release shared | `/tmp/asc-cpp-m8-clang-release-shared-final` | focused 139/139 pass |

The post-correction repeated-package command was:

```sh
ctest --test-dir /tmp/asc-cpp-m8-integration --output-on-failure \
  -R '^asc_cpp\.package\.repeated_component_lookup'
```

Result: 2/2 pass. Independent verifier CPU rerun: 2/2 pass in 2.78 seconds.
Independent portability CUDA rerun: 2/2 pass in 21.37 seconds.

### Focused hardening/downstream command

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cpu-gcc-make \
  --output-on-failure -R 'asc_cpp\.(hardening|downstream)'
```

Result: 26/26 pass in 129.78 seconds. It covered source/installed surface,
install/copy/relocation/path-with-spaces, exact headers, installed API,
version/metadata cases, compiler/object probes, and four asc-xde-shaped
downstream modes.

The final lead-owned post-correction checkpoint slice was:

```sh
ctest --test-dir /tmp/asc-cpp-m8-integration \
  --output-on-failure -L milestone-8 -j 1
```

Result: 32/32 pass in 365.72 seconds. This includes the complete CPU package,
hardening, installed API, repeated lookup, relocation, and downstream M8
registration after the final oracle correction.

### Expected diagnostic and bounded skips

- The first diagnostic Ninja configure failed because Ninja is not installed;
  the Unix Makefiles rerun passed. Ninja is `skipped`.
- The first CUDA repeated-lookup test failed 0/2 because its oracle rejected
  an unrelated toolkit target that `FindCUDAToolkit` defines. No product link
  edge was present. The corrected exact-interface oracle passed CPU 2/2 and
  CUDA 2/2; `PORT-M8-005` and `VER-M8-004` preserve the diagnostic and
  resolution.
- The first default Compute Sanitizer diagnostic reported the intentional
  negative out-of-memory API result. The memory/leak run with API-return
  diagnostics disabled passed 7/7 with zero memory errors and leaks.
- ThreadSanitizer configured and compiled; one focused runtime passed, while
  dense-random could not start on WSL2 (`unexpected memory mapping`, exit 66).
  TSan runtime is `skipped`, not a product failure.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Host sanitizers

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m8-portability-asan-ubsan \
  --output-on-failure -LE 'package|consumer|performance'
```

Result: ASan+UBSan 134/134 pass; leak detection enabled. ASCCMake sanitizer
capability probes, configure, and build all passed.

GCC TSan selection used
`ASC_CPP_ENABLE_THREAD_SANITIZER=ON` in
`/tmp/asc-cpp-m8-portability-tsan`; configure/build passed, sparse-random
runtime passed, and the dense-random WSL2 runtime is `skipped` as described
above. Standalone LSan was not separately selected; leak coverage came from
ASan.

### Package and relocation

- Full GCC/Clang static/shared runs passed all registered build-tree,
  copied-build-tree, install, relocated-prefix, and paths-with-spaces cases.
- Exact `0.9.0`, compatible `0.9`, and expected rejection of newer `0.9.1`,
  older-minor `0.8`, and major `1.0` requests passed.
- Installed manifests passed for exactly 37 provider-free headers and 49
  CUDA-enabled headers.
- Installed package metadata remained relocatable and exposed no required
  source-tree, build-tree, or asc-cmake path.
- Provider-free lookup did not discover CUDAToolkit or create CUDA targets.
  A CUDA request with `/nonexistent/nvcc` failed configuration as expected.

### Isolated consumers and downstream

- Provider-free isolated component consumers: 19/19 pass.
- Installed public aggregate API: configure/build/run pass.
- Repeated `core` then `dense`/`dense_cuda` then `utilities` lookup:
  CPU 2/2 and CUDA 2/2 pass with exact ASC target closures and exact
  linkage-dependent interfaces.
- asc-xde-shaped build-tree, copied-build-tree, installed, and relocated
  trials: 4/4 pass.
- Real asc-xde remained clean at
  `abcb29b51f22f40afd7f174707b7ccf83c32d4bf` before and after.

## 5. CPU/GPU provider evidence

### CPU

Serial-reference core, expression, dense, sparse, random, random storage, and
utilities behavior passed under both GCC and Clang, Debug and Release, static
and shared. Public headers compiled in standalone and exceptions-disabled
modes; negative contracts, multi-TU checks, numerical oracles, allocation
checks, packages, relocation, and isolated consumers passed. This is local
Linux x86-64/libstdc++ evidence.

### GPU classifications

| Surface | Classification | Result |
| --- | --- | --- |
| CUDA-disabled/provider-free isolation | `configure-tested` | pass |
| CUDA request with nonexistent compiler | `configure-tested` | expected failure pass |
| CUDA 12.9.86, toolkit targets, architecture 86 | `configure-tested` | pass |
| all six CUDA facets and provider tests | `compile-tested` | pass |
| `core_cuda` allocation/copy/context/event | `runtime-tested` | pass |
| `core_cuda` host/device round trips | `parity-tested` | pass |
| `dense_cuda` storage/evaluation/algebra/concurrency | `runtime-tested` | 4/4 pass |
| `dense_cuda` hand-computed float/double oracles | `parity-tested` | pass |
| `sparse_cuda` clone/SpMV/evaluation | `runtime-tested` | pass |
| `sparse_cuda` structural/numerical oracles | `parity-tested` | pass |
| `random_cuda` raw Philox generation | `runtime-tested` | pass |
| `random_cuda` independent Philox word oracle | `parity-tested` | pass |
| `random_dense_cuda` float/double fills | `runtime-tested` | pass |
| `random_dense_cuda` CPU bit/padding oracle | `parity-tested` | pass |
| `random_sparse_cuda` float/double generation | `runtime-tested` | pass |
| `random_sparse_cuda` coordinate/value/CPU-bit oracles | `parity-tested` | pass |
| seven provider binaries under Compute Sanitizer | `runtime-tested` | 7/7 pass; zero leaks/errors |
| trusted-device CSC evaluator success path | `skipped` | no approved trusted device CSC producer |
| multi-device, peer access, MIG, other GPU architectures | `skipped` | unavailable hardware/topology |
| Clang CUDA-host pairing, other toolkits/drivers, hosted GPU CI | `skipped` | not run |

The exact static CUDA configure/build and focused real-device commands were:

```sh
cmake -S . -B /tmp/asc-cpp-m8-portability-cuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda-12.9/bin/nvcc \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DBUILD_SHARED_LIBS=OFF
cmake --build /tmp/asc-cpp-m8-portability-cuda --parallel 6
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  --output-on-failure \
  -R 'asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency|benchmark)|sparse_cuda\.(runtime|benchmark)|random_cuda\.(runtime|benchmark)|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)'
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  --output-on-failure -R '^asc_cpp\.hardening\.'
```

Results: configure/build pass for all 15 targets; real-device suite 11/11
pass; CUDA package/API hardening 22/22 pass.

The exact shared CUDA ELF command is retained in `portability-review.md`; it
configured `BUILD_SHARED_LIBS=ON`, built the eleven CPU/CUDA libraries, and
ran:

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda-shared \
  --output-on-failure -R '^asc_cpp\.hardening\.elf_abi$'
```

Result: 1/1 pass against the exact GNU 11/CUDA 12 Release digest.

The exact device-memory command was:

```sh
for m8_binary in \
  tests/core_cuda/asc_core_cuda_test \
  tests/dense_cuda/asc_dense_cuda_storage_evaluate_test \
  tests/dense_cuda/asc_dense_cuda_linalg_test \
  tests/sparse_cuda/asc_sparse_cuda_test \
  tests/random_cuda/asc_random_cuda_test \
  tests/random_dense_cuda/asc_random_dense_cuda_test \
  tests/random_sparse_cuda/asc_random_sparse_cuda_test
do
  compute-sanitizer --tool memcheck --error-exitcode=99 \
    --leak-check=full --report-api-errors=no "$m8_binary"
done
```

The focused real-device runtime/benchmark suite passed 11/11. The CUDA
hardening slice passed 22/22 in 208.67 seconds, and the GNU 11/nvcc 12
Release/shared exact eleven-library ELF gate passed 1/1.

## 6. Review findings and resolutions

| Finding | Resolution |
| --- | --- |
| Current package diagnostics/examples still referred to M7/0.7 | Version-neutral unavailable message and current `0.9`/`0.9.0` assertions/docs; resolved and independently reviewed |
| Production surface/target/ELF tools initially lacked root registration | Registered configure/source/install/shared checks; independent runs passed |
| ELF tool's explicit non-ELF skip could appear as a CTest pass | Added `SKIP_REGULAR_EXPRESSION`; resolved |
| Default Compute Sanitizer reported intentional negative API returns | Retained diagnostic; final memcheck disables API-return reporting only and passes zero leaks/errors |
| Repeated CUDA lookup confused toolkit target existence with a link edge | Exact ASC closure and exact static/shared interfaces now checked; CPU and CUDA 2/2 pass |
| Compile timing was noisy | Gate remains correctness and within-command object-size repeatability only; no unsupported timing threshold |
| CUDA installed API/header case was initially pending | Independent CUDA hardening 22/22, exact 49 headers, installed all-facet API configure/build/run; resolved |
| Documentation needed the target-existence/link-interface distinction | Package guide corrected by its documentation owner and independently accepted |

Production, verification, documentation/API, and portability reviewers all
accept the integrated candidate for Publication Checkpoint B. No open product
C++ defect, package defect, unapproved dependency, or later-milestone feature
remains.

## 7. Performance evidence

The same aggregate public-header probe compiled three times per compiler:

```sh
cmake \
  -DCXX_COMPILER=/usr/bin/g++ \
  -DCOMPILER_ID=GNU \
  -DINCLUDE_DIR=/home/yicai/AI4SciComp/asc-cpp/include \
  -DSOURCE_FILE=/home/yicai/AI4SciComp/asc-cpp/benchmarks/hardening/public_header_compile_probe.cc \
  -DOUTPUT_DIR=/tmp/asc-cpp-m8-portability-compile-gcc \
  -DREPETITIONS=3 \
  -P benchmarks/hardening/run_compile_object_probe.cmake
cmake \
  -DCXX_COMPILER=/usr/bin/clang++-19 \
  -DCOMPILER_ID=Clang \
  -DINCLUDE_DIR=/home/yicai/AI4SciComp/asc-cpp/include \
  -DSOURCE_FILE=/home/yicai/AI4SciComp/asc-cpp/benchmarks/hardening/public_header_compile_probe.cc \
  -DOUTPUT_DIR=/tmp/asc-cpp-m8-portability-compile-clang \
  -DREPETITIONS=3 \
  -P benchmarks/hardening/run_compile_object_probe.cmake
```

| Compiler | Elapsed seconds | Object bytes |
| --- | --- | --- |
| GCC 11.4 | 2.84594, 2.75995, 2.68286 | 1864 each |
| Clang 19 | 3.87677, 4.02490, 3.77456 | 1144 each |

Correctness and repeatable object size passed. These are noisy local
observations, not compiler comparisons or absolute gates.

A final GCC Release/static CPU rerun from
`/tmp/asc-cpp-m8-integration` reported:

```sh
./tests/dense/asc_dense_allocation_free_benchmark
./tests/sparse/asc_sparse_allocation_free_benchmark
./tests/random_dense/asc_random_storage_benchmark
```

| Probe | Correctness/allocation result | Time |
| --- | --- | --- |
| dense 32x32, 100 evaluate-add/GEMM iterations | zero operation allocations; checksum 4.75 | 26573 us |
| sparse CSR 128x128, 382 nnz, 200 SpMV iterations | zero operation allocations; checksum 382 | 301 us |
| random dense 64x64, 500 iterations | zero operation allocations; checksum 2046.81 | 24341 us |
| random sparse 32x32/count 64, 20 iterations | 40 matching allocation/deallocation calls; checksum 39724 | 28653 us |

The real-device CUDA probes passed their untimed correctness/parity oracle
after warmup and synchronization. Observed timed work included dense
pointwise/AXPY/GEMV/GEMM, sparse CSR SpMV, raw Philox, dense random fill, and
sparse random generation. Exact workloads/times/checksums are retained in
`portability-review.md`. The exact command was:

```sh
ctest --test-dir /tmp/asc-cpp-m8-portability-cuda \
  -V -R 'asc_cpp\.(dense_cuda|sparse_cuda|random_cuda)\.benchmark'
```

Result: 3/3 pass. No speedup, cross-machine threshold, or optimization claim
is made.

## 8. Remaining risks

- The local evidence is Linux/WSL2 x86-64, libstdc++, GCC 11, Clang 19,
  CMake 4.1.2/Unix Makefiles, CUDA 12.9, one driver, and one compute-8.6 GPU.
  Windows/MSVC, macOS/AppleClang, libc++, non-x86-64, Ninja, and exact
  CMake 3.25 positive execution are `skipped`.
- ELF baselines are compiler/configuration-specific pre-1.0 observations.
  Intentional internal template-support symbols are visible, compiler weak/
  unique exports differ, and shared libraries have unversioned SONAMEs. No
  cross-toolchain, cross-build, cross-platform, or cross-minor ABI promise is
  made.
- Trusted device CSC evaluation, multi-device restoration/peer access/MIG,
  other GPU architectures/toolkits, and hosted GPU CI are `skipped`.
- Arbitrary external CUDA allocation terminal bounds remain caller-supplied.
  Trusted sparse provenance is not a readiness event; storage, resources,
  workspaces, and provider contexts must outlive completion.
- TSan runtime is bounded by the WSL2 mapping failure. ASan+UBSan and Compute
  Sanitizer pass, but other sanitizer/platform combinations remain unrun.
- Performance observations are single-host smoke measurements with
  uncontrolled thermal state, scheduling, first-use, and frequency scaling.
- The complete cumulative M0--M8 implementation remains uncommitted over the
  unchanged baseline. Remote review cannot begin until the owner separately
  authorizes a reviewed commit plan for that cumulative tree.

## 9. Exact proposed remote and branch-cleanup actions

### Current remote/ref facts

Read-only inspection used:

```sh
git remote -v
git ls-remote --heads origin 'refs/heads/feature/asc-cpp-m*'
git ls-remote --heads origin main master
```

`origin/main` is
`33b261ea33616a6395c4ad3b20646093103344f7`. No remote
`feature/asc-cpp-m*` branch currently exists. All local M0--M8 feature branches
and `main` still point at that same commit; the cumulative implementation is
only in the dirty working tree.

### Proposed publication actions — do not execute without separate approval

First obtain explicit authority for a reviewed commit plan that intentionally
captures the cumulative M0--M8 restart while excluding unrelated files. After
those commits exist on the current branch, the exact proposed remote actions
are:

```sh
git push --set-upstream origin feature/asc-cpp-m8-hardening-downstream
gh pr create --draft \
  --base main \
  --head feature/asc-cpp-m8-hardening-downstream \
  --title "asc-cpp: complete milestones 0-8 through hardening checkpoint" \
  --body "Local Publication Checkpoint B is recorded in docs/development/asc-cpp-m8-hardening-downstream/publication-checkpoint-b.md. No release is requested."
```

Push and draft-PR creation are proposed only. They were not executed.

### Proposed cleanup — only after authorized merge and reachability proof

After the reviewed commits are merged, publication evidence is retained, and
`origin/main` is verified to contain the work:

```sh
git switch main
git pull --ff-only origin main
git branch --delete \
  feature/asc-cpp-m0-foundation \
  feature/asc-cpp-m1-core \
  feature/asc-cpp-m2-independent-foundations \
  feature/asc-cpp-m3-dense-cpu \
  feature/asc-cpp-m4-sparse-cpu \
  feature/asc-cpp-m5-random-storage-generation \
  feature/asc-cpp-m6-gpu-core-dense \
  feature/asc-cpp-m7-gpu-sparse-random \
  feature/asc-cpp-m8-hardening-downstream
git push origin --delete feature/asc-cpp-m8-hardening-downstream
```

The remote-delete command applies only if the proposed M8 push occurred and
the branch was merged. There are currently no remote predecessor feature
branches to delete. Local and remote branch deletion remain separately
unauthorized and were not performed.
