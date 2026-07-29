# Milestone 7 Publication Checkpoint B

Status: complete local candidate; publication not authorized

Date: 2026-07-28

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the exact cumulative, intentionally uncommitted Milestones 0--6
candidate recorded at the Milestone 6 Publication Checkpoint B

Corrections: No corrections.

Owner-approved contract amendment: raw Random CUDA generation accepts the
existing Core `MutableMemoryView` plus `word_count`, not a bare pointer plus
count. The declared byte capacity is validated without adding an allocation
registry or CUDA Driver API dependency.

## Scope result

The frozen **Milestone 7 — sparse CUDA and random CUDA facets** contract is
satisfied. The unreleased package candidate advances to 0.7.0 and adds exactly:

```text
ASC::sparse_cuda
ASC::random_cuda
ASC::random_dense_cuda
ASC::random_sparse_cuda
```

Sparse CUDA provides canonical host-CSR staging, explicit-workspace
deterministic cuSPARSE CSR SpMV, a zero-workspace positive-nonunit-stride
project kernel, and bounded same-structure coordinate/CSR evaluation. Random
CUDA provides exact Philox4x32-10 raw words, Dense Uniform01 generation, and
exact-count canonical Sparse Uniform01 generation with explicit word domains
and completion.

Separate production, independent verification, documentation/API, and
portability/GPU/performance agents ran with disjoint write scopes. The lead
alone changed shared root CMake, package, manifests, test registration, and
integration paths. Independent verification and portability review accept the
candidate with no open product blocker.

No cuRAND, Thrust/CUB dependency, cuSOLVER, hidden transfer, packing,
conversion, densification, computational workspace, fallback, later provider,
Milestone 8 implementation, or release work was added.

No commit, push, pull request, merge, tag, release, registry write, branch
deletion, or other remote/history operation occurred.

## 1. Changed files and reasons

This Milestone 7 layer is cumulative over the approved uncommitted
Milestones 0--6 predecessor. Shared paths can therefore contain changes from
more than one milestone. The exact repository state is reproducible with:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
```

At checkpoint freeze, asc-cpp reports 484 porcelain entries, 334 staged
predecessor paths, 38 tracked-unstaged paths, and 334 untracked files. The M7
preflight recorded 459, 334, 38, and 265 respectively; M7 added 69 untracked
paths, including this report, while preserving all prior staged and tracked
work. The unrelated modified MdeCpp `Makefile` was not touched.

### Production API and implementation

```text
include/asc/core/execution.h
include/asc/sparse/compressed.h
include/asc/sparse/coordinate.h
include/asc/sparse/providers/cuda.h
include/asc/sparse/providers/cuda_export.h
include/asc/random/providers/cuda.h
include/asc/random/providers/cuda_export.h
include/asc/random/providers/dense_cuda.h
include/asc/random/providers/dense_cuda_export.h
include/asc/random/providers/sparse_cuda.h
include/asc/random/providers/sparse_cuda_export.h
src/sparse/cuda/context.cc
src/sparse/cuda/context_internal.h
src/sparse/cuda/kernels.cu
src/sparse/cuda/kernels_internal.h
src/sparse/cuda/operations.cc
src/random/cuda/raw.cc
src/random/cuda/raw_kernels.cu
src/random/cuda/raw_kernels_internal.h
src/random/cuda/dense.cc
src/random/cuda/dense_kernels.cu
src/random/cuda/dense_kernels_internal.h
src/random/cuda/sparse.cc
src/random/cuda/sparse_kernels.cu
src/random/cuda/sparse_kernels_internal.h
```

These add the four SDK-free public provider facets, original CUDA kernels,
cuSPARSE adapter/context, checked validation and failure draining, move-only
owners/results, canonical device-view provenance, checked values rebinding,
and the one shared-library visibility correction for the existing private
completed-event constructor.

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
docs/modules/random.md
docs/modules/sparse.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
src/random/CMakeLists.txt
src/sparse/CMakeLists.txt
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/consumer/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
```

