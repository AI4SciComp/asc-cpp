# Milestone 6 Publication Checkpoint B

Status: complete local candidate; publication not authorized

Date: 2026-07-28

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the exact cumulative, intentionally uncommitted Milestones 0--5
candidate recorded at the Milestone 5 Publication Checkpoint B

Corrections: No corrections.

## Scope result

The frozen **Milestone 6 — GPU core and dense** contract is satisfied. The
unreleased package candidate advances to 0.6.0 and adds exactly the first two
optional CUDA provider facets:

```text
ASC::core_cuda  -> ASC::core
ASC::dense_cuda -> ASC::dense;ASC::core_cuda
```

Core CUDA implements device discovery, pinned/device/managed resources,
explicit stream execution contexts, asynchronous validated copies, completion
events, query/wait, and stable ASC error translation. Dense CUDA implements
uninitialized device ownership, a bounded built-in pointwise evaluator,
project-owned Copy/Scal/Axpy kernels, and float/double cuBLAS Gemv/Gemm.

Separate production, independent verification, documentation/API, and
portability/GPU/performance agents ran with disjoint write scopes. The lead
alone changed shared root CMake, package, manifests, test registration, and
integration paths. All accepted findings are resolved and the integrated tree
was rebuilt from clean build directories.

No Sparse/Random CUDA provider, HIP provider, reduction, solver,
factorization, provider registry, native-handle adoption, hidden transfer or
fallback, unapproved dependency, or Milestone 7/8 behavior was implemented.

No commit, push, pull request, merge, tag, release, registry write, or branch
deletion occurred.

## 1. Changed files and reasons

