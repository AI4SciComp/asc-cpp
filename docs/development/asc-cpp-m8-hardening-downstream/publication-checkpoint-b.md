# Milestone 8 Publication Checkpoint B

Status: corrected local candidate independently accepted; Publication
Checkpoint B reached; publication not authorized

Date: 2026-07-28

Milestone: **Milestone 8 — packaging/API/performance/downstream hardening**

Branch: `feature/asc-cpp-m8-hardening-downstream-r2`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the exact cumulative, intentionally uncommitted Milestones 0--7
candidate recorded at the Milestone 7 Publication Checkpoint B

Corrections: all 20 actionable findings from the post-checkpoint independent
review and its first correction re-review were accepted for correction.

The frozen correction boundary and integrated evidence are recorded in:

- [`review-correction-contract.md`](review-correction-contract.md);
- [`review-correction-ownership.md`](review-correction-ownership.md);
- [`review-correction-verification.md`](review-correction-verification.md);
- [`review-correction-documentation.md`](review-correction-documentation.md);
- [`review-correction-integration.md`](review-correction-integration.md); and
- [`review-correction-portability.md`](review-correction-portability.md) after
  its final independent re-review.

The integration report is authoritative for the corrected candidate's changed
files, exact validation counts, package/relocation results, GPU evidence,
performance observations, resolved findings, residual evidence limits, and
proposed publication/cleanup actions. The original checkpoint below is
retained as the pre-correction audit snapshot; where its evidence or
“no corrections” state differs, the correction records supersede it.

## Original pre-correction scope result

The frozen Milestone 8 contract is satisfied. The unreleased package candidate
advances from 0.7.0 to 0.9.0 and hardens the existing package, public-surface
inventory, symbol/ABI observations, performance evidence, and downstream
integration. It adds no public C++ API, product target, component, direct
product dependency, provider, numerical operation, or later-milestone work.

The product surface remains exactly 15 targets when CUDA is enabled and nine
when CUDA is disabled. The source public-header manifest contains 49 headers;
the provider-free installed projection contains 37. The owner-approved M7
`MutableMemoryView + word_count` raw Random CUDA API is unchanged and is the
only exported form.

Four separate roles ran with disjoint write scopes:

1. production hardening implementation;
2. independent verification;
3. documentation and API review; and
4. portability, GPU, and performance review.

The lead alone changed shared root CMake/package files and integration. All
four reviews accept the candidate. No material design decision remains
unresolved.

No commit, push, pull-request mutation, merge, tag, release, registry write,
branch deletion, or other remote/history action occurred. The prior
`feature/asc-cpp-m8-hardening-downstream` branch and draft pull request 1 were
preserved unchanged because their unique commit predates the approved M7 API
amendment.

## 1. Changed files and reasons

