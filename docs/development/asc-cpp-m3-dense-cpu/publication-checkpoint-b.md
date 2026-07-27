# Milestone 3 Publication Checkpoint B

Status: Complete locally; stopped for owner approval

Date: 2026-07-26

Approved milestone: **Milestone 3 — dense CPU**

Corrections: None from the owner

Branch: `feature/asc-cpp-m3-dense-cpu`

Uncommitted baseline and current `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

No commit, push, merge, tag, release, branch deletion, or remote mutation was
performed.

## Checkpoint disposition

Milestone 3 is accepted by all four scoped roles and by lead integration with
no unresolved release-blocking local finding. The production implementation,
independently derived tests, documentation/API review, and
portability/GPU/performance review are reconciled.

The complete clean local matrices passed:

```text
CMake 4.1.2, GCC 11.4, Debug, static:          108/108
CMake 4.1.2, Clang 19.0, Debug, shared:        108/108
CMake 3.25.0, GCC 11.4, Release, static:       108/108
Clang 19.0, ASan+UBSan, eligible non-package:    94/94
final focused architecture+dense recheck:         14/14
```

GPU evidence is exactly **skipped**. Hosted MSVC and AppleClang validation
remains a publication gate and is not pre-claimed by local evidence.

## 1. Changed files and reasons

### Dense production

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
src/dense/CMakeLists.txt
src/dense/reference_linalg.cc
```

These files add the compiled dense component, its export definitions, validated
layouts and views, move-only host storage, storage-neutral expression
evaluation and reductions, and deterministic serial reference linear algebra.

### Independent tests, consumers, and benchmark

```text
tests/dense/CMakeLists.txt
tests/dense/allocation_counter.cc
tests/dense/allocation_counter.h
tests/dense/array_test.cc
tests/dense/evaluate_test.cc
tests/dense/layout_view_test.cc
tests/dense/linalg_test.cc
tests/dense/test_support.h
tests/compile/m3_dense_contract.cc
tests/compile/m3_dense_multi_tu.h
tests/compile/m3_dense_multi_tu_a.cc
tests/compile/m3_dense_multi_tu_b.cc
tests/compile/m3_dense_multi_tu_main.cc
tests/compile/m3_dense_negative_const_mutation.cc
tests/compile/m3_dense_negative_owner_copy.cc
tests/compile/m3_dense_negative_unsupported_element.cc
tests/consumer/dense/CMakeLists.txt
tests/consumer/dense/main.cc
benchmarks/dense/allocation_free_benchmark.cc
```

These files provide independent layout, view, owner, expression, reduction,
linear-algebra, constness, ODR, negative-compilation, component-import,
relocation, and allocation-observation evidence.

### Lead-owned integration

```text
CMakeLists.txt
CMakePresets.json
.github/workflows/ci.yml
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
tests/CMakeLists.txt
tests/compile/CMakeLists.txt
tests/consumer/CMakeLists.txt
tests/consumer/run_component_consumer.cmake
tests/consumer/run_subproject_consumer.cmake
tests/consumer/subproject/CMakeLists.txt
tests/consumer/subproject/main.cc
tests/package/CMakeLists.txt
tests/package/package_test.cmake
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/dependency_check.cmake
```

These changes register `asc_dense`, export/install the `dense` component,
enforce exact dependency and public-file inventories, add dense-only package
and subproject consumers, and extend static/shared, sanitizer, minimum-CMake,
and hosted-CI coverage.

The component-consumer version checks for core, utilities, expression, and
random were updated from the predecessor candidate to the cumulative `0.3.0`
package candidate where required by the shared package harness.

### Documentation, manifests, and review records