This Milestone 6 layer is cumulative over the approved uncommitted
Milestones 0--5 predecessor. Shared paths can therefore contain changes from
more than one milestone. The exact repository state is reproducible with:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
```

The predecessor state and the owner's earlier deletions were preserved. The
unrelated modified MdeCpp `Makefile` was not touched.

At checkpoint freeze, the repository reports 459 porcelain entries, 334
staged predecessor paths, 38 tracked-unstaged paths, and 265 untracked files.
Milestone 6 added 58 untracked files, including this report; existing shared
tracked paths account for the other integration changes.

### Production API and implementation

```text
include/asc/core/execution.h
include/asc/core/memory.h
include/asc/core/providers/cuda.h
include/asc/core/providers/cuda_export.h
include/asc/dense/array.h
include/asc/dense/view.h
include/asc/dense/providers/cuda.h
include/asc/dense/providers/cuda_export.h
src/core/execution.cc
src/core/execution_internal.h
src/core/cuda/cuda_internal.h
src/core/cuda/runtime.cc
src/dense/cuda/context.cc
src/dense/cuda/context_internal.h
src/dense/cuda/kernels.cu
src/dense/cuda/kernels_internal.h
src/dense/cuda/operations.cc
src/dense/cuda/validation_internal.h
```

These paths provide the provider-neutral opaque execution/event bridge, CUDA
runtime resource/context/event implementation, extents-first uninitialized
Dense ownership, pinned-host view access, Dense provider context, evaluator,
kernels, cuBLAS calls, and provider-boundary validation.

### Root build, package, architecture, and live documentation integration

```text
.github/workflows/ci.yml
CHANGELOG.md
CMakeLists.txt
CMakePresets.json
README.md
cmake/ASCCppComponents.cmake
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
docs/README.md
docs/api.md
docs/modules/core.md
docs/modules/dense.md
docs/modules/expression.md
docs/modules/random.md
docs/modules/sparse.md
docs/modules/utilities.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
src/core/CMakeLists.txt
src/dense/CMakeLists.txt
```

These advance the candidate to 0.6.0, add the default-off CUDA option,
conditional language/toolkit discovery, exact provider targets/exports,
component closures, shared CUDA Runtime selection, installed-package
discovery, live API/lifetime guidance, and final capability/dependency
evidence. Provider-free configuration and `ASC::cpp` retain no CUDA edge.

### Architecture, compile, runtime, package, consumer, and performance evidence

```text
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/m3_dependency_check.cmake
tests/compile/m6_core_cuda_header.cc
tests/compile/m6_dense_cuda_header.cc
tests/compile/m6_negative_copy_completion_event.cc
tests/compile/m6_negative_copy_cuda_resource.cc
tests/compile/m6_negative_copy_dense_cuda_context.cc
tests/compile/m6_negative_integral_cuda_destination.cc
tests/compile/m6_negative_volatile_cuda_destination.cc
tests/compile/m6_provider_headers_no_exceptions.cc
tests/compile/m6_provider_odr.h
tests/compile/m6_provider_odr_a.cc
tests/compile/m6_provider_odr_b.cc
tests/compile/m6_provider_odr_main.cc
tests/core_cuda/CMakeLists.txt
tests/core_cuda/core_cuda_native_state_test.cc
tests/core_cuda/core_cuda_runtime_test.cc
tests/core_cuda/core_cuda_validation_test.cc
tests/core_cuda/test_support.h
tests/dense_cuda/CMakeLists.txt
tests/dense_cuda/counting_resource.h
tests/dense_cuda/dense_cuda_concurrency_test.cc
tests/dense_cuda/dense_cuda_evaluate_test.cc
tests/dense_cuda/dense_cuda_linalg_test.cc
tests/dense_cuda/dense_cuda_move_assignment_test.cc
tests/dense_cuda/dense_cuda_owner_test.cc
tests/dense_cuda/dense_cuda_scalar_release_regression_test.cc
tests/dense_cuda/device_test_helpers.h
tests/dense_cuda/test_support.h
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/cpp/CMakeLists.txt
tests/consumer/dense/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/random_dense/CMakeLists.txt
tests/consumer/random_sparse/CMakeLists.txt
tests/consumer/sparse/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/consumer/subproject/CMakeLists.txt
tests/consumer/run_component_consumer.cmake
tests/consumer/run_core_consumer.cmake
tests/consumer/run_subproject_consumer.cmake
tests/consumer/core_cuda/CMakeLists.txt
tests/consumer/core_cuda/main.cc
tests/consumer/dense_cuda/CMakeLists.txt
tests/consumer/dense_cuda/main.cc
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/package/check_cuda_disabled_isolation.cmake
tests/package/expect_cuda_unavailable.cmake
benchmarks/dense_cuda/benchmark.cc
```

These enforce the public-file and dependency boundaries; strict
self-contained/no-exception headers; five expected compile failures;
multi-TU ODR; resource/copy/event/lifetime/error tests; independent numerical
parity; layout/provider-width/alias validation; stream independence; move
assignment; allocation evidence; provider-free isolation; exact provider
consumers; build-tree/install/relocation packaging; unavailable-provider
failure; and threshold-free performance observations.

### Contract, provenance, ownership, and reviews

```text
docs/development/asc-cpp-m6-gpu-core-dense/dependency-audit.md
docs/development/asc-cpp-m6-gpu-core-dense/documentation-api-review.md
docs/development/asc-cpp-m6-gpu-core-dense/milestone-contract.md
docs/development/asc-cpp-m6-gpu-core-dense/ownership.md
docs/development/asc-cpp-m6-gpu-core-dense/portability-review.md
docs/development/asc-cpp-m6-gpu-core-dense/preflight.md
docs/development/asc-cpp-m6-gpu-core-dense/production-self-review.md
docs/development/asc-cpp-m6-gpu-core-dense/provenance-record.md
docs/development/asc-cpp-m6-gpu-core-dense/publication-checkpoint-b.md
docs/development/asc-cpp-m6-gpu-core-dense/verification-design.md
docs/development/asc-cpp-m6-gpu-core-dense/verification-review.md
```

These freeze the bounded contract, disjoint ledger, clean-room boundary,
dependency audit, independent oracles/reviews, findings, resolutions, skips,
and this checkpoint evidence.

## 2. APIs, targets, and direct dependency changes

### Core and Core CUDA

```text
CompletionEvent::Query() -> Result<bool>

CudaDeviceCount() -> Result<int32_t>
CudaMemoryResource::Create(device, memory_space)
    -> Result<unique_ptr<CudaMemoryResource>>
CreateCudaExecutionContext(device, determinism)
    -> Result<ExecutionContext>
RecordCudaEvent(context) -> Result<CompletionEvent>
```

`ExecutionContext` and `CompletionEvent` retain SDK-free public signatures.
CUDA resources accept exactly pinned-host, device, and managed spaces. Copies
are explicit, context-bound, validated before enqueue, event-returning, and
never use device-wide synchronization.

### Dense ownership and Dense CUDA

```text
DenseArray::CreateUninitialized(extents, resource, layout)
    -> Result<DenseArray>

