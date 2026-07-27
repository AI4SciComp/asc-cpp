# Milestone 6 Publication Checkpoint B

Status: **Reached; local publication candidate ready for owner review with
explicit skips and no open production defect**

Date: 2026-07-27

Milestone: **Milestone 6 — GPU core and dense**

Owner corrections: **No corrections**

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Remote: `origin = git@github.com:AI4SciComp/asc-cpp.git`

This checkpoint stops before every remote or history-changing action. No
commit, push, pull request, merge, tag, release, or branch deletion was
performed.

The working tree intentionally contains the cumulative, uncommitted
Milestones 0--6 restart plus the owner's earlier deletion of the retired
implementation. The M6 branch starts at the same unchanged baseline commit as
the predecessor branches. Consequently, the baseline `git diff` cannot
mechanically partition M6 from the retained M0--M5 work. The frozen M6 contract
and ownership ledger define the logical M6 boundary below. Unrelated work was
not restored, reset, committed, or discarded.

## 1. Changed files and reasons

### Frozen governance and evidence

- `docs/development/asc-cpp-m6-gpu-core-dense/milestone-contract.md` freezes
  the owner-approved milestone, exclusions, APIs, targets, package behavior,
  and publication gate.
- `docs/development/asc-cpp-m6-gpu-core-dense/ownership.md` freezes five
  disjoint write scopes: lead integration, production, independent
  verification, documentation/API review, and portability/GPU/performance
  review.
- `docs/development/asc-cpp-m6-gpu-core-dense/preflight.md`,
  `dependency-audit.md`, and `provenance-record.md` record the starting state,
  approved dependency boundary, and clean-room provenance.
- `docs/development/asc-cpp-m6-gpu-core-dense/verification-design.md`,
  `verification-review.md`, `production-self-review.md`,
  `documentation-api-review.md`, and `portability-review.md` record the
  independent contracts, findings, corrections, exact commands, results, and
  skips.
- This file records Publication Checkpoint B.

### Production implementation

- `include/asc/core/execution.h`, `include/asc/core/memory.h`, and
  `src/core/execution.cc`, `src/core/execution_internal.h` add provider-neutral
  opaque execution/event state, event query, provider dispatch, checked copy,
  and explicit lifetime behavior while preserving serial behavior.
- `include/asc/core/providers/cuda.h`,
  `include/asc/core/providers/cuda_export.h`, `src/core/cuda/cuda.cc`, and
  `src/core/cuda/provider_internal.h` add the SDK-neutral public core CUDA
  facet and its Runtime-backed devices, resources, streams, events, copy
  validation, error translation, and guarded teardown.
- `include/asc/dense/array.h` adds explicit uninitialized allocation and keeps
  ordinary initialized creation host-only.
- `include/asc/dense/providers/cuda.h`,
  `include/asc/dense/providers/cuda_export.h`, `src/dense/cuda/cuda.cc`,
  `src/dense/cuda/kernels.cu`, and `src/dense/cuda/kernels_internal.h` add the
  move-only dense CUDA context, bounded evaluator, project kernels, and the
  selected float/double cuBLAS operations.

### Build, package, architecture, and integration

- `CMakeLists.txt`, `cmake/ASCCppOptions.cmake`,
  `cmake/ASCCppComponents.cmake`, and `cmake/ASCCppConfig.cmake.in` add the
  default-off CUDA option, conditional CUDA language/toolkit discovery,
  component closures, exports, and request-conditional installed dependency
  discovery. The root now selects `CMAKE_CUDA_RUNTIME_LIBRARY=Shared` for
  every CUDA-language target so all provider facets share one Runtime state.
- `src/core/CMakeLists.txt` and `src/dense/CMakeLists.txt` define and export the
  two provider targets through the actual asc-cmake APIs. Dense also carries a
  target-local `CUDA_RUNTIME_LIBRARY=Shared` assertion surface.
- `CMakePresets.json` and `.github/workflows/ci.yml` add M6 CPU/CUDA
  configuration coverage without changing the default CPU-only build.