This Milestone 8 layer is cumulative over the approved uncommitted
Milestones 0--7 predecessor. Shared paths can therefore contain work from
more than one approved milestone. The exact candidate is inspectable with:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
```

At checkpoint freeze, the cumulative worktree has 496 default porcelain
entries (711 when every untracked file is expanded), 334 tracked status
entries, 334 staged paths, 39 tracked-unstaged paths, and 377 untracked files.
The M8 preflight recorded 484 default porcelain entries, 334 staged paths, 38
tracked-unstaged paths, and 334 untracked files. M8 preserved the predecessor
and added the bounded hardening layer and checkpoint records above it.

The M8-specific changes are grouped below.

### Root build, package, CI, and test integration

```text
.github/workflows/ci.yml
CHANGELOG.md
CMakeLists.txt
CMakePresets.json
README.md
cmake/ASCCppComponents.cmake
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
tests/CMakeLists.txt
tests/hardening/CMakeLists.txt
tests/downstream/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/package_test.cmake
tests/package/repeated_components/CMakeLists.txt
tests/package/repeated_components/main.cc
```

These advance package metadata to unreleased 0.9.0, retain
`SameMinorVersion`, add the release/shared and M8 presets, register clean
package/install/relocation fixtures, test repeated component lookup, enforce
installed metadata path hygiene, and integrate hardening and downstream
evidence. Actual ASCCMake 0.1.0 APIs remain in use; standard CMake continues
to own the approved multi-export component package behavior.

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

These mechanically inventory the public headers, target/component surface,
target properties and direct links, installed header equivalence, and bounded
ELF/shared-symbol observations. The baselines are local pre-1.0 observations,
not a cross-toolchain ABI promise.

### Independent verification, downstream, and compile-cost evidence

```text
tests/hardening/package_metadata_version_test.cmake
tests/hardening/header_manifest_test.cmake
tests/hardening/shared_symbol_test.cmake
tests/hardening/public_api_surface_test.cc
tests/downstream/run_asc_xde_trial.cmake
tests/downstream/asc_xde_trial.cc
benchmarks/hardening/compile_core.cc
benchmarks/hardening/compile_dense.cc
benchmarks/hardening/compile_provider_free.cc
benchmarks/hardening/observe_compile_object.cmake
```

These independently check package/version behavior in every package mode,
the 49/37-header projections, immutable API and move-only contracts,
representative shared symbols, relocated installed-header compile/object
observations, and an asc-xde-shaped `dense` consumer in build-tree, copied,
installed, relocated, and path-with-spaces configurations.

### Live user and maintainer documentation

```text
docs/README.md
docs/api.md
docs/modules/core.md
docs/modules/utilities.md
docs/modules/expression.md
docs/modules/dense.md
docs/modules/sparse.md
docs/modules/random.md
docs/support-matrix.md
docs/api-compatibility.md
docs/package-capabilities.md
docs/extension-guide.md
docs/downstream-integration.md
docs/performance.md
```

These document 0.9 package lookup and metadata, exact component closures,
source/ABI/provider compatibility boundaries, extension ownership rules,
downstream integration, the M7 Random CUDA capacity/lifetime contract, support
limits, GPU evidence terminology, and the threshold-free performance
envelope.

### Architecture, dependency, and roadmap records

```text
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/development/asc-cpp-architecture/release-roadmap.md
```

These mark the M8 checkpoint, record the exact local CPU/CUDA/sanitizer
revalidation, and preserve the approved 15-target graph. They do not claim a
release or Milestone 9 completion.

### Contract, ownership, provenance, and independent reviews

```text
docs/development/asc-cpp-m8-hardening-downstream/milestone-contract.md
docs/development/asc-cpp-m8-hardening-downstream/ownership.md
docs/development/asc-cpp-m8-hardening-downstream/preflight.md
docs/development/asc-cpp-m8-hardening-downstream/dependency-audit.md
docs/development/asc-cpp-m8-hardening-downstream/provenance-record.md
docs/development/asc-cpp-m8-hardening-downstream/production-self-review.md
docs/development/asc-cpp-m8-hardening-downstream/verification-design.md
docs/development/asc-cpp-m8-hardening-downstream/verification-review.md
docs/development/asc-cpp-m8-hardening-downstream/documentation-api-review.md
docs/development/asc-cpp-m8-hardening-downstream/portability-review.md
docs/development/asc-cpp-m8-hardening-downstream/publication-checkpoint-b.md
```

These freeze the revision-2 boundary, disjoint ownership, clean-room and
dependency results, independent oracle, all review findings and resolutions,
explicit skips, and this checkpoint.

The real `asc-xde` repository remained clean and read-only at
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf`. The unrelated modified MdeCpp
`Makefile` and all other sibling repositories were preserved.

## 2. APIs, targets, and direct dependency changes

### Public C++ API

No public C++ API was added, removed, or intentionally changed in M8.

The hardened raw Random CUDA signature remains:

```text
CudaFillPhilox4x32(ExecutionContext, MutableMemoryView, word_count,
                   stream, subsequence, offset)
```

No pointer-plus-count overload exists. Independent source/API, compiled
consumer, and exported-symbol checks passed.

### CMake package API

The candidate package version is now 0.9.0 with `SameMinorVersion`
compatibility. These existing metadata variables are reviewed and documented:

```text
ASCCpp_VERSION
ASCCpp_KNOWN_COMPONENTS
ASCCpp_AVAILABLE_COMPONENTS
ASCCpp_<component>_FOUND
```

Compatible 0.9.x requests pass; incompatible pre-1.0 minors and majors fail.
Unknown required and unavailable required components fail truthfully. Quiet
optional misses remain nonfatal. CUDA discovery occurs only for a requested
CUDA closure.