DenseCudaContext::Create(ExecutionContext)
    -> Result<DenseCudaContext>
DenseCudaContext::execution_context() -> const ExecutionContext&

CudaEvaluate(...)
CudaCopy(...)
CudaScal(...)
CudaAxpy(...)
CudaGemv(...)
CudaGemm(...)
    -> Result<CompletionEvent>
```

The evaluator accepts exactly unqualified float/double destinations, ranks
zero through eight, and the frozen bounded expression forms. Copy/Scal/Axpy
are project-owned kernels. Gemv/Gemm use typed cuBLAS operations with checked
integer narrowing and column-major-compatible layouts. All operations are
asynchronous and require referenced storage to remain valid through event
completion.

### Targets, components, and dependencies

```text
build target    consumer target   direct ASC links          private external
asc_core_cuda   ASC::core_cuda    ASC::core                 CUDA::cudart
asc_dense_cuda  ASC::dense_cuda   ASC::dense;ASC::core_cuda CUDA::cublas
```

Known components are the nine provider-free components plus `core_cuda` and
`dense_cuda`. CUDA-disabled packages expose nine; CUDA-enabled packages expose
all eleven. `find_dependency(CUDAToolkit 12)` runs only for a requested CUDA
component closure.

Direct product dependency changes: the two optional, private system-provider
edges above. No dependency was added to a provider-free target. Configure-time
ASCCMake remains exact released 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

## 3. Exact commands and pass/fail/skip results

### Preflight

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m5-random-storage-generation \
  feature/asc-cpp-m6-gpu-core-dense
git status --porcelain=v1
git diff --cached --name-only
git diff --name-only
git ls-files --others --exclude-standard
git worktree list --porcelain
```

Result: PASS. `HEAD`, `main`, `origin/main`, M5, and M6 were exactly
`33b261ea33616a6395c4ad3b20646093103344f7`; one worktree existed. The
intentionally dirty M5 predecessor was unchanged before the branch switch:
437 porcelain entries, 334 staged paths, 34 tracked-unstaged paths, and 207
untracked files.

`main:AGENTS.md`, the architecture package, ADRs 0001--0018, manifests,
backend matrix, M5 checkpoint report, repository/provenance audits,
implementation/testing/CI/ASCCMake guidance, roadmap, and runbook were read.
No material decision remained unresolved.

### Clean GCC Release/shared CUDA

```sh
cmake -S . -B build/m6-final-cuda-release-shared \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build build/m6-final-cuda-release-shared --parallel 4
ctest --test-dir build/m6-final-cuda-release-shared \
  --output-on-failure --label-exclude package --parallel 4
```

Result: PASS. CMake 4.1.2, GCC 11.4, nvcc/CUDAToolkit 12.9.86,
architecture 86; clean configure/build passed and 153/153 non-package tests
passed. This includes four M6 compile/ODR tests, three Core CUDA runtime tests,
six Dense CUDA runtime/parity tests, and the benchmark.

### Clean Clang Release/shared CPU

```sh
cmake -S . -B build/m6-final-clang-release-shared \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build build/m6-final-clang-release-shared --parallel 4
ctest --test-dir build/m6-final-clang-release-shared \
  --output-on-failure --label-exclude 'package|consumer' --parallel 4
```

Result: PASS. Clang 19.0.0 clean configure/build passed; 139/139 tests passed.

### Minimum CMake CPU install

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . -B build/m6-final-cmake325-cpu \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build build/m6-final-cmake325-cpu --parallel 4
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --install build/m6-final-cmake325-cpu \
  --prefix build/m6-final-cmake325-cpu/install
```

Result: PASS with CMake 3.25.0 and GCC 11.4.

### Style and repository checks

```sh
find include src tests benchmarks -type f \
  \( -name '*.h' -o -name '*.cc' -o -name '*.cu' \) -print0 |
  xargs -0 clang-format-19 --dry-run --Werror
git diff --check
git diff --cached --check
```

Result: PASS.

One early Ninja generator probe failed before project configuration because
Ninja was not installed. The required Unix Makefiles fallback configured,
built, and passed. Ninja evidence is therefore skipped for this host, not
reported as a product failure or pass.

One intermediate CUDA Debug full-suite run passed 180/181. The sole failure
was the frozen M3 dependency-audit glob treating new, separately audited M6
provider paths as M3 inputs. The lead restricted that predecessor inventory to
its frozen layer; the exact test rerun passed, and both clean final matrices
above passed their architecture tests.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### CPU ASan and UBSan

```sh
cmake -S . -B build/m6-final-cpu-asan \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m6-final-cpu-asan --parallel 4
ASAN_OPTIONS='abort_on_error=1:halt_on_error=1:handle_segv=0:print_stacktrace=1' \
UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1' \
ctest --test-dir build/m6-final-cpu-asan --output-on-failure \
  --label-exclude 'package|consumer' --parallel 4