- `tests/CMakeLists.txt`, `tests/architecture/CMakeLists.txt`,
  `tests/architecture/check_dependency_manifest.cmake`,
  `tests/architecture/check_approved_product_targets.cmake`,
  `tests/architecture/check_public_file_policy.cmake`,
  `tests/compile/CMakeLists.txt`, and
  `tests/compile/dependency_check.cmake` register M6 tests and assert the
  module, target, language, direct-link, runtime-selection, and public-header
  contracts.
- `tests/consumer/CMakeLists.txt`,
  `tests/consumer/run_component_consumer.cmake`,
  `tests/consumer/run_core_consumer.cmake`, and
  `tests/consumer/run_subproject_consumer.cmake` add provider consumer
  orchestration.
- `tests/package/CMakeLists.txt`,
  `tests/package/check_cuda_disabled_isolation.cmake`,
  `tests/package/expect_cuda_unavailable.cmake`,
  `tests/package/package_test.cmake`,
  `tests/package/component_unavailable/CMakeLists.txt`, and
  `tests/package/component_unavailable/core_smoke.cc` extend build-tree,
  copied-tree, install, relocation, path-with-spaces, component,
  provider-free, provider-enabled, registry, and requested-unavailable package
  checks.
- `docs/development/asc-cpp-architecture/backend-capability-matrix.md`,
  `capability-manifest.yaml`, `dependency-manifest.yaml`, `ci-strategy.md`,
  `release-roadmap.md`, and `testing-strategy.md` record only the approved CUDA
  core/dense capability and evidence contract.
- `README.md`, `CHANGELOG.md`, `docs/README.md`, and `docs/api.md` publish the
  M6 option, components, surface, and unreleased 0.6.0 change.

### Independent verification and benchmark

- `tests/core_cuda/CMakeLists.txt`, `core_cuda_test.cc`, and `test_support.h`
  cover device discovery, all approved resource spaces, copies, events,
  lifetime, error recovery, inverse labels, checked address spans, and failure
  transactionality.
- `tests/dense_cuda/CMakeLists.txt`, `dense_cuda_storage_evaluate_test.cc`,
  `dense_cuda_linalg_test.cc`, `dense_cuda_concurrency_test.cc`, and
  `test_support.h` cover storage, cloning, supported and rejected expressions,
  layouts, float/double parity, alias rules, provider-width boundaries,
  concurrency, and error isolation.
- `tests/compile/m6_provider_contract.cc`,
  `m6_provider_multi_tu.{h,a.cc,b.cc,main.cc}`, and
  `m6_negative_{array_copy,dense_context_copy,event_copy,resource_copy,resource_move,unsupported_element}.cc`
  cover headers, ownership, linkage, and prohibited forms.
- `tests/consumer/core_cuda/{CMakeLists.txt,main.cc}` and
  `tests/consumer/dense_cuda/{CMakeLists.txt,main.cc}` exercise isolated
  build-tree and relocated installed consumption.
- `benchmarks/dense_cuda/dense_cuda_benchmark.cc` provides a smoke-only
  transfer-separated evaluator/algebra harness with warmups and checksums.

All `CMakeLists.txt`, package, and shared integration files were written only
by the lead. Specialist ownership of files co-located beneath a test directory
excluded its lead-owned `CMakeLists.txt`; no specialist crossed that boundary.

### Documentation/API review

- `docs/modules/core.md` documents explicit memory, context/stream/event
  ownership, copy behavior, failure semantics, and exact evidence labels.
- `docs/modules/dense.md` documents uninitialized storage, CUDA context,
  supported evaluator/algebra surfaces, alias rules, costs, and exact evidence
  labels.

No production file in utilities, expression, sparse, random, or either random
storage facet was changed for M6. No later-milestone provider was added.

## 2. APIs, targets, and direct dependency changes

### Public API

The approved additions are:

```text
CompletionEvent::Query() -> Result<bool>

CudaDeviceCount() -> Result<std::int32_t>

CudaMemoryResource::Create(Device, MemorySpace)
    -> Result<std::unique_ptr<CudaMemoryResource>>

CreateCudaExecutionContext(
    Device, Determinism = Determinism::kDeterministic)
    -> Result<ExecutionContext>

RecordCudaEvent(const ExecutionContext&)
    -> Result<CompletionEvent>

DenseArray<Element, Extents>::CreateUninitialized(
    const Extents&, MemoryResource&, LayoutLeft = {})
    -> Result<DenseArray>

DenseArray<Element, Extents>::CreateUninitialized(
    const Extents&, MemoryResource&, LayoutRight)
    -> Result<DenseArray>

DenseCudaContext::Create(ExecutionContext)
    -> Result<DenseCudaContext>

DenseCudaContext::execution_context()
    -> const ExecutionContext& noexcept

CudaEvaluate(...)
CudaCopy(...)
CudaScal(...)
CudaAxpy(...)
CudaGemv(...)
CudaGemm(...)
    -> Result<CompletionEvent>
```

The core/dense common public headers remain free of CUDA/cuBLAS SDK types.
`CudaMemoryResource` is noncopyable and nonmovable; `ExecutionContext` is a
copyable immutable shared-state owner; `CompletionEvent` and
`DenseCudaContext` are move-only.

The only new option is:

```text
ASC_CPP_ENABLE_CUDA=OFF
```

### Targets and direct links

| Build/export target | Kind | Exact direct link interface |
| --- | --- | --- |
| `asc_core_cuda` / `ASC::core_cuda` | compiled C++20 provider facet | `ASC::core`; private `CUDA::cudart` |
| `asc_dense_cuda` / `ASC::dense_cuda` | compiled C++20/CUDA provider facet | `ASC::dense;ASC::core_cuda`; private `CUDA::cublas` |

The actual asc-cmake APIs used are `asc_target_enable_cxx20`,
`asc_target_enable_warnings`, `asc_target_enable_sanitizers`, and
`asc_register_test`. The external imported targets are the standard CMake
`CUDA::cudart` and `CUDA::cublas`.

The known component set is:

```text
core utilities expression random dense sparse
random_dense random_sparse cpp core_cuda dense_cuda
```

When CUDA is disabled, only the original nine provider-free components are
available. When enabled, `core_cuda` and `dense_cuda` are also available.
The installed package discovers CUDAToolkit only when a requested closure
contains a CUDA component.

There is no provider edge from `ASC::cpp`, no cuSOLVER link, no sparse/random
CUDA target, and no new third-party dependency.

## 3. Exact commands and pass/fail/skip results

All successful builds below used
`ASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug`, whose source
repository was clean at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

### Clean CPU Release, static

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-cpu.plMFF2/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-cpu.plMFF2/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-cpu.plMFF2/build \
  --output-on-failure
```

Result: **PASS, 161/161**. Both long package tests, registry, CUDA-unavailable,
relocation, and isolation checks passed.

### Clean CPU Release, shared

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-shared-cpu.VVRn7O/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-shared-cpu.VVRn7O/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-shared-cpu.VVRn7O/build \
  --output-on-failure
```

Result: **PASS, 161/161**.

### CPU Clang 19

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-clang.QPpQFN/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-clang.QPpQFN/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-clang.QPpQFN/build \
  -R '^asc_cpp\.(architecture\.|compile\.|core\.|dense\.)' \
  --output-on-failure
```

Result: configure/build **PASS**; selected tests **PASS, 100/100**.

### Clean CUDA Release, static

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build \
  -L milestone-6 --output-on-failure
```

Result: configure/build **PASS**; M6 aggregate **PASS, 50/50** in 834.58
seconds. The build-tree component matrix passed in 349.21 seconds, the
installed/relocated component matrix passed in 283.05 seconds, and the
registry and requested-CUDA-unavailable tests passed.

### Clean CUDA Release, shared

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-shared-cuda.PdzuBI/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-shared-cuda.PdzuBI/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-shared-cuda.PdzuBI/build \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency)|consumer\.(core_cuda|dense_cuda)\.(build_tree|install_relocate))$' \
  --output-on-failure