These advance the candidate to 0.7.0, conditionally define/export/install the
four provider facets, compute exact requested component closures, discover
CUDAToolkit only for provider requests, register all M7 evidence, and update
the live API, dependency, capability, and backend records. Provider-free
configuration and `ASC::cpp` remain CUDA-free.

### Runtime, compile, package-consumer, and performance evidence

```text
tests/sparse_cuda/CMakeLists.txt
tests/sparse_cuda/sparse_cuda_test.cc
tests/random_cuda/CMakeLists.txt
tests/random_cuda/philox_oracle.h
tests/random_cuda/random_cuda_test.cc
tests/random_cuda/test_support.h
tests/random_dense_cuda/CMakeLists.txt
tests/random_dense_cuda/random_dense_cuda_test.cc
tests/random_sparse_cuda/CMakeLists.txt
tests/random_sparse_cuda/random_sparse_cuda_test.cc
tests/compile/m7_cuda_contracts.cc
tests/compile/m7_cuda_negative_copy.cc
tests/compile/m7_negative_dense_const_destination.cc
tests/compile/m7_negative_dense_integral.cc
tests/compile/m7_negative_random_result_copy.cc
tests/compile/m7_negative_sparse_integral.cc
tests/compile/m7_negative_sparse_random_result_copy.cc
tests/compile/m7_negative_sparse_volatile.cc
tests/compile/m7_provider_odr.h
tests/compile/m7_provider_odr_a.cc
tests/compile/m7_provider_odr_b.cc
tests/compile/m7_provider_odr_main.cc
tests/compile/m7_random_cuda_header.cc
tests/compile/m7_random_dense_cuda_header.cc
tests/compile/m7_random_sparse_cuda_header.cc
tests/compile/m7_sparse_cuda_header.cc
tests/consumer/sparse_cuda/CMakeLists.txt
tests/consumer/sparse_cuda/main.cc
tests/consumer/random_cuda/CMakeLists.txt
tests/consumer/random_cuda/main.cc
tests/consumer/random_dense_cuda/CMakeLists.txt
tests/consumer/random_dense_cuda/main.cc
tests/consumer/random_sparse_cuda/CMakeLists.txt
tests/consumer/random_sparse_cuda/main.cc
benchmarks/sparse_cuda/sparse_cuda_benchmark.cc
benchmarks/random_cuda/random_cuda_benchmark.cc
```

These enforce independent Philox/Uniform01/sparse-structure/numerical oracles;
clone, SpMV, evaluator, ownership, allocation, rollback, overlap, concurrency,
lifetime, and rank-nine behavior; self-contained/no-exception headers; seven
expected compile failures; multi-TU ODR; isolated build-tree and relocated
consumers; and threshold-free performance observations.

### Contract, provenance, ownership, and reviews

```text
docs/development/asc-cpp-m7-gpu-sparse-random/dependency-audit.md
docs/development/asc-cpp-m7-gpu-sparse-random/documentation-api-review.md
docs/development/asc-cpp-m7-gpu-sparse-random/milestone-contract.md
docs/development/asc-cpp-m7-gpu-sparse-random/ownership.md
docs/development/asc-cpp-m7-gpu-sparse-random/portability-review.md
docs/development/asc-cpp-m7-gpu-sparse-random/preflight.md
docs/development/asc-cpp-m7-gpu-sparse-random/production-self-review.md
docs/development/asc-cpp-m7-gpu-sparse-random/provenance-record.md
docs/development/asc-cpp-m7-gpu-sparse-random/publication-checkpoint-b.md
docs/development/asc-cpp-m7-gpu-sparse-random/verification-design.md
docs/development/asc-cpp-m7-gpu-sparse-random/verification-review.md
```

These freeze the bounded contract, owner amendment, disjoint ledger,
clean-room boundary, exact dependency audit, independent oracle design and
reviews, findings/resolutions, explicit skips, and this checkpoint.

## 2. APIs, targets, and direct dependency changes