```

Result: PASS, 139/139.

Independent UBSan-only CUDA runtime/parity: PASS, 9/9. Combined CUDA
ASan+UBSan configure/compile: PASS. Runtime: SKIPPED as
environment-incompatible because WSL `libcuda.so.1.1` reproducibly
double-frees at first `cuInit`/`cudaGetDeviceCount`, before ASC behavior.

Compute Sanitizer, each with `--error-exitcode=99`:

```sh
compute-sanitizer --tool memcheck --leak-check full \
  --error-exitcode=99 \
  /tmp/asc_cpp_m6_portability_cuda_make/tests/core_cuda/asc_core_cuda_core_cuda_runtime_test
compute-sanitizer --tool memcheck --error-exitcode=99 \
  /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_linalg_test
compute-sanitizer --tool racecheck --error-exitcode=99 \
  /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_concurrency_test
compute-sanitizer --tool initcheck --error-exitcode=99 \
  /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_owner_test
compute-sanitizer --tool synccheck --error-exitcode=99 \
  /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_evaluate_test
```

Result: PASS; zero memcheck errors/leaks, racecheck hazards/errors/warnings,
initcheck errors, or synccheck errors.

### Package and isolated consumers

```sh
ctest --test-dir build/m6-final-cuda-release-shared \
  --output-on-failure \
  -R '^asc_cpp\.(consumer\.(core|cpp|core_cuda|dense_cuda)\.(build_tree|install_relocate)|package\.(build_tree_components|install_and_relocate_components|registry_unchanged|cuda_disabled_isolation|cuda_requested_unavailable))$' \
  --parallel 2
```

Result: PASS, 13/13. Provider-free `core`/`cpp` and provider
`core_cuda`/`dense_cuda` consumers configured, linked, and ran from the build
tree and a relocated install, including paths with spaces. The full component
matrix passed from build-tree and relocated packages. CUDA-disabled isolation,
requested-unavailable behavior, and package-registry non-mutation passed.

The remaining provider-free isolated consumers were then run:

```sh
ctest --test-dir build/m6-final-cuda-release-shared \
  --output-on-failure \
  -R '^asc_cpp\.consumer\.(foundations|utilities|expression|dense|sparse|random|random_dense|random_sparse)\.' \
  --parallel 2
```

Result: PASS, 15/15. Together with the disjoint 153-test non-package suite and
13-test selected package/provider suite, every one of the 181 tests registered
in the clean Release/shared CUDA build passed.

Provider-free consumer configurations set
`CMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE`, proving no accidental toolkit
discovery. Installed provider consumers remained C++-only consumer projects
while resolving the package's provider dependency.

## 5. CPU/GPU provider evidence

```text
Serial/provider-free CPU regressions:
  GCC Debug/static ASan+UBSan: runtime-tested (139/139)
  Clang Release/shared:       runtime-tested (139/139)

Core CUDA:
  configure-tested
  compile-tested
  runtime-tested