```

Result: configure/full build **PASS**; selected runtime and isolated consumers
**PASS, 8/8**.

### Independent post-integration Runtime linkage matrix

```sh
cmake -S . -B /tmp/asc-cpp-m6-runtime-static.z5zvn6/build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-runtime-static.z5zvn6/build \
  --target asc_utilities asc_random asc_dense asc_sparse \
           asc_core_cuda asc_dense_cuda asc_dense_cuda_concurrency_test \
  --parallel 4

cmake -S . -B /tmp/asc-cpp-m6-runtime-shared.hgkhHB/build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-runtime-shared.hgkhHB/build \
  --target asc_utilities asc_random asc_dense asc_sparse \
           asc_core_cuda asc_dense_cuda asc_dense_cuda_concurrency_test \
  --parallel 4

ctest --test-dir /tmp/asc-cpp-m6-runtime-static.z5zvn6/build \
  -R '^asc_cpp\.(dense_cuda\.concurrency|consumer\.(core_cuda|dense_cuda)\.)' \
  --output-on-failure
ctest --test-dir /tmp/asc-cpp-m6-runtime-shared.hgkhHB/build \
  -R '^asc_cpp\.(dense_cuda\.concurrency|consumer\.(core_cuda|dense_cuda)\.)' \
  --output-on-failure
```

Result: both configure/builds **PASS**; static **PASS, 5/5**; shared **PASS,
5/5**.

### Formatting, diff, and preset hygiene

```sh
find include src tests benchmarks \
  -type f \( -name '*.cc' -o -name '*.h' -o -name '*.cu' \) \
  -print0 | xargs -0 clang-format-19 --dry-run --Werror
git diff --check
cmake --list-presets=all
```

Result: **PASS**. All M6 presets enumerate successfully.

### Explicit skips and expected negative outcomes

- Exact CMake 3.25 endpoint rerun: **skipped**, because no 3.25 executable is
  currently installed. `/usr/bin/cmake` is 3.22.1 and correctly rejects the
  declared `cmake_minimum_required(VERSION 3.25)` before configure.
- Ninja: **skipped**, executable unavailable.
- clang-tidy: **skipped**, executable unavailable.
- Native Windows/MSVC CUDA, macOS/AppleClang, hosted GPU CI, and multi-device:
  **skipped**, corresponding host/tool/second device unavailable.
- No remote CI was triggered because pushing is explicitly prohibited at this
  checkpoint.

The complete specialist command transcripts are retained in
`verification-review.md`, `documentation-api-review.md`, and
`portability-review.md`.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Host sanitizers

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-cpu-sanitized.aoPjAt/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-cpu-sanitized.aoPjAt/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-cpu-sanitized.aoPjAt/build \
  -E 'package|consumer' --output-on-failure
```

Result: CPU ASan+UBSan **PASS, 137/137**.

### CUDA UBSan and ASan

```sh
cmake -S . -B /tmp/asc-cpp-m6-lead-cuda-ubsan.hajYEW/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  '-DASC_CPP_SANITIZER_ARGUMENTS=UNDEFINED' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-lead-cuda-ubsan.hajYEW/build \
  --target asc_core_cuda_test asc_dense_cuda_storage_evaluate_test \
           asc_dense_cuda_linalg_test asc_dense_cuda_concurrency_test \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-lead-cuda-ubsan.hajYEW/build \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency))$' \
  --output-on-failure
```

Result: CUDA/provider-management UBSan **PASS, 4/4**.

The corresponding `ADDRESS UNDEFINED` configuration and compilation at
`/tmp/asc-cpp-m6-lead-cuda-asan.LCIcq5/build` passed. CUDA runtime execution is
**skipped**, not passed: ASan reports a double-free during `cuInit` wholly
inside the WSL NVIDIA driver libraries before ASC test work begins.

### Compute Sanitizer