```text
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/dense.md
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/release-roadmap.md
docs/development/asc-cpp-m3-dense-cpu/milestone-contract.md
docs/development/asc-cpp-m3-dense-cpu/ownership.md
docs/development/asc-cpp-m3-dense-cpu/dependency-audit.md
docs/development/asc-cpp-m3-dense-cpu/provenance-record.md
docs/development/asc-cpp-m3-dense-cpu/production-self-review.md
docs/development/asc-cpp-m3-dense-cpu/verification-design.md
docs/development/asc-cpp-m3-dense-cpu/verification-review.md
docs/development/asc-cpp-m3-dense-cpu/documentation-api-review.md
docs/development/asc-cpp-m3-dense-cpu/portability-review.md
docs/development/asc-cpp-m3-dense-cpu/publication-checkpoint-b.md
```

These files freeze the scope and ownership, document the public API and
observable costs, advance the architecture manifests to Milestone 3, record
all specialist evidence and resolutions, and preserve clean-room provenance.

### Retained predecessor and unrelated work

The working tree deliberately remains a cumulative, uncommitted Milestones
0--3 candidate based on the same baseline commit. It also retains the user's
earlier deletion of the legacy asc-cpp implementation. Milestone 3 did not
restore, reset, discard, commit, or reclassify those changes.

The verified MdeCpp checkout at `/home/yicai/repo/MdeRepo/MdeCpp` remains on
`main` at `f6294e9079262682ce63ae7ff2d8a643e658bf5d`, with its unrelated modified
`Makefile` untouched.

## 2. APIs, targets, and direct dependency changes

### Exact target graph after Milestone 3

```text
asc_core        / ASC::core        -> []
asc_utilities   / ASC::utilities   -> [ASC::core]
asc_expression  / ASC::expression  -> [ASC::core]
asc_random      / ASC::random      -> [ASC::core]
asc_dense       / ASC::dense       -> [ASC::core, ASC::expression]
```

`asc_expression` remains an interface target. The other implemented targets
are compiled static or shared libraries according to `BUILD_SHARED_LIBS`.
No sparse, random storage facet, aggregate, retired array/linalg, or provider
target exists.

The new exact target contract is:

```text
build target:                asc_dense
build alias/imported target: ASC::dense
kind:                        compiled static/shared library
public compile feature:      cxx_std_20
direct public links:         ASC::core;ASC::expression
package component:           dense
candidate package version:   0.3.0
```

Dense has no direct or transitive production use of utilities, sparse, random,
BLAS/LAPACK, Eigen, OpenMP, TBB, CUDA, HIP, SYCL, or another provider.

### Public API added

- `LayoutLeft`, `LayoutRight`, `LayoutStride`,
  `DenseLayoutKind`, and `DenseLayoutMapping<Rank>`.
- `DenseView<Element, Rank>` with checked `At`, rank-preserving `Subview`,
  const propagation, exact-view identity, and conservative physical
  `MayOverlap`.
- `DenseArray<Element, ExtentsType>` bound to core `Extents<...>`, with
  `Create`, `view`, explicit-resource/context `Clone`, and transactional
  `ResizeDiscard`.
- `ExpressionAdapter<DenseView<...>>`, `Evaluate`, `ReduceSum`, `ReduceMin`,
  and `ReduceMax`.
- `MatrixOperation::{kNone,kTranspose}` and float/double `Copy`, `Scal`,
  `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm`.

The API is in flat `namespace asc`, uses self-contained `.h` headers and a
compiled `.cc` reference implementation, requires explicit memory resources
and execution contexts where state matters, and exposes no implicit provider
selection.

The exact ASCCMake release used is the clean `v0.1.0` contract at commit
`8a7dcbad3a97267cce59810aff24de800a3497a7` (annotated tag object
`620b2e912ac5bac7561e09529a65cce965ebc920`). Root integration requires
`find_package(ASCCMake 0.1.0 EXACT CONFIG REQUIRED)` and uses only the verified
APIs `asc_target_enable_cxx20`, `asc_target_enable_warnings`,
`asc_target_enable_sanitizers`, and `asc_register_test`.