Dense CUDA:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested
```

GPU evidence comes from one NVIDIA GeForce RTX 3060 Laptop GPU, compute
capability 8.6, 6144 MiB; driver/runtime API 12090; CUDA 12.9.86; cuBLAS
headers 12.9.1. Same-device current-device preservation is runtime-tested.
Cross-device restoration is skipped because only one GPU was available.

Windows/MSVC CUDA, Apple CUDA, Clang CUDA, CUDA Sparse/Random, and HIP/ROCm are
skipped: the first three environments were unavailable and the latter
providers are later-milestone scope.

## 6. Review findings and resolutions

1. Shared provider bridge constructors were hidden; the two private opaque
   constructors received the required Core export visibility. Clean
   Release/shared consumers and runtime tests pass.
2. Pinned-host `DenseView::At` was missing; the ledger was amended to assign
   the bounded file and host/pinned access now passes while device/managed
   dereference remains rejected.
3. `CreateUninitialized` initially used resource-first ordering; production
   corrected it to the frozen extents-first API and guide/compile tests pass.
4. `CudaEvaluate` had incomplete direct includes and cv-qualified destination
   handling; direct includes and the exact unqualified float/double constraint
   were added. Strict headers and negative fixtures pass.
5. Release scalar fill exposed expired conditional `initializer_list`
   storage in overlap validation; explicit operand checks replaced it.
   Optimized regression and consumer tests pass.
6. Default Dense CUDA context move assignment could destroy a stream before
   its cuBLAS state; ordered reset/move logic and lifecycle regressions pass.
7. Exact self-copy initially bypassed native pointer validation; validation
   now precedes the no-op path and a deliberately mislabeled-pointer
   regression passes.
8. Layout identity and all cuBLAS integer-width boundaries lacked complete
   coverage; erased descriptors retain identity and tests reject true
   LayoutRight plus every checked narrowing case before allocation/dispatch.
9. The benchmark hard-coded architecture and printed an unmeasured allocation
   count; it now queries metadata and uses counting resources.
10. The predecessor M3 audit initially globbed M6 provider paths; it was
    bounded to its original layer and the architecture suite passes.

Production self-review, independent verification, documentation/API review,
and portability/GPU/performance review report no remaining material finding.

## 7. Performance evidence

The Release benchmark used three warmups, twelve measured repetitions, CUDA
events, waited completion, one post-timing checksum, and measured ASC resource
calls. On the recorded RTX 3060 Laptop GPU:

```text
h2d                6.953022152 GB/s
d2h                7.049683867 GB/s
d2d               61.26671511  GB/s
terminal evaluate  5.817760824 GB/s
axpy               26.78591659 GFLOP/s
gemv               33.57730934 GFLOP/s
gemm             2154.387743   GFLOP/s
```

Every operation produced a finite checksum and
`asc_resource_allocation_calls=0`. This is a performance smoke observation,
not a threshold, regression gate, optimized cross-system claim, or provider
implementation-allocation claim.

## 8. Remaining risks

- Real GPU runtime/parity evidence covers one Linux/WSL NVIDIA/GCC machine,
  one toolkit, one architecture, and one physical device.
- Multi-GPU/cross-device restoration is untested.
- CUDA runtime under combined ASan+UBSan is unavailable because the WSL
  driver fails during initialization; UBSan-only and Compute Sanitizer
  evidence is clean.
- Windows/MSVC, Apple, and Clang CUDA portability is untested.
- `cudaFree`/`cudaFreeHost` can synchronize during teardown; callers must
  honor the documented completion/lifetime contract.
- A device-restoration failure in a destructor cannot be propagated.
- cuBLAS may own initialization/handle allocations outside ASC resources.
- Benchmark values have no threshold and do not predict other systems.
- The cumulative M0--M6 candidate remains intentionally uncommitted and
  dirty; publication must preserve the exact reviewed state.

No remaining risk authorizes later-milestone work.

## 9. Exact proposed remote and branch-cleanup actions

No action below was executed at this checkpoint. Publication requires a
separate owner authorization.

Proposed publication, preserving the exact cumulative candidate:

```sh
git status --short
git diff --check
git diff --cached --check
git add --all
git commit -m "Implement asc-cpp milestone 6 GPU core and dense"
git push --set-upstream origin feature/asc-cpp-m6-gpu-core-dense
gh pr create \
  --base main \
  --head feature/asc-cpp-m6-gpu-core-dense \
  --title "Implement asc-cpp milestone 6 GPU core and dense" \
  --body-file docs/development/asc-cpp-m6-gpu-core-dense/publication-checkpoint-b.md
```

Before committing, the publication operator must compare the staged manifest
to this report because `git add --all` intentionally includes the cumulative
approved M0--M6 tree and the owner's earlier tracked deletions.

After required CI and independent review pass and the pull request is merged,
the exact proposed cleanup is:

```sh
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor \
  feature/asc-cpp-m6-gpu-core-dense main
git worktree list --porcelain
git branch -d feature/asc-cpp-m6-gpu-core-dense
git push origin --delete feature/asc-cpp-m6-gpu-core-dense
git fetch --prune origin
```

The ancestry and worktree checks must pass immediately before each deletion.
Do not delete `main`, any tag, the M8 handoff/recovery branch, another active
worktree branch, or any branch with unpreserved unique commits. No Milestone
6 tag is proposed: the roadmap does not designate this checkpoint as a
release, and publication has not been authorized.