```sh
for executable in \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/core_cuda/asc_core_cuda_test \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_storage_evaluate_test \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_linalg_test \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_concurrency_test
do
  compute-sanitizer --tool memcheck --report-api-errors no \
    --error-exitcode 99 "$executable"
done

compute-sanitizer --tool racecheck --report-api-errors no \
  --error-exitcode 99 \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_concurrency_test

for executable in \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_storage_evaluate_test \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_linalg_test \
  /tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_concurrency_test
do
  compute-sanitizer --tool initcheck --report-api-errors no \
    --error-exitcode 99 "$executable"
  compute-sanitizer --tool synccheck --report-api-errors no \
    --error-exitcode 99 "$executable"
done
```

Results:

- memcheck: **PASS**, zero errors for core, storage/evaluate, linalg, and
  concurrency;
- racecheck: **PASS**, zero errors, warnings, or hazards for concurrency;
- initcheck: **PASS**, zero errors for all three dense runtime executables;
- synccheck: **PASS**, zero errors for all three dense runtime executables.

The corrected shared-library concurrency executable also passed memcheck and
racecheck independently with zero findings.

### Package, relocation, and consumers

- CPU Release static: full suite **PASS, 161/161**; build-tree package,
  installed/relocated package, registry, provider-free isolation, paths with
  spaces, and requested-unavailable CUDA behavior passed.
- CPU Release shared: full suite **PASS, 161/161** with the same package
  matrix.
- CUDA static: full M6 architecture/header/dependency/runtime/parity/negative/
  consumer/package selection **PASS, 50/50**. `core_cuda` and `dense_cuda`
  build-tree and relocated installed consumers **PASS, 4/4**; the build-tree
  component matrix passed in 349.21 seconds and the installed/relocated matrix
  passed in 283.05 seconds.
- CUDA shared: `core_cuda` and `dense_cuda` build-tree and relocated installed
  consumers **PASS, 4/4**.
- Independent fresh static/shared re-audit: concurrency plus four provider
  consumers **PASS, 5/5** in each configuration.
- Provider-free consumers against CUDA-enabled packages remain provider-free.
  `ASC::cpp` does not acquire a CUDA edge.
- CPU-only package metadata contains neither provider target exports nor CUDA
  discovery. CUDA-enabled metadata adds only `core_cuda` and `dense_cuda`;
  `find_dependency(CUDAToolkit 12)` is conditional on the requested closure.

The installed/exported direct target closures were inspected as:

```text
ASC::core_cuda:
  ASC::core;$<LINK_ONLY:CUDA::cudart>

ASC::dense_cuda:
  ASC::dense;ASC::core_cuda;$<LINK_ONLY:CUDA::cublas>
```

Static and shared producer/consumer link lines contain `-lcudart`, never
`-lcudart_static`. ELF and loader inspection shows `libcudart.so.12` as the
single Runtime dependency; CUDA Runtime and fat-binary registration symbols
remain undefined imports rather than private ASC definitions.

## 5. CPU/GPU provider evidence

Environment:

```text
host:                Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
CPU:                 Intel Core i7-11800H
CMake:               4.1.2
GCC:                 11.4.0
Clang:               19.0.0
CUDA compiler:       NVCC 12.9.86
CUDA toolkit/runtime:12.9 / libcudart.so.12
cuBLAS headers:      12.9.1.4
Compute Sanitizer:   2025.2.1
driver:              576.83
GPU:                 NVIDIA GeForce RTX 3060 Laptop GPU
device count:        1
compute capability:  8.6
compiled architecture: 86
GPU memory:          6144 MiB
```

Only the required exact GPU evidence labels are used:

| Facet | Classification | Evidence |
| --- | --- | --- |
| CUDA language, toolkit, targets, architecture 86 | **configure-tested** | CMake 4.1.2 found NVCC/CUDAToolkit 12.9.86 and the required Runtime/cuBLAS targets |
| static and shared `core_cuda`/`dense_cuda`, headers, and isolated consumers | **compile-tested** | GCC 11.4, Clang 19 SDK-free headers, NVCC 12.9.86, C++20/CUDA 20 |
| core resources, copies, streams, events, errors; dense storage, evaluator, algebra, concurrency | **runtime-tested** | device 0, static and shared providers, normal runtime, UBSan, and Compute Sanitizer |
| float/double evaluator and Copy/Scal/Axpy/Gemv/Gemm | **parity-tested** | CPU serial-reference oracles versus downloaded CUDA results, approved layouts/transposes/aliases/degenerate cases |
| multi-device teardown/restoration | **skipped** | one CUDA device detected |
| CUDA runtime under ASan | **skipped** | WSL NVIDIA driver fails in `cuInit` before ASC work |
| deterministic event-record fault injection | **skipped** | no approved native-handle or provider fault-injection API |
| native Windows/macOS and hosted GPU CI | **skipped** | hosts/runners unavailable |
| sparse/random CUDA, cuSOLVER, HIP, and SYCL | **skipped** | outside approved M6 |