## 3. Exact validation commands and results

### GCC 11.4, CMake 4.1.2, Debug static

```bash
cmake -S . \
  -B /tmp/asc-cpp-m3-final-gcc.C9L2bB/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m3-final-gcc.C9L2bB/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-final-gcc.C9L2bB/build \
  --output-on-failure --parallel 4
```

Result: configure pass, build pass, **108/108 passed**; 14 package-labeled
tests passed; total CTest time 234.67 seconds.

### Clang 19, CMake 4.1.2, Debug shared

The same commands used:

```text
-DCMAKE_CXX_COMPILER=clang++-19
-DBUILD_SHARED_LIBS=ON
build root=/tmp/asc-cpp-m3-final-clang.U4vc0c/build
```

Result: configure pass, build pass, **108/108 passed**; all package and
consumer fixtures passed; total CTest time 230.61 seconds.

### Minimum CMake 3.25.0, GCC 11.4, Release static

The GCC command matrix was repeated with:

```text
cmake=/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/cmake
ctest=/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/ctest
-DCMAKE_BUILD_TYPE=Release
-DBUILD_SHARED_LIBS=OFF
build root=/tmp/asc-cpp-m3-final-cmake325.w9O6pZ/build
```

Result: configure pass, build pass, **108/108 passed**; total CTest time
211.15 seconds.

### Final post-review format and focused recheck

```bash
clang-format-19 --dry-run --Werror -style=file \
  include/asc/dense.h include/asc/dense/*.h src/dense/*.cc \
  tests/dense/*.cc tests/dense/*.h \
  tests/compile/m3_dense*.cc tests/compile/m3_dense*.h \
  tests/consumer/dense/*.cc benchmarks/dense/*.cc
git diff --check
ctest --test-dir /tmp/asc-cpp-m3-final-gcc.C9L2bB/build \
  --output-on-failure \
  -R '^(asc_cpp\.(architecture|compile\.dependency)|asc_cpp\.dense\.)'
```

Result: formatting pass, whitespace pass, **14/14 passed**.

Every dense public header and `src/dense/reference_linalg.cc` also passed
direct strict GCC 11 and Clang 19 C++20 compilation with:

```text
-std=c++20 -pedantic-errors -Wall -Wextra
-Wconversion -Wsign-conversion -Werror
```

Both exception-enabled and `-fno-exceptions` configurations passed. All 3/3
negative sources failed compilation as required.

`clang-tidy-19` is not installed locally, so no local clang-tidy result is
claimed. The hosted Clang CI job retains its production-source clang-tidy 18
step.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Sanitizers

```bash
cmake -S . \
  -B /tmp/asc-cpp-m3-final-sanitizer.Fxwc9Z/build \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m3-final-sanitizer.Fxwc9Z/build --parallel 4
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m3-final-sanitizer.Fxwc9Z/build \
  --output-on-failure -LE package
```

Result: configure/build pass, **94/94 eligible tests passed**, with no ASan,
leak, or UBSan diagnostic.

Package consumers were intentionally validated in the non-instrumented
matrices so sanitizer runtime linkage is not exported or mistaken for a
package dependency.

### Package and consumer coverage

Each complete 108-test matrix passed all 14 package-labeled cases:

- build-tree and copied-build-tree component requests;
- install and relocation;
- relocation prefixes containing spaces;
- exact available and unavailable component behavior;
- no-component and optional-unavailable component behavior;
- package-registry isolation;
- add-subdirectory/subproject use;
- isolated build-tree and relocated consumers.

A `dense`-only request imports exactly `ASC::dense`, `ASC::core`, and
`ASC::expression`. It does not import utilities, random, sparse, a random
storage facet, an aggregate, or a provider. Static and shared exports both
publish `cxx_std_20` and the exact dependency closure; the static export also
publishes `ASC_DENSE_STATIC_DEFINE`.

## 5. CPU/GPU provider evidence

