# Milestone 7 Publication Checkpoint B

Status: Reached locally; awaiting publication authority

Date: 2026-07-27

Approved milestone: Milestone 7 — sparse CUDA and random CUDA facets

Corrections: No corrections.

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

Unchanged baseline `HEAD`: `33b261ea33616a6395c4ad3b20646093103344f7`

Remote: `git@github.com:AI4SciComp/asc-cpp.git`

The milestone contract and ownership ledger are frozen. Four separate roles
performed production implementation, independent verification,
documentation/API review, and portability/GPU/performance review with disjoint
write scopes. The lead alone integrated shared root, package, architecture,
and CMake files.

No commit, push, merge, tag, release, branch deletion, or other remote action
was performed. The repository still contains the intentionally cumulative,
unpublished Milestones 0--7 restart over the unchanged baseline, plus
pre-existing deletions. The M7-attributable files below do not claim that the
full dirty-tree status is M7-only.

## 1. Changed files and reasons

### Frozen governance and reviews

The complete milestone record is in
`docs/development/asc-cpp-m7-gpu-sparse-random/`:

- `milestone-contract.md`, `ownership.md`, and `preflight.md` freeze approval,
  scope, direct dependencies, evidence vocabulary, branch, and disjoint
  ownership.
- `dependency-audit.md` and `provenance-record.md` record the approved
  dependency and clean-room boundaries.
- `production-self-review.md`, `verification-design.md`,
  `verification-review.md`, `documentation-api-review.md`, and
  `portability-review.md` contain the four role-specific records.
- `publication-checkpoint-b.md` is this final integration and validation
  record.

### Production API and implementation

- `include/asc/sparse/coordinate.h` and
  `include/asc/sparse/compressed.h`: added internal canonical provenance,
  provider-neutral owner factories, trusted read access, and checked
  `RebindValues`.
- `include/asc/sparse/providers/cuda.h` and `cuda_export.h`: added the sparse
  CUDA context, CSR clone, provider-neutral vector view, workspace query,
  SpMV, and bounded evaluator API/export surface.
- `include/asc/random/providers/cuda.h` and `cuda_export.h`: added raw
  Philox4x32 device generation.
- `include/asc/random/providers/dense_cuda.h` and
  `dense_cuda_export.h`: added dense float/double Uniform01 device generation.
- `include/asc/random/providers/sparse_cuda.h` and
  `sparse_cuda_export.h`: added exact-count canonical sparse Uniform01 device
  generation.
- `src/sparse/cuda/cuda.cc`, `kernels.cu`, and `kernels_internal.h`: implemented
  guarded cuSPARSE CSR SpMV, nonunit-stride project SpMV, explicit workspace,
  CSR clone, canonical validation, and structure-preserving evaluation.
- `src/random/cuda/cuda.cc`, `kernels.cu`, and `kernels_internal.h`: implemented
  the original Philox4x32-10 CUDA kernel and launch validation.
- `src/random/cuda/dense_cuda.cc`, `dense_kernels.cu`, and
  `dense_kernels_internal.h`: implemented logical-order dense Uniform01 fill.
- `src/random/cuda/sparse_cuda.cc`, `sparse_kernels.cu`, and
  `sparse_kernels_internal.h`: implemented low-workspace, exact-count sparse
  generation.
- `src/core/execution_internal.h` and `src/core/cuda/cuda.cc`: exposed the
  internal already-complete event path, made failure cleanup narrow, and added
  exact bound tracking for ASC-owned CUDA allocations.
- `src/dense/cuda/cuda.cc`: preserved the established exact-self no-op contract
  after the shared CUDA allocation-bound integration.

### Targets, package, architecture, and validation registration

- `CMakeLists.txt`, `cmake/ASCCppOptions.cmake`,
  `cmake/ASCCppComponents.cmake`, and `cmake/ASCCppConfig.cmake.in`: advanced
  the package to `0.7.0`, registered four conditional facets and exact
  component closures, and limited installed CUDAToolkit discovery to requested
  CUDA closures.
- `src/sparse/CMakeLists.txt` and `src/random/CMakeLists.txt`: defined,
  exported, installed, and audited the four product targets using actual
  ASCCMake target APIs and standard CMake CUDA targets.
- `tests/CMakeLists.txt`, `tests/compile/CMakeLists.txt`,
  `tests/consumer/CMakeLists.txt`, `tests/package/CMakeLists.txt`,
  `tests/package/package_test.cmake`, and the four M7 test-directory
  `CMakeLists.txt` files: registered runtime, compile, negative, benchmark,
  package, relocation, and isolated-consumer gates.