The serial-reference CPU provider passed GCC static/shared Release, Clang
Release, and ASan+UBSan Debug evidence. No optimized CPU provider was added or
claimed.

## 6. Review findings and resolutions

All production findings are resolved; no open release-blocking defect remains.

- M6-PORT-01/M6-VER-05: overflow-prone/truncated launch geometry was replaced
  by checked quotient/remainder block calculation, a capped grid, and
  overflow-safe grid-stride traversal. Boundary helpers test the former grid
  limit and `INT64_MAX`.
- M6-PORT-02/M6-VER-06: cuBLAS teardown now selects/restores the owning device.
  Multi-device runtime remains explicitly skipped.
- M6-PORT-03: provider declarations no longer gain `dllimport` only on a later
  redeclaration; native Windows remains a platform skip.
- M6-PORT-04/05/06 and M6-VER-07/12: sticky Runtime errors are isolated;
  completion state is pre-created; possible post-acceptance kernel/copy/event/
  cuBLAS failures drain only the affected stream before returning without an
  event. Event-record fault injection is source-reviewed and explicitly
  skipped at runtime.
- M6-PORT-07/08 and M6-VER-08: declared placement and uniqueness are validated
  before lowering; exact-self Copy is the approved no-op; exact same-index
  Axpy executes; partial Copy/Axpy overlap and every Gemv/Gemm output/input
  overlap reject transactionally.
- M6-PORT-09/M6-VER-09: `kHost` accepts only genuine unregistered pageable host
  memory; pinned/device/managed inverse labels reject and preserve storage.
- M6-PORT-10: CUDA-compiled shared symbols now follow hidden visibility;
  project kernels do not leak from the DSO.
- M6-PORT-11: cleanup stops if the owning device cannot be selected, avoiding
  an operation on the wrong device.
- M6-DOC-01/M6-VER-11: both source and destination half-open address ends use
  checked `std::uintptr_t` addition; independent normal/UBSan/memcheck
  regressions return `kOverflow` before enqueue.
- M6-DOC-03: guide formatting, explicit copy wording, and exact host storage
  wording were corrected and pass GCC/Clang/ClangFormat probes.
- M6-VER-10: the architecture oracle was corrected from the unapproved later
  `cuBLAS/cuSOLVER` wording to selected cuBLAS only.
- M6-PORT-12/M6-VER-13: shared dense originally embedded a second static CUDA
  Runtime and failed concurrency after a deliberate sticky error. Root/target
  shared-Runtime selection and a configure-time invariant resolved it.
  Independent static/shared link, ELF, loader, runtime, package, relocation,
  and consumer checks close the defect for tested Linux/GCC/NVCC builds.

The documentation/API reviewer found no unresolved public signature defect.
Strict GCC 11 and Clang 19 C++20 warnings-as-errors, `-pedantic-errors`,
`-fno-exceptions` probes parsed provider headers without CUDA include paths;
ownership traits and all four guide examples passed.

## 7. Performance evidence

The benchmark is intentionally smoke-only. It has no speed threshold,
comparison, or release performance claim. The final lead Release run used
three warmups, vector size 1,048,576 with 80 repetitions, GEMV 512 by 512 with
80 repetitions, and GEMM 256 by 256 with 40 repetitions.

```sh
/tmp/asc-cpp-m6-lead-cuda-final.qLz3Pm/build/tests/dense_cuda/asc_dense_cuda_benchmark
```