The serial CPU reference backend is **runtime-tested** locally with:

- GCC 11.4 static Debug and Release builds;
- Clang 19 shared Debug;
- Clang 19 ASan+UBSan;
- arbitrary validated non-negative strides and left/right/padded layouts;
- deterministic evaluation, reductions, and all seven linear-algebra
  operations.

No optimized CPU provider is implemented or claimed. Symbol and dynamic-link
inspection found no optional provider dependency.

The host inventory includes CUDA 12.9, NVIDIA driver 576.83, and an NVIDIA
GeForce RTX 3060 Laptop GPU with 6 GB. This inventory is not evidence for M3:
there is no GPU target, option, source, or provider in the approved milestone.
Nothing was configured, compiled, executed, or parity-compared for a GPU.

GPU evidence: **skipped**.

No `configure-tested`, `compile-tested`, `runtime-tested`, or `parity-tested`
GPU claim is made.

## 6. Review findings and resolutions

### Production implementation

The production role delivered the entire frozen public/compiled surface
without crossing its ownership scope or adding a dependency. Its self-review
confirmed C++20, strict warnings, self-contained headers, exception-disabled
compilation, exact host/serial behavior, and clean-room provenance.

### Independent verification

The verifier found and closed:

- `V-M3-001`: rank-zero `Evaluate` triggered GCC `-Wtype-limits`; corrected
  with compile-time guarding.
- `DOC-M3-01`: zero-extent mappings must be vacuously unique/exhaustive.
- `DOC-M3-02`: Boolean/volatile and cv constraints needed truthful rejection.
- `DOC-M3-03`: mutable linalg inputs needed independent readable-operand
  deduction.
- `DOC-M3-04`: huge zero-containing named mappings must avoid irrelevant
  stride overflow.
- `DOC-M3-05`: owners must bind specifically to unqualified core
  `Extents<...>`.

Its final independent results were dense 10/10, header configurations 14/14,
dense consumers 3/3, sanitizer dense 10/10, and negative compilation 3/3.

### Documentation and API review

The documentation reviewer independently raised `DOC-M3-01` through
`DOC-M3-05` above and verified every correction. `DOC-M3-06` clarified that
the no-allocation guarantee covers successful computational storage,
temporaries, workspace, and packing; failed `Status` diagnostics may allocate
through `std::string`. The exact installed guide example configured, compiled,
linked, ran, and passed repository formatting using only `ASC::dense`.

### Lead integration

`M3-LEAD-01` found that expression alias validation needed physical byte-span
overlap, not only alias-token equality. Recursive dense-terminal overlap
checking was added and independently regression-tested.

### Portability/GPU/performance review

The independent reviewer accepted the milestone with no unresolved blocker
after GCC static dense 27/27, Clang shared dense 27/27, focused Clang
ASan+UBSan 7/7, CMake 3.25 Release build, exception-disabled compilation,
formatting, export, and provider-isolation checks.

Its residual findings are evidence boundaries:

- successful numerical paths allocate no computational storage/workspace, but
  diagnostic `Status` creation may allocate;
- the benchmark is an allocation/timing observation, not a stable baseline;
- MSVC and AppleClang await hosted CI;
- external `DenseView` pointer provenance, allocation length, alignment, and
  lifetime remain caller preconditions;
- conservative alias/layout checks may reject some safe exotic cases.

No review finding remains unresolved.

## 7. Performance evidence

The project-owned benchmark measures setup-free successful `Evaluate(add)` and
`Gemm` operation scopes at shape 32x32 for 100 iterations. It records compiler,
configuration, backend, shape, operation set, allocation count, elapsed time,
and checksum.

Lead observations:

```text
GCC 11.4.0, Release-like:
allocations_in_operations=0 elapsed_us=19831 checksum=4.75

GCC 11.4.0, Debug:
allocations_in_operations=0 elapsed_us=158523 checksum=4.75

Clang 19.0.0, Debug:
allocations_in_operations=0 elapsed_us=163026 checksum=4.75
```