### Sparse CUDA and provider-enabling Sparse evolution

```text
SparseCudaContext::Create(ExecutionContext)
SparseCudaContext::execution_context()

CudaStridedVectorView<Element>::Create(data, extent, stride)

CudaCloneCsr(context, canonical_host_csr, device_resource)
  -> Result<CudaCsrClone<Element>>

CudaCsrSpmvWorkspaceSize(context, device_csr, input, output)
  -> Result<size_t>

CudaCsrSpmv(context, alpha, device_csr, input, beta, output, workspace)
  -> Result<CompletionEvent>

CudaEvaluate(context, expression, coordinate_or_compressed_destination)
  -> Result<CompletionEvent>
```

`CudaCsrArray` is move-only and exposes a trusted canonical device CSR view.
Existing `CoordinateView` and `CompressedSparseView` add
`canonical_structure_trusted()` and checked `RebindValues`; provider-created
views preserve canonical provenance without making raw device structure
trusted.

### Random CUDA facets

```text
CudaFillPhilox4x32(context, MutableMemoryView, word_count,
                   stream, subsequence, offset)
  -> Result<CudaRandomWordGeneration>

CudaFillDenseUniform01(context, DenseView<Element, Rank>,
                       stream, subsequence, offset)
  -> Result<CudaDenseUniform01Generation<Element, Rank>>

CudaGenerateSparseUniform01(context, extents, exact_count, resource,
                            structure_stream/subsequence/offset,
                            value_stream/subsequence/offset)
  -> Result<CudaSparseUniform01Generation<Element, ExtentsType>>
```

All generation results own a move-only completion event and exact next
offsets. Sparse generation additionally owns the canonical device coordinate
and value buffers.

### Targets, components, and dependencies

| Build target | Consumer target | Direct ASC links | Private/direct system edge |
| --- | --- | --- | --- |
| `asc_sparse_cuda` | `ASC::sparse_cuda` | `ASC::sparse`; `ASC::core_cuda` | `CUDA::cusparse` |
| `asc_random_cuda` | `ASC::random_cuda` | `ASC::random`; `ASC::core_cuda` | none; CUDA Runtime is supplied by the established Core CUDA closure |
| `asc_random_dense_cuda` | `ASC::random_dense_cuda` | `ASC::random_dense`; `ASC::random_cuda`; `ASC::core_cuda` | none |
| `asc_random_sparse_cuda` | `ASC::random_sparse_cuda` | `ASC::random_sparse`; `ASC::random_cuda`; `ASC::core_cuda` | none |

The package exposes fifteen total targets when CUDA is enabled and nine when
disabled. The only new external product edge is private
`CUDA::cusparse`; CUDA Runtime was approved in M6. No new third-party
dependency or provider-free edge was added. ASCCMake remains exact released
0.1.0 at `8a7dcbad3a97267cce59810aff24de800a3497a7`.

## 3. Exact commands and pass/fail/skip results

### Preflight

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m6-gpu-core-dense \
  feature/asc-cpp-m7-gpu-sparse-random
git status --porcelain=v1
git diff --cached --name-only
git diff --name-only
git ls-files --others --exclude-standard
git worktree list --porcelain
```

Result: PASS. `HEAD`, `main`, `origin/main`, M6, and M7 were exactly
`33b261ea33616a6395c4ad3b20646093103344f7`; one worktree existed. The
intentionally dirty M6 predecessor was unchanged at 459 porcelain entries,
334 staged paths, 38 tracked-unstaged paths, and 265 untracked files.

Repository `main:AGENTS.md`, the architecture package, ADRs 0001--0018,
manifests, backend matrix, roadmap, M6 checkpoint, repository/provenance
audits, implementation/testing/CI/ASCCMake guidance, and the runbook were
read. No material decision remained unresolved after the owner-approved raw
destination amendment.

### Clean GCC Debug/static CPU

```sh
cmake -S . -B build/m7-final-gcc-debug-static-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-gcc-debug-static-make --parallel 4
ctest --test-dir build/m7-final-gcc-debug-static-make \
  --output-on-failure --parallel 4