- `tests/architecture/check_approved_product_targets.cmake`,
  `check_dependency_manifest.cmake`, and `check_public_file_policy.cmake`:
  extended exact target/dependency/public-file policy checks.
- `tests/compile/dependency_check.cmake`: added exact direct-edge and forbidden
  dependency audits.
- `.github/workflows/ci.yml`: extended the declared CPU/CUDA validation matrix
  to the approved M7 boundary; no hosted CI was triggered.

### Independent verification and performance probes

- `tests/sparse_cuda/sparse_cuda_test.cc`: independent sparse clone, SpMV,
  evaluator, validation, alias, scalar-effect, provenance, and CPU-reference
  checks.
- `tests/random_cuda/random_cuda_test.cc`, `philox_oracle.h`, and
  `test_support.h`: independent Philox oracle, raw generation, offsets,
  placement, overflow, concurrency, and transfer helpers.
- `tests/random_dense_cuda/random_dense_cuda_test.cc`: layout, rank, stride,
  alias, offset, and bit-parity checks.
- `tests/random_sparse_cuda/random_sparse_cuda_test.cc`: exact-count,
  canonicality, uniqueness, domain, failure, and bit-parity checks.
- `tests/compile/m7_*.cc`: four standalone public-header checks, combined API
  contracts, and six ownership/type/const negative contracts.
- `tests/consumer/{sparse_cuda,random_cuda,random_dense_cuda,random_sparse_cuda}/main.cc`:
  isolated component consumers.
- `benchmarks/sparse_cuda/sparse_cuda_benchmark.cc` and
  `benchmarks/random_cuda/random_cuda_benchmark.cc`: synchronized,
  checksum-verified smoke benchmarks with independent result oracles.

### User and architecture documentation

- `README.md`, `CHANGELOG.md`, `docs/README.md`, and `docs/api.md`: documented
  version `0.7.0`, explicit CUDA component requests, public APIs, and limits.
- `docs/modules/sparse.md` and `docs/modules/random.md`: documented provider
  operations, ownership/lifetime, exact scalar rules, algorithms, workspaces,
  reproducibility, and costs.
- `docs/modules/core.md` and `docs/modules/dense.md`: documented shared CUDA
  memory/event behavior used by M7.
- `docs/development/asc-cpp-architecture/architecture-blueprint.md`,
  `backend-capability-matrix.md`, `capability-manifest.yaml`,
  `dependency-manifest.yaml`, `release-roadmap.md`, `testing-strategy.md`, and
  `ci-strategy.md`: advanced capability and evidence state only through M7.

## 2. APIs, targets, and direct dependency changes

### Public APIs

- `SparseCudaContext`, `CudaStridedVectorView`, `CudaCsrClone`,
  `CudaCloneCsr`, `CudaCsrSpmvWorkspaceSize`, `CudaCsrSpmv`, and sparse
  `CudaEvaluate`.
- `CudaRandomWordGeneration` and `CudaFillPhilox4x32`.
- `CudaDenseUniform01Generation` and `CudaFillDenseUniform01`.
- `CudaSparseUniform01Generation` and `CudaGenerateSparseUniform01`.
- `CoordinateView::RebindValues` and
  `CompressedSparseView::RebindValues`.

No CUDA SDK type appears in a public signature. Common sparse/random headers
remain provider-neutral. `ASC::cpp` remains provider-free.

### Product targets and exact direct edges

| Build target / installed target | Exact direct ASC dependencies | Direct external dependency |
| --- | --- | --- |
| `asc_sparse_cuda` / `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` | private `CUDA::cusparse` |
| `asc_random_cuda` / `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | none added; CUDA Runtime closure comes through `core_cuda` |
| `asc_random_dense_cuda` / `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | none added |
| `asc_random_sparse_cuda` / `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | none added |

The matching package components are `sparse_cuda`, `random_cuda`,
`random_dense_cuda`, and `random_sparse_cuda`. They exist only with
`ASC_CPP_ENABLE_CUDA=ON`. No cuRAND, Thrust/CUB API dependency, cuSOLVER,
Driver API, or unapproved package was added.

Installed shared-object inspection confirmed `libcusparse.so.12` only on
`libasc_sparse_cuda.so`; all four facets use `libcudart.so.12` through the
approved CUDA closure.

## 3. Exact commands and pass/fail/skip results

Toolchain/hardware:

```text
CMake 4.1.2
GNU C++ 11.4.0
Clang 19.0.0
nvcc 12.9.86 / CUDA Toolkit 12.9
Driver 576.83
NVIDIA GeForce RTX 3060 Laptop GPU, 6144 MiB, compute capability 8.6
Compiled CUDA architecture: 86
ASCCMake_DIR:
  /home/yicai/AI4SciComp/asc-cmake/build/test-debug