The independent verifier's separate Release-like observation was:

```text
allocations_in_operations=0 elapsed_us=20585 checksum=4.75
```

There is no timing pass/fail threshold, warm-up protocol, repetition
distribution, baseline, optimized-provider comparison, cross-compiler speed
claim, or GPU comparison. The timing values are informational only. The
meaningful frozen evidence is deterministic execution, checksum 4.75, and
zero observed C++ heap allocations in the measured successful operations.

## 8. Remaining risks and publication gates

- Hosted VS2022 shared multi-config and macOS 15 AppleClang jobs must pass
  before publication; no local Windows DLL or macOS install-name/runtime claim
  is made.
- Local clang-tidy was unavailable; the configured hosted check must run.
- Non-owning views can dangle when callers violate owner/resource lifetime.
- External expressions can misreport placement, indexing, or alias metadata.
- Conservative physical-span and arbitrary-stride proofs can reject safe
  padded/interleaved layouts.
- External pointer provenance, allocation length, alignment, and actual
  placement cannot be proven by `DenseView::Create`.
- The serial scalar kernels are correctness baselines, not optimized CPU
  provider performance.
- Failure diagnostics may allocate even though successful dense computations
  do not allocate computational storage or workspace.
- The cumulative uncommitted Milestones 0--3 tree must be partitioned and
  reviewed intentionally before any remote publication.

These are documented caller obligations, conservative approved behavior,
hosted validation gates, or later-milestone work. None justifies expanding
Milestone 3.

License/provenance remains clean: the unchanged asc-cpp Apache-2.0 license has
SHA-256
`c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4`,
matching asc-cmake. MdeCpp has distinct license hash
`230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809`;
no MdeCpp or deleted asc-cpp source, test, vector, table, or prose was copied.

## 9. Exact proposed remote and branch-cleanup actions

No immediate push of the current cumulative working tree is proposed. The
existing milestone branches all still point at the common baseline, while
Milestones 0--3 are uncommitted in one working tree. Publishing that state as
one giant Milestone 3 change would destroy the approved milestone review
boundaries.

After a separate owner publication approval, the proposed sequence is:

1. Partition the cumulative tree into separately audited, ordered M0, M1, M2,
   and M3 commits without changing file content.
2. Reuse/move each existing feature branch to its corresponding audited
   milestone tip, with each later branch based on its predecessor.
3. Re-run the applicable clean validation at each exact commit.
4. Push and review in dependency order:

```bash
git push -u origin feature/asc-cpp-m0-foundation
git push -u origin feature/asc-cpp-m1-core
git push -u origin feature/asc-cpp-m2-independent-foundations
git push -u origin feature/asc-cpp-m3-dense-cpu
```

Each push occurs only after its branch has the intended content and parent,
and each pull request targets `main` only after its predecessor is preserved
or merged. No tag or release is proposed at this checkpoint.

Only after merge, remote confirmation, ancestry verification, clean-worktree
verification, and confirmation that no unique work remains would branch
cleanup be proposed, individually:

```bash
git branch -d feature/asc-cpp-m0-foundation
git branch -d feature/asc-cpp-m1-core
git branch -d feature/asc-cpp-m2-independent-foundations
git branch -d feature/asc-cpp-m3-dense-cpu
git push origin --delete feature/asc-cpp-m0-foundation
git push origin --delete feature/asc-cpp-m1-core
git push origin --delete feature/asc-cpp-m2-independent-foundations
git push origin --delete feature/asc-cpp-m3-dense-cpu
```

Those commands are proposals, not executed actions. Branch deletion requires
separate explicit approval after the safety checks.

## Stop

The repository is stopped at Publication Checkpoint B. No later milestone,
remote publication, or cleanup action is authorized or in progress.