```

Result: PASS, 163/163. This includes 19 provider-free isolated consumers,
static build-tree/install/relocation packaging, registry isolation,
CUDA-disabled isolation, and requested-CUDA-unavailable failure.

### Clean Clang Release/shared CPU

```sh
cmake -S . -B build/m7-final-clang-release-shared-cpu-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-clang-release-shared-cpu-make --parallel 4
ctest --test-dir build/m7-final-clang-release-shared-cpu-make \
  --output-on-failure --parallel 4
```

Result: PASS, 163/163. CPU-only shared package, relocation, isolated
consumers, CUDA-disabled isolation, and required-provider failure all pass.

### Clean GCC Release/shared CUDA

```sh
cmake -S . -B build/m7-final-gcc-cuda-release-shared-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_CUDA_HOST_COMPILER=g++ \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-gcc-cuda-release-shared-make \
  --clean-first --parallel 4
ctest --test-dir build/m7-final-gcc-cuda-release-shared-make \
  --output-on-failure --parallel 4
```

Result: PASS, 217/217 in 1137.31 seconds. This includes 22 M7
header/no-exception/positive/negative/ODR tests, four M7 runtime/parity tests,
two M7 benchmark smokes, 31 consumers, 36 package-labeled tests, the full
build-tree component aggregate, relocated install aggregate, registry
isolation, CUDA-disabled isolation, and requested-CUDA-unavailable failure.

After manifest/checkpoint status updates:

```sh
ctest --test-dir build/m7-final-gcc-cuda-release-shared-make \
  --output-on-failure -L architecture -j1
```

Result: PASS, 7/7.

### Independent Release/static and Release/shared verification

The portability reviewer configured the clean static build:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m7-portability-final.MI4gyG/build \
  -G 'Unix Makefiles' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_BUILD_TYPE=Release
```

Result: PASS. Static provider targets, export closures, four M7 runtimes, and
the two benchmarks passed. The lead then completed the whole static tree and
ran:

```sh
cmake --build /tmp/asc-cpp-m7-portability-final.MI4gyG/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-portability-final.MI4gyG/build \
  --output-on-failure \
  -R 'asc_cpp\.consumer\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.(build_tree|install_relocate)' \
  -j1
```

Result: PASS, 8/8 in 206.70 seconds. Static build-tree and relocated installed
consumption pass for every M7 component.

The independent verifier configured a distinct Release/shared tree:

```sh
cmake -S . -B /tmp/asc-cpp-m7-verifier-release \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m7-verifier-release --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-verifier-release \
  --output-on-failure \
  -R 'asc_cpp\.(compile\.m7_|sparse_cuda\.(runtime|benchmark)|random_cuda\.(runtime|benchmark)|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)' \
  -j1
```

Result: PASS after M7-VER-005 resolution; focused M7 suite 28/28 in
13.34 seconds. Its eight M7 build-tree/relocated isolated consumers also pass.

### Style, dependency, and symbol checks

```sh
find include/asc/sparse/providers include/asc/random/providers \
     src/sparse/cuda src/random/cuda \
     tests/sparse_cuda tests/random_cuda \
     tests/random_dense_cuda tests/random_sparse_cuda \
     tests/compile \
     tests/consumer/sparse_cuda tests/consumer/random_cuda \
     tests/consumer/random_dense_cuda tests/consumer/random_sparse_cuda \
     benchmarks/sparse_cuda benchmarks/random_cuda \
     -type f \( -name '*.h' -o -name '*.cc' -o -name '*.cu' \) -print0 |
  xargs -0 clang-format-19 --dry-run --Werror
git diff --check
git diff --cached --check
nm -D --defined-only \
  build/m7-final-gcc-cuda-release-shared-make/src/core/libasc_core.so |
  c++filt | rg 'CompletionEvent::CompletionEvent\(bool\)'
```