### Targets and direct dependencies

No target, component, or direct dependency edge changed. The exact product set
remains:

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

The direct product graph remains exactly the approved dependency manifest.
`ASC::cpp` is provider-free. CUDA Runtime, cuBLAS, and cuSPARSE remain the only
provider implementation dependencies. No cuRAND, CUDA Driver, cuSOLVER, NCCL,
OpenMP, Eigen, MKL, HIP, SYCL, or other dependency was added.

ASCCMake remains exact released 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

## 3. Exact commands and pass/fail/skip results

### Preflight and branch boundary

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main
git status --porcelain=v1
git diff --cached --name-only
git diff --name-only
git ls-files --others --exclude-standard
git worktree list --porcelain
git ls-remote --heads origin
```

Result: PASS. `HEAD`, `main`, and `origin/main` were
`33b261ea33616a6395c4ad3b20646093103344f7`; one worktree existed. The
required predecessor was the intentionally dirty approved M7 candidate, not a
clean commit. The stale local/remote M8 branch remained at
`d611aa876576ab949c2c213977f628d2844539ba`.

Repository `main:AGENTS.md`, the architecture package, ADRs 0001--0018,
dependency/capability manifests, backend matrix, roadmap, M7 checkpoint,
repository/provenance audits, implementation/testing/CI/ASCCMake guidance, and
the complete runbook were read. No material decision remained unresolved.

### Lead clean GCC Debug/static validation

```sh
cmake -S . -B /tmp/asc-cpp-m8-lead-final.0BzAot/build \
  -G 'Unix Makefiles' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m8-lead-final.0BzAot/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m8-lead-final.0BzAot/build \
  --output-on-failure --parallel 4
```

Result: PASS, 189/189, 197.66 seconds. This was a fresh configure/build and
included all package, consumer, hardening, performance-smoke, relocation, and
downstream fixtures.

### CPU compiler/mode/linkage matrix

For each GCC 11.4 or Clang 19, Debug or Release, static or shared row:

```sh
cmake -S . -B <build> -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=<Debug-or-Release> \
  -DCMAKE_CXX_COMPILER=</usr/bin/g++-or-/usr/bin/clang++-19> \
  -DBUILD_SHARED_LIBS=<OFF-or-ON> \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build <build> --parallel 4
ctest --test-dir <build> --output-on-failure --parallel 4 \
  --label-exclude 'package|consumer|downstream|performance'
cmake --install <build> --prefix <prefix>
```

Results:

| Compiler | Mode | Linkage | Result |
| --- | --- | --- | --- |
| GCC 11.4 | Debug | static | PASS, 137/137; 37/37 installed headers |
| GCC 11.4 | Debug | shared | PASS, 139/139; 37/37 installed headers |
| GCC 11.4 | Release | static | PASS, 137/137; 37/37 installed headers |
| GCC 11.4 | Release | shared | PASS, 139/139; 37/37 installed headers |
| Clang 19 | Debug | static | PASS, 137/137; 37/37 installed headers |
| Clang 19 | Debug | shared | PASS, 139/139; 37/37 installed headers |
| Clang 19 | Release | static | PASS, 137/137; 37/37 installed headers |
| Clang 19 | Release | shared | PASS, 139/139; 37/37 installed headers |

The independent complete GCC Release/static suite passed 189/189 in 137.21
seconds. The Clang Release/shared M8 selection passed 54/54 in 152.31 seconds.
Each CPU configure enabled nine provider-free targets and truthfully skipped
six CUDA targets.

### Full GCC/NVCC CUDA validation

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build \
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
  /tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build \
  --parallel 4
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-cuda-final.1GL6Sq/gcc-release-shared/build \
  --output-on-failure --parallel 4
```

Result: PASS. Configure found 15/15 targets with GCC 11.4, NVCC 12.9.86,
GNU 11.4 host, CUDA Toolkit 12.9, and architecture 86. Build passed; the full
suite passed 245/245 in 1468.34 seconds on the RTX 3060 Laptop GPU.

### Mixed Clang C++/NVCC CUDA validation