Result: **PASS**.

| Scalar | H2D (us) | Pointwise add (us) | Axpy (us) | Gemv (us) | Gemm (us) | D2H (us) | Checksum |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| float | 4518 | 20043 | 4208 | 1359 | 842 | 1009 | 349.632 |
| double | 3839 | 26951 | 7943 | 5311 | 14698 | 2179 | 349.632 |

Transfers are reported separately from operations; the final event is waited;
both checksums are finite and match the smoke oracle. These single-machine
timings are not a durable baseline.

## 8. Remaining risks

- Only one RTX 3060 Laptop GPU, driver 576.83, CUDA/cuBLAS 12.9, and
  architecture 86 were runtime/parity-tested.
- Multi-device teardown/current-device restoration is source- and
  compile-reviewed but not runtime-tested.
- Native Windows/MSVC, macOS/AppleClang, other linkers, other CUDA toolkits,
  other architectures, and hosted GPU CI have no local evidence.
- CUDA ASan is unusable on this WSL driver because it fails in driver
  initialization. Standalone UBSan and Compute Sanitizer are the usable local
  provider evidence.
- Deterministic post-enqueue `cudaEventRecord` fault injection was not added
  because doing so would require an unapproved native-handle or fault-injector
  surface.
- The SDK-neutral internal opaque-launch bridge in the public provider header
  remains an ABI maintenance surface before a stable release.
- Provider teardown can synchronize; under irrecoverable device-selection
  failure, cleanup deliberately leaks rather than acting on the wrong device.
- CUDA/cuBLAS asynchronous failures cannot always be attributed to the exact
  high-level submission; event wait is the defined completion/error boundary.
- Exact CMake 3.25, Ninja, and clang-tidy were unavailable for a fresh lead
  endpoint/tooling rerun.
- The cumulative M0--M6 work remains intentionally uncommitted and therefore
  must be partitioned and revalidated without content drift before any remote
  publication.
- Performance evidence is smoke-only and defines no regression threshold.

## 9. Exact proposed remote and branch-cleanup actions

Nothing below has been executed. It requires a separate owner publication
approval.

First, partition the cumulative local candidate into audited, ordered M0--M6
commits without changing file content. Move/reuse each already-approved
feature branch so it is based on the exact predecessor tip, then rerun its
frozen validation at the exact commit. After comparing every commit/tree to
its milestone ledger, the proposed pushes, in dependency order, are:

```sh
git push -u origin feature/asc-cpp-m0-foundation
git push -u origin feature/asc-cpp-m1-core
git push -u origin feature/asc-cpp-m2-independent-foundations
git push -u origin feature/asc-cpp-m3-dense-cpu
git push -u origin feature/asc-cpp-m4-sparse-cpu
git push -u origin feature/asc-cpp-m5-random-storage-generation
git push -u origin feature/asc-cpp-m6-gpu-core-dense
```

Open review requests in the same dependency order. Do not tag or release from
this checkpoint.

Branch cleanup is proposed only after merge, remote confirmation, an ancestry
check proving every branch has no unique work, and separate explicit owner
approval:

```sh
git branch -d feature/asc-cpp-m0-foundation
git branch -d feature/asc-cpp-m1-core
git branch -d feature/asc-cpp-m2-independent-foundations
git branch -d feature/asc-cpp-m3-dense-cpu
git branch -d feature/asc-cpp-m4-sparse-cpu
git branch -d feature/asc-cpp-m5-random-storage-generation
git branch -d feature/asc-cpp-m6-gpu-core-dense

git push origin --delete feature/asc-cpp-m0-foundation
git push origin --delete feature/asc-cpp-m1-core
git push origin --delete feature/asc-cpp-m2-independent-foundations
git push origin --delete feature/asc-cpp-m3-dense-cpu
git push origin --delete feature/asc-cpp-m4-sparse-cpu
git push origin --delete feature/asc-cpp-m5-random-storage-generation
git push origin --delete feature/asc-cpp-m6-gpu-core-dense
```

Publication Checkpoint B is the terminal state of this task.