Result: PASS. The completed-event constructor is a defined exported Core
symbol. `ldd` shows cuSPARSE only on `libasc_sparse_cuda.so`; the three Random
provider DSOs use the established Core CUDA/Runtime closure and no cuSPARSE,
cuRAND, cuBLAS, or cuSOLVER library.

### Recorded failures and skips

- `cmake -S . -B build/m7-final-gcc-debug-static -G Ninja ...` failed before
  project configuration because `CMAKE_MAKE_PROGRAM` was not found. Ninja is
  `skipped` on this host; the Unix Makefiles matrices above are terminal.
- Independent shared compilation initially failed M7-VER-005 because the
  existing private `CompletionEvent(bool)` constructor lacked export
  visibility. The lead added `ASC_CORE_EXPORT`; clean shared rebuilds, symbol
  inspection, 217/217, and independent 28/28 pass.
- The first lead shared build then failed M7-VER-006 because a verifier test
  relied on invalid `vector`-to-`span` template deduction. The verifier used
  explicit `span<const float>` and strengthened all five truthful workspace
  overlap operands. The clean-first rebuild, final sparse runtime, and full
  suite pass.
- The first static eight-consumer command passed all four build-tree tests but
  failed all four install-relocate tests because the reviewer had deliberately
  built only focused targets and whole-project install could not find the
  unbuilt `libasc_utilities.a`. After the full static build, the identical
  eight-test command passed 8/8. No product change was required.
- Preliminary runs in a shared lead build directory were discarded after two
  review agents inadvertently invoked CTest there. All terminal evidence above
  comes from clean lead directories or distinct reviewer `/tmp` directories.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### CPU ASan and UBSan

```sh
cmake -S . -B build/m7-final-clang-asan-ubsan-static-make \
  -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build build/m7-final-clang-asan-ubsan-static-make --parallel 4
ASAN_OPTIONS=detect_leaks=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ctest --test-dir build/m7-final-clang-asan-ubsan-static-make \
  --output-on-failure --parallel 4 \
  --label-exclude 'package|consumer'
```

Result: PASS, 139/139. Package/consumer subprocess tests are deliberately
excluded because they do not reliably inherit the instrumented runtime.

### CUDA Compute Sanitizer

```sh
for exe in \
  tests/sparse_cuda/asc_sparse_cuda_test \
  tests/random_cuda/asc_random_cuda_test \
  tests/random_dense_cuda/asc_random_dense_cuda_test \
  tests/random_sparse_cuda/asc_random_sparse_cuda_test
do
  compute-sanitizer --tool memcheck --leak-check full --error-exitcode=99 \
    "/tmp/asc-cpp-m7-portability-final.MI4gyG/build/$exe"
done
```

Result: PASS, 4/4. Every executable reports `ERROR SUMMARY: 0 errors` and
`LEAK SUMMARY: 0 bytes leaked in 0 allocations`. The strengthened final sparse
fixture was rerun under memcheck with the same zero-error result.

CUDA racecheck is `skipped`: NVIDIA documents a possible false race for
cuSPARSE SpMV with `beta == 0`, so it is not used as an M7 correctness oracle.
CUDA initcheck/synccheck, host TSan, multi-GPU sanitizer execution, and CUDA
under host ASan are also `skipped` because they were not run in the available
terminal matrix. They are not reported as passes.

### Package/relocation summary

- CPU-only GCC static: full 163/163, including all build-tree/install-relocate
  consumers and package isolation.
- CPU-only Clang shared: full 163/163 with the same package modes.
- CUDA-enabled GCC shared: full 217/217; aggregate build-tree package passed
  in 385.79 seconds and aggregate relocated install passed in 373.16 seconds.
- CUDA-enabled GCC static: all four M7 components passed build-tree and
  installed/relocated path-with-spaces consumption, 8/8.
- Independent Release/shared verifier: all four M7 components passed the same
  two isolated consumer modes, 8/8.
- Registry non-mutation, CUDA-disabled package isolation, requested-CUDA
  unavailable failure, unknown/unavailable component behavior, exact component
  closure, and no-component `ASC::cpp` behavior pass.