```sh
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-cuda-clang-mixed.rJ9Y9Z/build \
  --output-on-failure \
  -R 'asc_cpp\.(core_cuda\.|dense_cuda\.|sparse_cuda\.(runtime|benchmark)|random_cuda\.(runtime|benchmark)|random_dense_cuda\.runtime|random_sparse_cuda\.runtime|consumer\.(core_cuda|dense_cuda|sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)\.)'
```

Result: PASS, 28/28 in 219.40 seconds after a successful complete build. This
used Clang 19 for C++ translation units, NVCC 12.9 for CUDA translation units,
and GNU 11 as NVCC's actual host compiler. It is not evidence for Clang as the
CUDA compiler or NVCC host.

### Independent focused verification

```sh
ctest --test-dir <clean-shared-build> --output-on-failure --parallel 4 \
  -L milestone-8
ctest --test-dir <integrated-static-build> --output-on-failure --parallel 4 \
  -L milestone-8
ctest --test-dir <cuda-build> --output-on-failure \
  -R 'asc_cpp\.(core_cuda|dense_cuda|sparse_cuda|random_cuda|random_dense_cuda|random_sparse_cuda)'
```

Results: PASS, 28/28 clean shared hardening/downstream; PASS, 25/25 integrated
static hardening/downstream; PASS, 13/13 real-device provider runtime/parity
revalidation. The CUDA package independently passed all 15 component closures,
49 headers, the immutable Random signature, and 11 shared-library symbol
checks.

### Refreshed architecture and hygiene checks

```sh
cmake -S . -B build/m8-integration-smoke -G 'Unix Makefiles' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=g++ \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
ctest --test-dir build/m8-integration-smoke \
  -L architecture --output-on-failure -j1
find tests/hardening tests/downstream benchmarks/hardening \
  tests/package/repeated_components -type f \
  \( -name '*.cc' -o -name '*.h' -o -name '*.hpp' -o -name '*.cu' \) \
  -print0 | xargs -0 --no-run-if-empty \
  clang-format-19 --dry-run --Werror
git diff --check
git diff --cached --check
cmake --list-presets
cmake --list-presets=build
cmake --list-presets=test
```

Results: PASS, architecture 7/7; formatting PASS; both diff checks PASS; all
configure/build/test presets parse.

No required test failed. Skips are recorded in Sections 4, 5, and 8.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Sanitizers

```sh
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
ctest \
  --test-dir /tmp/asc-cpp-m8r2-port-sanitizers-final.39I6he/asan-ubsan/build \
  --output-on-failure --parallel 4 \
  --label-exclude 'package|consumer|downstream|performance|abi'
```

Result: PASS, ASan+UBSan 137/137 in 0.49 seconds, with no sanitizer
diagnostic.

Standalone LSan and TSan instrumentation configured successfully. The exact
sanitizer-safe runtime subset passed LSan 12/12 in 0.10 seconds and TSan 12/12
in 0.34 seconds. Complete LSan/TSan test-suite builds are **skipped** because
deliberate test executables overriding global allocation functions conflict
with those sanitizer runtimes' interceptors; this is not reported as a product
pass.

Compute Sanitizer used:

```sh
compute-sanitizer --tool memcheck --leak-check full \
  --error-exitcode=99 <runtime-executable>
```

Result: PASS for 12/12 applicable CUDA runtime executables, each with zero
errors and zero leaked bytes. The native-state impossible-allocation negative
is **skipped** under memcheck because its intended failing `cudaMalloc` is
reported as one API error; its normal CTest passed. Racecheck is **skipped** at
the documented cuSPARSE `beta == 0` false-positive boundary. Initcheck and
synccheck were not run.

### Package, relocation, and consumers

- CPU and CUDA package tests passed from build tree, copied build tree,
  installed tree, relocated prefix, and paths containing spaces.
- Registry-disabled lookup, CUDA-disabled isolation, unavailable-component
  negatives, version requests, and repeated component requests passed.
- The CUDA build-tree package aggregate passed in 310.86 seconds; the
  installed/relocated aggregate passed in 305.11 seconds.
- Every one of the 15 component closures passed. The CUDA package installed
  49/49 byte-equivalent headers; CPU packages installed the exact 37-header
  provider-free projection.
- All 12 CUDA isolated consumers passed: six CUDA facets against both the
  build-tree and installed/relocated package. All provider-free isolated
  consumers in the 189-test CPU suites passed.