asc-cmake source HEAD:
  8a7dcbad3a97267cce59810aff24de800a3497a7
```

### Formatting and source checks

```sh
/usr/bin/clang-format-19 -i \
  include/asc/sparse/coordinate.h include/asc/sparse/compressed.h \
  src/core/execution_internal.h src/core/cuda/cuda.cc \
  include/asc/sparse/providers/*.h include/asc/random/providers/*.h \
  src/sparse/cuda/*.{cc,cu,h} src/random/cuda/*.{cc,cu,h} \
  tests/sparse_cuda/*.cc tests/random_cuda/*.{cc,h} \
  tests/random_dense_cuda/*.cc tests/random_sparse_cuda/*.cc \
  tests/compile/m7*.cc \
  tests/consumer/{sparse_cuda,random_cuda,random_dense_cuda,random_sparse_cuda}/main.cc \
  benchmarks/{sparse_cuda,random_cuda}/*.cc

/usr/bin/clang-format-19 --dry-run --Werror \
  include/asc/sparse/coordinate.h include/asc/sparse/compressed.h \
  src/core/execution_internal.h src/core/cuda/cuda.cc \
  include/asc/sparse/providers/*.h include/asc/random/providers/*.h \
  src/sparse/cuda/*.{cc,cu,h} src/random/cuda/*.{cc,cu,h} \
  tests/sparse_cuda/*.cc tests/random_cuda/*.{cc,h} \
  tests/random_dense_cuda/*.cc tests/random_sparse_cuda/*.cc \
  tests/compile/m7*.cc \
  tests/consumer/{sparse_cuda,random_cuda,random_dense_cuda,random_sparse_cuda}/main.cc \
  benchmarks/{sparse_cuda,random_cuda}/*.cc

git diff --check
```

Result: pass, pass, pass.

The portability reviewer also ran:

```sh
for f in \
  tests/compile/m7_sparse_cuda_header.cc \
  tests/compile/m7_random_cuda_header.cc \
  tests/compile/m7_random_dense_cuda_header.cc \
  tests/compile/m7_random_sparse_cuda_header.cc \
  tests/compile/m7_cuda_contracts.cc
do
  clang++-19 -std=c++20 -fsyntax-only -Iinclude \
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
    -Werror -fno-exceptions "$f"
done
```

Result: pass for 5/5 translation units.

### Clean CPU-only Release static

```sh
cmake -S . -B /tmp/asc-cpp-m7-final-cpu-static.Gs21bC/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m7-final-cpu-static.Gs21bC/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cpu-static.Gs21bC/build \
  --output-on-failure --no-tests=error
cmake --install /tmp/asc-cpp-m7-final-cpu-static.Gs21bC/build \
  --prefix /tmp/asc-cpp-m7-final-cpu-static.Gs21bC/prefix
```

Result: configure pass; build pass; 161/161 pass, 0 fail, 0 skip,
386.37 seconds; install pass.

An earlier attempt selected Ninja in
`/tmp/asc-cpp-m7-final-cpu-static.NLildp` and failed at configuration because
Ninja was not installed. That environment failure was corrected by using the
available `Unix Makefiles` generator above; no source failure was involved.

### Clean CPU-only Debug shared

```sh
cmake -S . -B /tmp/asc-cpp-m7-final-cpu-shared.nEcEvg/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m7-final-cpu-shared.nEcEvg/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cpu-shared.nEcEvg/build \
  --label-exclude 'package|consumer' --output-on-failure --no-tests=error
cmake --install /tmp/asc-cpp-m7-final-cpu-shared.nEcEvg/build \
  --prefix /tmp/asc-cpp-m7-final-cpu-shared.nEcEvg/prefix
```

Result: configure pass; build pass; 137/137 pass, 0 fail, 0 skip,
14.06 seconds; shared install pass.

### Clang ASan plus UBSan

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m7-final-clang-san.GkxJr3/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m7-final-clang-san.GkxJr3/build --parallel 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
ctest --test-dir /tmp/asc-cpp-m7-final-clang-san.GkxJr3/build \
  --label-exclude 'package|consumer' --output-on-failure --no-tests=error
```

Result: configure pass; build pass; 137/137 pass, 0 fail, 0 skip,
33.36 seconds; no ASan, leak, or UBSan finding.

### Requested CUDA unavailable negative gate

```sh
cmake -S . -B /tmp/asc-cpp-m7-final-cuda-unavailable.LOe5Bc/build \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_COMPILER=/definitely/unavailable/nvcc \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
```

Result: expected configuration failure because the requested compiler path
does not exist; negative gate pass.

### Clean CUDA Release static and package matrix

Initial clean configuration/build:

```sh
cmake -S . -B /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  --output-on-failure --no-tests=error
```

The first full diagnostic run passed 224/226. It exposed two shared-core
integration regressions: M6 address-overflow classification and an M6
exact-self dense no-op rejected by the new ASC allocation registry. The four
M7 runtimes, both M7 benchmarks, all M7 compile/negative checks, all 31
consumers, and all four aggregate package tests passed in that run. The lead
fixed validation ordering and preserved no-access exact-self behavior.

Post-fix current-code commands:

```sh
cmake --build /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  --label-exclude package --output-on-failure --no-tests=error
ctest --test-dir /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  --label-regex consumer --output-on-failure --no-tests=error
cmake --install /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  --prefix /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/prefix
```

Result: rebuild pass; 191/191 non-package tests pass, 0 fail, 0 skip,
24.32 seconds; 31/31 isolated consumers pass, 0 fail, 0 skip,
268.67 seconds; install pass.

Aggregate package results from the same clean configuration:

```text
asc_cpp.package.build_tree_components:            pass, 525.58 s
asc_cpp.package.install_and_relocate_components:  pass, 542.56 s
asc_cpp.package.registry_unchanged:                pass,   1.04 s
asc_cpp.package.cuda_unavailable:                  pass,   1.58 s
```

### Clean CUDA Release shared

```sh
cmake -S . -B /tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/build \
  -R '^asc_cpp\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.' \
  --output-on-failure --no-tests=error
cmake --install /tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/build \
  --prefix /tmp/asc-cpp-m7-final-cuda-shared.jGwUQR/prefix
```

Result: configure pass; build pass; 17/17 focused M7 tests pass, 0 fail,
0 skip, 11.24 seconds; shared install pass.

### Final independent focused verification

```sh
cmake --build /tmp/asc-cpp-m7-verification-n7zSsT \
  --target asc_sparse_cuda_test asc_random_cuda_test \
           asc_random_dense_cuda_test asc_random_sparse_cuda_test \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-verification-n7zSsT \
  -R '^asc_cpp\.(sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.runtime$' \
  --output-on-failure
```

Result: build pass; 4/4 pass, 0 fail, 0 skip, 1.15 seconds; independent
verification disposition accepted.

After the lead resolved the two shared-core integration regressions, the
verifier performed one final read-only code audit and ran:

```sh
cmake --build /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  --target asc_core_cuda_test asc_dense_cuda_storage_evaluate_test \
           asc_sparse_cuda_test asc_random_cuda_test \
           asc_random_dense_cuda_test asc_random_sparse_cuda_test \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.storage_evaluate|sparse_cuda\.runtime|random_cuda\.runtime|random_dense_cuda\.runtime|random_sparse_cuda\.runtime)$' \
  --output-on-failure --no-tests=error
```

Result: build pass; 6/6 pass, 0 fail, 0 skip, 1.37 seconds. The final
integration audit found no production-code blocker.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Sanitizers

- Clang 19 ASan+UBSan, CUDA disabled: 137/137 pass with leak detection and
  halt-on-error enabled.
- Final real-device memcheck command:

```sh
for m7_binary in \
  /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build/tests/sparse_cuda/asc_sparse_cuda_test \
  /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build/tests/random_cuda/asc_random_cuda_test \
  /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build/tests/random_dense_cuda/asc_random_dense_cuda_test \
  /tmp/asc-cpp-m7-final-cuda-static.fdeHNi/build/tests/random_sparse_cuda/asc_random_sparse_cuda_test
do
  compute-sanitizer --tool memcheck --error-exitcode=99 \
    --leak-check=full "$m7_binary"
done
```

Result: 4/4 pass; every process reported `0 bytes leaked in 0 allocations`
and `0 errors`.

### Package and relocation

- CPU Release static full package/consumer matrix: pass within 161/161.
- CUDA Release static aggregate package matrix: 4/4 pass, including copied
  build-tree package paths, original install prefix, relocated prefix with
  spaces, required/quiet components, disabled registry behavior, and requested
  CUDA unavailable behavior.
- Final current-code standalone consumers: 31/31 pass.
- The four M7 consumers each passed build-tree and install-relocate modes:
  8/8 pass.
- Static install and shared CPU/CUDA installs passed.
- No package registry was mutated.

## 5. CPU/GPU provider evidence

| Evidence | Classification | Result |
| --- | --- | --- |
| CUDA disabled, static and shared configuration with no M7 target/toolkit discovery | `configure-tested` | pass |
| CUDA requested with unavailable compiler | `configure-tested` | expected failure pass |
| CUDA Toolkit 12.9, nvcc 12.9.86, architecture 86 | `configure-tested` | pass |
| Four M7 facets, static and shared, GNU 11.4 + nvcc C++20, warnings as errors | `compile-tested` | pass |
| Public headers/contracts under strict Clang 19 with exceptions disabled | `compile-tested` | 5/5 pass |
| Four M7 facets on RTX 3060 Laptop GPU, driver 576.83 | `runtime-tested` | 4/4 focused runtimes and 17/17 shared focused suite pass |
| Sparse CSR clone/SpMV/evaluator versus independent CPU reference | `parity-tested` | float/double numerical parity pass |
| Raw Philox versus independent CPU word oracle | `parity-tested` | bit parity pass |
| Dense Uniform01 versus independent provider-free/CPU oracle | `parity-tested` | float/double bit parity pass |
| Sparse exact-count coordinates and values versus independent CPU oracle | `parity-tested` | canonical coordinate and bit parity pass |
| Trusted device CSC evaluator success path | `skipped` | no approved M7 trusted CSC producer |
| Multi-GPU, MSVC/nvcc, Clang CUDA, AppleClang, 32-bit, big-endian, and non-NVIDIA providers | `skipped` | unavailable locally |
| Cross-toolkit/cross-hardware bitwise SpMV identity | `skipped` | no claim; local deterministic algorithm and numerical parity only |

CPU code supplied the independent numerical/bit oracles; no CPU fallback was
executed by a CUDA API.

## 6. Review findings and resolutions

Production self-review is complete. Independent verification is accepted with
the disclosed external-allocation limit. Documentation/API review is complete
with disclosed limits. Portability/GPU/performance review is accepted after
correction.

- M7-VER-001: supported evaluator operations were initially unreachable.
  Resolved by implementing the bounded public dispatch and opaque launch.
- M7-VER-002 / M7-DOC-08: arbitrary device CSR/coordinate canonicality could
  not be established safely without synchronization. Resolved with
  non-user-settable trusted provenance from validated owners/producers; raw
  device sparse views are rejected.
- M7-VER-003 / M7-DOC-02 / PORT-001: CUDA Runtime cannot discover the terminal
  bound of every arbitrary external allocation. Resolved exactly for
  `CudaMemoryResource` allocations with a no-throw registry; external pointer
  bounds remain a disclosed risk.
- M7-VER-004 / M7-DOC-09: SpMV algorithm/workspace behavior needed separation.
  Resolved: unit stride uses `CUSPARSE_SPMV_CSR_ALG2` with explicit queried
  workspace; positive nonunit stride uses the deterministic ASC kernel with
  zero workspace.
- M7-VER-005 / M7-DOC-11: trusted immutable sparse structure could not safely
  be reused with new values. Resolved with checked `RebindValues`.
- M7-VER-006 / PORT-002: nonzero scalar add/subtract could densify implicit
  zeros. Resolved by accepting only exact zero for add/subtract, while scalar
  multiplication remains supported; independent rejection/parity tests pass.
- M7-VER-007 / M7-DOC-11: structure/value overlap needed rejection. Resolved
  with checked rebinding and independent partial-overlap rejection tests.
- PORT-003: cuSPARSE device/error guarding needed hardening. Resolved with a
  persistent context-device guard and mutex, stale Runtime-error clearing, and
  narrow failure drains.
- M7-DOC-01: zero-count raw work recorded an unnecessary event. Resolved with
  an internal already-complete event.
- M7-DOC-03 through M7-DOC-07: alignment/lifetime wording, downstream provider
  friendship, sparse-random full cost, rank greater than eight, and sparse
  context move destruction were corrected in API, implementation, and docs.
- M7-DOC-10 / PORT-005: strict Clang/format defects were corrected; final
  strict Clang, `clang-format-19 --dry-run --Werror`, and `git diff --check`
  pass.
- PORT-004: the first benchmark set lacked double SpMV and independent sparse
  checks, and one benchmark CSR generator wrapped into noncanonical indices.
  Resolved with float/double SpMV, independent random oracles, sorted unique
  CSR columns, checksums, and passing reruns.
- Lead integration diagnostic: the first clean full CUDA run found two M6
  regressions introduced by shared allocation-bound validation. Resolved by
  ordering integer address-span overflow before provider inspection and
  validating only the touched pointer for an exact-self no-access dense copy.
  The corrected Release CUDA suite passes 191/191 and consumers pass 31/31.

No reviewed-scope release blocker remains.

## 7. Performance evidence

These are smoke observations, not speedup, scalability, regression-threshold,
cross-toolkit, or cross-hardware claims. Timed regions exclude setup and
include explicit completion waits. Every observation has an independent
checksum/oracle.

Environment: RTX 3060 Laptop GPU, driver 576.83, CUDA 12.9, Release,
architecture 86.

| Operation | Work | Warmups / repetitions | Observation |
| --- | --- | --- | --- |
| CSR SpMV float | 1024x1024, 5120 nnz | 3 / 20 | 1167 us total, 704 workspace bytes, checksum 10240 |
| CSR SpMV double | 1024x1024, 5120 nnz | 3 / 20 | 1247 us total, 752 workspace bytes, checksum 10240 |
| Raw Philox | 1,048,576 words | 3 / 20 | 1471 us total, checksum 2253713208691922 |
| Dense Uniform01 float | 1024x1024, right layout | 3 / 20 | 4150 us total, checksum 524733 |
| Sparse Uniform01 float | 128x128, exact count 128, allocation included | 1 / 5 | 4,101,647 us total, coordinate checksum 16629, value checksum 66.3448 |

Sparse random is intentionally correctness-first:
`O(exact_count * logical_size + exact_count^2 * rank)` time, exactly two result
allocations, and no computational workspace. The observation confirms the
documented non-scalable cost.

## 8. Remaining risks

1. The true terminal bound of an arbitrary external CUDA Runtime pointer or
   suballocation cannot be proven without an unapproved Driver API. Placement,
   device, alignment, representable address span, and declared span are still
   checked; ASC-owned allocations have exact bounds.
2. The public CSC evaluator template has no reachable successful M7 path
   because no approved operation produces trusted device CSC. Adding staging
   or a user assertion token would be a later approved change.
3. Local GPU evidence covers one Linux x86-64 host, one NVIDIA device, one
   driver/toolkit, GNU as the nvcc host compiler, and architecture 86.
4. Sparse random is deliberately low-workspace and non-scalable. It is not
   suitable for large domains without a later algorithm milestone.
5. CUDA provider host code was not built under ASan/UBSan because the local
   nvcc/host-sanitizer matrix was not approved as a release configuration.
   Strict Clang host parsing and real-device Compute Sanitizer memcheck cover
   complementary surfaces.
6. cuSPARSE ALG2 repeatability is bounded to the recorded environment; no
   cross-toolkit or cross-hardware bitwise SpMV guarantee is made.
7. If device selection fails during a no-throw sparse-context destructor, the
   implementation favors leaking the provider handle over destroying it on the
   wrong device.
8. The cumulative M0--M7 implementation remains uncommitted over the unchanged
   baseline. A publication commit must intentionally compose that cumulative
   tree and exclude unrelated owner work.

## 9. Exact proposed remote and branch-cleanup actions

Nothing in this section has been executed.

Running a push now would publish only the unchanged baseline `HEAD`, not M7.
First obtain explicit approval for the cumulative M0--M7 commit composition
and review its exact staged manifest. After that local commit exists, the
proposed remote publication command is:

```sh
git push --set-upstream origin feature/asc-cpp-m7-gpu-sparse-random
```

No force push is proposed. PR creation, merge, tag, and release remain separate
approval checkpoints.

Only after the remote branch has been reviewed and merged, and only with
separate cleanup approval, the proposed local and remote branch cleanup is:

```sh
git switch main
git pull --ff-only origin main
git branch -d feature/asc-cpp-m7-gpu-sparse-random
git push origin --delete feature/asc-cpp-m7-gpu-sparse-random
```

Until those approvals exist, retain the current branch and working tree
unchanged.