## 5. CPU/GPU provider evidence

```text
Serial/provider-free CPU:
  GCC Debug/static:            runtime-tested, 163/163
  Clang Release/shared:        runtime-tested, 163/163
  Clang ASan+UBSan static:     runtime-tested, 139/139 selected

sparse_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested

random_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested

random_dense_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested

random_sparse_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested
```

GPU evidence comes from one NVIDIA GeForce RTX 3060 Laptop GPU, compute
capability 8.6, 6144 MiB; driver 576.83; CUDA driver/runtime API 12090;
nvcc/CUDAToolkit 12.9.86; cuSPARSE headers 12510; GCC 11.4 host; architecture
86.

Trusted device CSC success is `skipped` because M7 has no approved trusted
device CSC producer. Multi-GPU/cross-device, cross-toolkit, other compute
capabilities, Windows/MSVC CUDA, Clang CUDA, Apple, non-Linux, HIP/ROCm, and
SYCL are `skipped`.

## 6. Review findings and resolutions

1. A raw pointer-plus-count API could not prove destination capacity. The
   owner approved `MutableMemoryView + word_count`; declared-capacity rejection
   passes without a new dependency.
2. External Dense/Sparse views cannot prove their physical allocation end.
   Tests and docs now state the truthful-storage precondition and do not
   fabricate unsafe undersized allocations.
3. `CudaCsrArray::view()` omitted its CSR format template argument. It now
   creates the explicit trusted CSR view; float/double staging passes.
4. The verifier initially counted zero-byte sparse outputs as physical live
   allocations. Attempt and physical-allocation accounting were separated.
5. Compressed sparse evaluation initially implemented only terminal copy. The
   frozen shallow copy/negate/add/subtract/multiply grammar now works for
   coordinate and compressed destinations.
6. Nonzero scalar add/subtract could densify implicit zeros. Only exact zero is
   admitted; matching-type multiplication remains structure-preserving.
7. Post-submission failures could release local storage before stream work
   completed. Clone, random sparse, cuSPARSE, kernels, copies, and event-record
   failure paths now drain only the affected stream.
8. Sparse generation/evaluation had unapproved rank caps. Descriptor metadata
   is dynamic and rank-nine generation/evaluation parity passes.
9. Workspace and sparse structure/value overlap checks were incomplete.
   Full workspace-versus-five-operands checks and pairwise structure/value plus
   partial-old-values rebinding checks now pass.
10. A volatile vector element could reach a qualification-discarding cast. The
    public constraint now rejects volatile types at compile time.
11. Public-header self-containment, clone constraints, and formatting had
    focused defects. Strict Clang/GCC, no-exception, ODR, negative, and format
    gates pass.
12. Shared M7 DSOs could not resolve the existing private completed-event
    constructor. Its Core export visibility was corrected and independently
    verified.
13. Sparse-random documentation omitted the quadratic canonical-insertion
    term. Guides/reviews now state
    `O(logical_size * exact_count + exact_count^2 + rank * exact_count)`.
14. A verifier workspace fixture used invalid template deduction and initially
    did not guarantee each overlap span was at least the required workspace
    size. Explicit spans and enlarged truthful allocations now exercise all
    five actual overlap rejections.
15. The first static relocation consumer invocation started from the
    reviewer's intentionally focused target build, so whole-project install
    could not find an unbuilt provider-free archive. Completing the static
    build resolved the validation precondition; the unchanged tests pass 8/8.

Production self-review, independent verification, documentation/API review,
and portability/GPU/performance review report no unresolved product finding.

## 7. Performance evidence

The Release benchmarks use three warmups, wait for every completion event,
exclude setup/transfers/oracles from the timed region, validate an independent
result afterward, report checksums, and expose ASC resource/workspace behavior.
One observation on the recorded RTX 3060 Laptop environment:

| Operation | Timed work | Observation | Workspace/allocation evidence | Checksum |
| --- | ---: | ---: | --- | ---: |
| CSR SpMV float | 20 x 1024x1024, 5120 NNZ | 1,227,648 ns; 8.34115e7 NNZ/s | 704 B workspace; 0 operation allocations | 14940377177479771011 |
| CSR SpMV double | 20 x 1024x1024, 5120 NNZ | 1,383,917 ns; 7.39929e7 NNZ/s | 752 B workspace; 0 operation allocations | 9178157494086742915 |
| raw Philox words | 12 x 1,048,576 words | 1,074,053 ns; 1.17154e10 words/s | caller storage; 0 operation allocations | 13841617604916660332 |
| Dense Uniform01 float | 12 x 1,048,576 values | 1,639,973 ns; 7.67263e9 values/s | caller storage; 0 operation allocations | 15165452046652654026 |
| Sparse Uniform01 float | 12 x 16,384 candidates, 128 selected | 7,336,389,504 ns; 26,799 candidates/s | 30 calls / 38,400 B: exactly two output allocations for 3 warmups + 12 timed runs | 7322770344431580519 |

These are correctness/performance smoke observations, not statistically stable
comparisons, thresholds, regression gates, or CPU/GPU speedup claims. The
sparse result demonstrates the disclosed low-workspace algorithm cost.

## 8. Remaining risks

- Runtime/parity covers one Linux/WSL NVIDIA/GCC host, one toolkit, one
  architecture, and one physical GPU.
- CUDA Runtime pointer attributes cannot establish the allocation end of
  arbitrary external Dense/Sparse storage; truthful valid storage remains a
  caller precondition.
- cuSPARSE workspace and reproducibility boundaries can change with toolkit,
  driver, hardware, alignment, or algorithm implementation.
- Trusted device CSC success and multi-GPU/cross-device restoration remain
  untested.
- Pageable host CSR input may be internally staged by CUDA Runtime.
- Context/resource/owner/view/workspace lifetimes remain caller obligations
  through completion; event destruction does not synchronize.
- Context destruction cannot report device-selection/restoration failure.
- Sparse exact-count generation is deliberately very slow for large domains
  or counts because hidden computational workspace is forbidden.
- The benchmark observations have no performance threshold.
- The cumulative M0--M7 candidate remains intentionally uncommitted and dirty;
  publication must preserve the exact reviewed state.

No remaining risk authorizes Milestone 8 or release work.

## 9. Exact proposed remote and branch-cleanup actions

No action below was executed. Publication requires a separate owner
authorization.

Proposed publication, preserving the exact cumulative candidate:

```sh
git status --short
git diff --check
git diff --cached --check
git add --all
git commit -m "Implement asc-cpp milestone 7 sparse and random CUDA facets"
git push --set-upstream origin feature/asc-cpp-m7-gpu-sparse-random
gh pr create \
  --base main \
  --head feature/asc-cpp-m7-gpu-sparse-random \
  --title "Implement asc-cpp milestone 7 sparse and random CUDA facets" \
  --body-file docs/development/asc-cpp-m7-gpu-sparse-random/publication-checkpoint-b.md
```

Before committing, the publication operator must compare the staged manifest
to this report because `git add --all` intentionally includes the cumulative
approved M0--M7 tree and the owner's earlier tracked deletions.

After required CI and independent review pass and the pull request is merged,
the exact proposed cleanup for this branch is:

```sh
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor \
  feature/asc-cpp-m7-gpu-sparse-random main
git worktree list --porcelain
git branch -d feature/asc-cpp-m7-gpu-sparse-random
git push origin --delete feature/asc-cpp-m7-gpu-sparse-random
git fetch --prune origin
```

The ancestry and worktree checks must pass immediately before each local or
remote deletion. Do not delete `main`, any tag, an active worktree branch,
recovery/handoff branch, or a branch with unpreserved unique commits.

No Milestone 7 tag or GitHub release is proposed. The approved roadmap says
Milestone 7 completion and release publication are separate decisions and
explicitly records this checkpoint as not a release.