- No source-tree or independent build-tree absolute path appeared in
  relocated installed package metadata.
- The asc-xde-shaped trial passed build-tree, copied build-tree, installed,
  and relocated path-with-spaces modes. It requested only `dense`, observed
  exactly the `core`/`expression`/`dense` closure, kept CUDA discovery off, and
  produced checksum `19.95`.

## 5. CPU/GPU provider evidence

### CPU

The provider-free CPU surface is configure-, compile-, and runtime-validated
across GCC/Clang, Debug/Release, and static/shared on the recorded host.
Provider-free shared objects `core`, `utilities`, `dense`, `sparse`, and
`random` have no CUDA Runtime, cuBLAS, or cuSPARSE dependency even in the
CUDA-enabled build.

### GPU

The frozen vocabulary is used exactly:

| Facet | Classification |
| --- | --- |
| `core_cuda` | `configure-tested`; `compile-tested`; `runtime-tested` |
| `dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |

Core CUDA is not promoted to `parity-tested`; the frozen independent
classification does not define a separate parity oracle for it. The other
five facets agree with independent numerical, structural, or random-bit
oracles on the real device.

Provider edges remain confined to their approved closures: Core CUDA uses
CUDA Runtime; Dense CUDA uses cuBLAS and CUDA Runtime; Sparse CUDA uses
cuSPARSE and CUDA Runtime; the three Random CUDA facets use ASC/Core CUDA and
CUDA Runtime only.

Trusted device CSC evaluation, multi-GPU, other GPU architectures/toolkits,
cross-device access, MIG, and a true Clang CUDA compiler/host are `skipped`.

## 6. Review findings and resolutions

### Production hardening review

- **PROD-001:** prior M8 baselines were stale. Regenerated from the approved
  revision-2 M7 API and current target/header surface.
- **PROD-002/003:** compiler-owned weak symbols and intentional support symbols
  are ABI-observable. Disclosed as bounded local evidence, not a compatibility
  promise.
- **PROD-004:** ELF tooling is platform-specific. Non-ELF inputs report
  `skipped`.
- **PROD-005:** shared objects have unversioned SONAMEs. Accepted under the
  disclosed pre-1.0 policy.
- **PROD-006:** an old install prefix was not final evidence. Fresh isolated
  prefixes replaced it.
- **PROD-007:** no product or package defect remained.

### Independent verification

- **VER-007:** compile/object driver was GNU-only. It now supports explicit
  `gnu` and `msvc` styles; unavailable MSVC execution remains skipped.
- **VER-008:** a filtered clean CTest could build the repeated-component
  consumer without setup. Fixture setup/requirements now make the dependency
  explicit.
- **VER-009:** compile observations lacked complete method fields. Build mode,
  hardware/provider, workload, warmup/repetitions, synchronization, result,
  checksums, and noise are now recorded.
- Package truthfulness, immutable M7 signature, downstream claim boundary,
  and symbol/ABI evidence limits were independently accepted.

### Documentation and API review

All DOC-001 through DOC-017 findings are resolved or accepted. Notable fixes
include the 0.9 candidate examples, removal of the stale predecessor-branch
roadmap claim, correction of an invented Random CUDA context, removal of an
unapproved extension operation category, exact CUDA synchronization language,
non-Cartesian CUDA matrix wording, and exact sanitizer scope.

### Portability/GPU/performance review

- **PORT-001:** compile/object probes initially used source headers. They now
  compile the relocated installed headers and require the relocation fixture.
- **PORT-002:** GNU-`nm` shared-symbol registration was too broad. It is now
  Linux-gated; the general inspector truthfully skips non-ELF inputs.
- **PORT-003/004:** complete LSan/TSan and the intentional CUDA allocation
  negative have the exact skip boundaries recorded above.
- **PORT-005:** the mixed CUDA toolchain is named precisely and is not
  overclaimed as Clang CUDA-host evidence.

No open product, package, API, dependency, target, symbol, header, downstream,
or locally exercised portability blocker remains.

## 7. Performance evidence

All observations are correctness-checked, threshold-free, and local to the
recorded Intel i7-11800H / RTX 3060 Laptop environment. They are not
cross-machine guarantees or CPU/GPU speedup claims.

### Installed-header compile/object observations

Each observation used zero warmup, one repetition, compiler-process exit as
synchronization, relocated installed headers, and recorded filesystem/cache/
scheduling noise.

| Compiler | Translation unit | Object bytes | Elapsed |
| --- | --- | ---: | ---: |
| GCC 11.4 | `compile_core.cc` | 6,576 | 0.706864 s |
| GCC 11.4 | `compile_dense.cc` | 271,920 | 0.756905 s |
| GCC 11.4 | `compile_provider_free.cc` | 2,048 | 0.767745 s |
| Clang 19 | `compile_core.cc` | 5,552 | 0.848178 s |
| Clang 19 | `compile_dense.cc` | 220,616 | 0.766944 s |
| Clang 19 | `compile_provider_free.cc` | 1,328 | 1.00464 s |

Object and source SHA-256 values are recorded in the portability review and
generated reports.

### Runtime observations

All six Release benchmark executables returned zero.

```text
CPU Dense evaluate: 64 iterations, 57,820 ns/iteration,
  checksum=1283, allocations=0
CPU Dense GEMM: 4 iterations, 17,732.5 ns/iteration,
  checksum=20790.7, allocations=0
CPU Sparse evaluate: 64 iterations, 131,129 ns/iteration,
  checksum=-574.125, allocations=0
CPU Sparse SpMV: 256 iterations, 5,782.64 ns/iteration,
  checksum=977.375, allocations=0
CPU Random aggregate checksum=8664949311000451886

CUDA Dense: H2D=7.291850490 GB/s, D2H=7.185404784 GB/s,
  D2D=73.581914030 GB/s, evaluate=5.977600587 GB/s,
  AXPY=30.321767700 GFLOP/s, GEMV=52.830310360 GFLOP/s,
  GEMM=3039.608844000 GFLOP/s, checksum=64.54296875,
  operation allocations=0
CUDA Sparse CSR SpMV: float=35,948,400 nnz/s,
  checksum=14940377177479771011; double=95,031,400 nnz/s,
  checksum=9178157494086742915; operation allocations=0
CUDA raw Random: 14,197,600,000 words/s,
  checksum=13841617604916660332, allocations=0
CUDA Dense Random: 8,156,760,000 values/s,
  checksum=15165452046652654026, allocations=0
CUDA Sparse Random: 26,697.4 selected values/s,
  checksum=7322770344431580519
```

Sparse Random retains its approved correctness-oriented
`O(logical_size * exact_count + exact_count^2)` selection cost. No
performance threshold or unapproved optimization was introduced.

## 8. Remaining risks and explicit skips

- Windows/MSVC, macOS/AppleClang, native non-WSL Linux variants, other
  standard libraries, CPU architectures, Ninja, and hosted CI are skipped.
- Other CUDA toolkits, drivers, GPU architectures, Windows CUDA, true Clang
  CUDA compilation/hosting, multi-GPU, cross-device access, MIG, and
  cross-toolkit/driver/architecture parity are skipped.
- Trusted device CSC evaluation is skipped because no approved producer
  constructs that view.
- Complete standalone LSan/TSan suites are skipped at deliberate test
  allocator interposition; the exact safe subsets passed.
- Compute Sanitizer racecheck, initcheck, and synccheck are not claimed.
- ELF/GNU-`nm` evidence does not cover PE/COFF or Mach-O. Unversioned SONAMEs
  and compiler/STL weak symbols remain disclosed pre-1.0 ABI risks.
- Compile/object and runtime measurements are single-host observations without
  statistical thresholds.
- CUDA Sparse Random's approved selection algorithm may be slow for large
  domains/counts.
- Caller-owned external CUDA views retain the valid-storage precondition;
  Runtime pointer attributes cannot prove every caller-declared terminal
  allocation bound.
- Hosted required CI and remote independent review have not run because this
  checkpoint does not authorize publication.
- The old M8 branch contains a unique commit based on the obsolete pointer
  Random API. It remains preserved and must not be deleted without a separate
  content/ancestry audit after the revision-2 candidate is committed.

None of these is a blocker within the frozen locally available M8 contract.

## 9. Exact proposed remote and branch-cleanup actions

These commands are proposals only. They were not executed.

### Candidate review, commit, and publication

```sh
git status --short --branch
git diff --cached --check
git diff --check
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort

# After owner publication approval, stage only the reviewed cumulative
# Milestones 0--8 candidate and inspect the exact result.
git add --all
git diff --cached --check
git diff --cached --stat main
git diff --cached --name-status main
git commit -m "Complete asc-cpp Milestone 8 hardening"

git push --set-upstream origin \
  feature/asc-cpp-m8-hardening-downstream-r2
gh pr create \
  --base main \
  --head feature/asc-cpp-m8-hardening-downstream-r2 \
  --title "Complete asc-cpp Milestone 8 hardening" \
  --body-file docs/development/asc-cpp-m8-hardening-downstream/publication-checkpoint-b.md
```

Then inspect every required check and independent review, fix failures on the
same branch with ordinary commits, and merge only after all required checks
pass. Do not force-push and do not push directly to `main`.

The roadmap designates 0.9.0 as an unreleased candidate, not a release.
Therefore the proposed publication has **no tag and no GitHub release**.

### Prior draft branch and pull request

First preserve and compare the unique prior attempt:

```sh
git fetch --prune origin
git log --left-right --cherry-pick --oneline \
  main...origin/feature/asc-cpp-m8-hardening-downstream
git range-diff \
  main...origin/feature/asc-cpp-m8-hardening-downstream \
  main...origin/feature/asc-cpp-m8-hardening-downstream-r2
git diff --stat \
  origin/feature/asc-cpp-m8-hardening-downstream \
  origin/feature/asc-cpp-m8-hardening-downstream-r2
git worktree list --porcelain
```

Close pull request 1 as superseded only after the revision-2 pull request is
open and its exact replacement boundary is recorded. Do not delete the old M8
local or remote branch unless every intended unique change is present in the
merged revision-2 history or separately preserved. Its obsolete pointer API
must not be merged into revision 2.

### Obsolete milestone branch cleanup after merge

After the revision-2 pull request is merged, fetch the resulting `main`, move
this worktree to `main`, and audit every branch individually:

```sh
git fetch --prune origin
git switch main
git pull --ff-only origin main
git worktree list --porcelain

git merge-base --is-ancestor feature/asc-cpp-m0-foundation main
git merge-base --is-ancestor feature/asc-cpp-m1-core main
git merge-base --is-ancestor feature/asc-cpp-m2-independent-foundations main
git merge-base --is-ancestor feature/asc-cpp-m3-dense-cpu main
git merge-base --is-ancestor feature/asc-cpp-m4-sparse-cpu main
git merge-base --is-ancestor feature/asc-cpp-m5-random-storage-generation main
git merge-base --is-ancestor feature/asc-cpp-m6-gpu-core-dense main
git merge-base --is-ancestor feature/asc-cpp-m7-gpu-sparse-random main
git merge-base --is-ancestor feature/asc-cpp-m8-hardening-downstream-r2 main
```

Only when each command succeeds and no worktree uses the branch, delete each
local branch separately:

```sh
git branch -d feature/asc-cpp-m0-foundation
git branch -d feature/asc-cpp-m1-core
git branch -d feature/asc-cpp-m2-independent-foundations
git branch -d feature/asc-cpp-m3-dense-cpu
git branch -d feature/asc-cpp-m4-sparse-cpu
git branch -d feature/asc-cpp-m5-random-storage-generation
git branch -d feature/asc-cpp-m6-gpu-core-dense
git branch -d feature/asc-cpp-m7-gpu-sparse-random
git branch -d feature/asc-cpp-m8-hardening-downstream-r2
```

No remote M0--M7 branches currently exist. After merge, verify the revision-2
remote ancestry and worktree state, then delete only that remote branch:

```sh
git merge-base --is-ancestor \
  origin/feature/asc-cpp-m8-hardening-downstream-r2 origin/main
git push origin --delete feature/asc-cpp-m8-hardening-downstream-r2
```

Never delete `main`, tags, an active worktree branch, a recovery branch, or
unpreserved unique work. The old revision-1 M8 branch is explicitly retained
at this checkpoint.

## Checkpoint disposition

Milestone 8 is locally complete and accepted at Publication Checkpoint B.
Work stops here. Publication, hosted CI, pull-request review, merge, release,
and audited branch cleanup require separate authorization.
