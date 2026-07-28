# Milestone 8 Independent Verification Design

Status: Frozen before production-hardening inspection

Date: 2026-07-28

Branch: `feature/asc-cpp-m8-hardening-downstream-r2`

## Independence statement

This oracle was derived only from the frozen Milestone 8 contract, ownership
ledger, repository instructions, the approved Milestone 7 Publication
Checkpoint B report, and the Milestone 7 public headers, target declarations,
and package configuration. It was frozen before inspecting `tools/hardening`,
`abi`, or the superseded Milestone 8 branch or pull request.

The verification implementation owns no product, package, shared CMake, or
production-reporting path. A failed oracle is reported to the lead; it is not
worked around by weakening the test.

## Frozen surface oracle

### Version, components, and closures

The candidate version is exactly `0.9.0`. Package version compatibility is
`SameMinorVersion`: `0.9` and exact `0.9.0` requests succeed; older or newer
pre-1.0 minors and any different major fail. A request newer than the
installed patch, such as `0.9.1`, also fails because the installed candidate
is too old.

The ordered known-component list is exactly:

```text
core
utilities
expression
dense
sparse
random
random_dense
random_sparse
cpp
core_cuda
dense_cuda
sparse_cuda
random_cuda
random_dense_cuda
random_sparse_cuda
```

A CUDA-disabled package has exactly the first nine available components. A
CUDA-enabled package has all fifteen. Default lookup requests `cpp` and loads
only the nine provider-free targets. Explicit component lookup loads the exact
transitive target closure below and no unrelated target:

```text
core:
utilities: core
expression: core
dense: core expression
sparse: core expression
random: core
random_dense: random dense
random_sparse: random sparse
cpp: core utilities expression dense sparse random random_dense random_sparse
core_cuda: core
dense_cuda: dense core_cuda
sparse_cuda: sparse core_cuda
random_cuda: random core_cuda
random_dense_cuda: random_dense random_cuda core_cuda
random_sparse_cuda: random_sparse random_cuda core_cuda
```

Unknown required components and known-but-unavailable required components
fail truthfully. Quiet optional misses are nonfatal and set the corresponding
`ASCCpp_<component>_FOUND` value false. Provider-free lookup must succeed with
`CMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE`, must not create any `CUDA::*`
target, and must not add a CUDA SDK link or include requirement. A requested
CUDA closure must discover CUDAToolkit and must fail truthfully when it is
disabled or unavailable.

Every loaded target propagates `cxx_std_20`. Compiled targets retain their
approved output names, target kind, exact direct ASC link interface, and
linkage-appropriate `ASC_*_STATIC_DEFINE` behavior. Interface facets and
`ASC::cpp` remain interface libraries. No package lookup may use the CMake
user package registry.

### Public-header manifest

The complete source manifest contains 49 headers. The provider-free manifest
contains 37:

```text
core:
  asc/core.h
  asc/core/configuration.h
  asc/core/contracts.h
  asc/core/execution.h
  asc/core/export.h
  asc/core/extents.h
  asc/core/io.h
  asc/core/memory.h
  asc/core/result.h
  asc/core/status.h
  asc/core/types.h
utilities:
  asc/utilities.h
  asc/utilities/command_line.h
  asc/utilities/export.h
  asc/utilities/timer.h
expression:
  asc/expression.h
  asc/expression/expression.h
  asc/expression/writable.h
dense:
  asc/dense.h
  asc/dense/array.h
  asc/dense/evaluate.h
  asc/dense/export.h
  asc/dense/layout.h
  asc/dense/linalg.h
  asc/dense/view.h
sparse:
  asc/sparse.h
  asc/sparse/compressed.h
  asc/sparse/coordinate.h
  asc/sparse/evaluate.h
  asc/sparse/export.h
  asc/sparse/linalg.h
random:
  asc/random.h
  asc/random/distribution.h
  asc/random/engine.h
  asc/random/export.h
random_dense:
  asc/random/dense.h
random_sparse:
  asc/random/sparse.h
```

The twelve CUDA headers are:

```text
core_cuda:
  asc/core/providers/cuda.h
  asc/core/providers/cuda_export.h
dense_cuda:
  asc/dense/providers/cuda.h
  asc/dense/providers/cuda_export.h
sparse_cuda:
  asc/sparse/providers/cuda.h
  asc/sparse/providers/cuda_export.h
random_cuda:
  asc/random/providers/cuda.h
  asc/random/providers/cuda_export.h
random_dense_cuda:
  asc/random/providers/dense_cuda.h
  asc/random/providers/dense_cuda_export.h
random_sparse_cuda:
  asc/random/providers/sparse_cuda.h
  asc/random/providers/sparse_cuda_export.h
```

The source tree, exported target file sets, and physical installed header tree
must agree exactly with the applicable manifest. CUDA headers are
SDK-independent at parse time but belong only to CUDA component file sets and
CUDA-enabled installs. Deleted compatibility headers such as `asc/array.h`
and `asc/linalg.h` must remain absent.

Every public header must parse as a first include in C++20, parse with
exceptions disabled where supported, and be order-independent when included
with a second public header. Provider headers must parse without directly
including a CUDA SDK header. Multi-TU use must not introduce duplicate
definitions.

### Public API and symbol observations

Verification treats the Milestone 7 C++ API as immutable. In particular, the
only raw CUDA random entry point is:

```text
CudaFillPhilox4x32(ExecutionContext, MutableMemoryView, word_count,
                   stream, subsequence, offset)
```

A bare-pointer overload is a failure.

Runtime surface checks use only public constructors, factories, views,
expression builders, evaluation, and reductions. Shared-object checks are
bounded ELF observations, not ABI promises. They require representative
exported symbols for each compiled component, hidden internal implementation
names, and the intentional Core support symbol needed by public templates:
`asc::CompletionEvent::CompletionEvent(bool)`. Static builds instead verify
the static-definition macro and successful isolated linkage.

The ABI record may contain compiler-specific mangled names, sizes, alignment,
and symbol versions. Verification compares only exact repeated observations
within one compiler, standard library, linkage, and configuration. It makes
no cross-toolchain or cross-minor compatibility claim.

## Independent tests

### Package metadata and version probe

`tests/hardening/package_metadata_version_test.cmake` will create isolated
throwaway consumers and verify:

1. exact version, known list, available list, and per-component `FOUND`
   variables;
2. default and every explicit component closure;
3. unknown required, unavailable required, optional quiet miss, and
   CUDA-disabled/unavailable behavior;
4. accepted `0.9`, accepted exact `0.9.0`, rejected `0.8`, `0.10`, `1.0`, and
   `0.9.1` requests;
5. provider-free CUDAToolkit isolation; and
6. disabled user-package-registry lookup.

The same probe is parameterized for build-tree, copied-build-tree, installed,
relocated, and path-with-spaces packages and for static/shared and
CUDA-disabled/enabled producer configurations.

### Header/file-set equivalence probe

`tests/hardening/header_manifest_test.cmake` carries the independent 49-header
oracle above. It configures a package consumer that obtains each imported
target's header file set, normalizes it to `asc/...`, and compares exact
target ownership. When an installed include directory is supplied, it also
compares the physical header tree byte-for-byte with the expected enabled
set and compares every installed header with its source counterpart.

### API and ownership probes

`tests/hardening/public_api_surface_test.cc` exercises a provider-free public
storage/expression workflow and records representative public type size and
alignment observations. Compile-time assertions cover move-only owners and
events, const-view conversion, C++20 propagation, and the absence of a
pointer-only raw Random CUDA overload. CUDA-specific signature assertions are
compiled only when the provider header is part of the requested package.

`tests/hardening/shared_symbol_test.cmake` inspects local ELF shared objects
with `nm` plus the configured demangler. It checks representative public
definitions and the completed-event support symbol, rejects exposed
`internal_*` implementation names, and writes a deterministic observation
record. Non-ELF hosts are an explicit skip.

### Compile-time and object-size observations

Three representative translation units live in `benchmarks/hardening`:

- Core types and `Result`;
- Dense storage plus expression evaluation; and
- all provider-free public umbrella headers.

`benchmarks/hardening/observe_compile_object.cmake` compiles each unit from an
installed include tree as C++20, records the exact compiler/version/flags,
elapsed compile observation, and object byte size, and emits a checksum of
the source plus observed object. There is no threshold or cross-machine
speedup claim. A failed compile is a failed probe; timing noise is reported.

### asc-xde-shaped downstream trial

The trial uses only `find_package(ASCCpp 0.9 REQUIRED COMPONENTS dense)` and
links only `ASC::dense`. It performs one explicit-Euler step on a
two-dimensional host Dense state:

```text
derivative = -decay * state
next = state + timestep * derivative
```

It verifies every resulting value and a reduction checksum. This is a small
ODE-oriented storage/evaluation workflow, not an asc-xde API or migration
claim.

The isolated consumer additionally asserts that `ASC::core`,
`ASC::expression`, and `ASC::dense` are the only imported ASC targets;
CUDAToolkit discovery is disabled; no `CUDA::*` target exists; and deleted
`asc/array.h` and `asc/linalg.h` headers are unavailable.

`tests/downstream/run_asc_xde_trial.cmake` runs the fixture against build-tree,
installed, relocated, and path-with-spaces package paths. Scratch trees are
outside the real asc-xde repository. Before and after the trial it verifies
the real repository commit
`abcb29b51f22f40afd7f174707b7ccf83c32d4bf`, records status unchanged, and
performs no downstream write.

## Matrix and evidence rules

Lead integration must register these independent probes for every locally
valid GCC/Clang, Debug/Release, static/shared, and CUDA-disabled/enabled
configuration. ASan+UBSan executes the runtime probes. CUDA-enabled builds
compile the immutable signature oracle and re-run the existing real-device
parity tests; this verification layer adds no duplicate numerical CUDA
implementation.

GPU evidence is classified only as `configure-tested`, `compile-tested`,
`runtime-tested`, `parity-tested`, or `skipped`. Hosted CI, non-ELF symbol
formats, missing compilers/generators, non-Linux systems, unavailable
sanitizers, and unavailable GPU topology/toolkit combinations are skips
rather than inferred passes.

## Integration requested from the lead

The lead exclusively supplies:

- `tests/hardening/CMakeLists.txt`,
  `tests/downstream/CMakeLists.txt`, and root registration;
- package/build/install/relocation paths and enabled-component parameters;
- shared-library paths and the configured symbol tools;
- compiler, include-tree, flags, and scratch paths for compile observations;
- clean full-matrix execution; and
- any product/package repair proven necessary by these tests.

No later-milestone implementation, product API addition, compatibility
overload, dependency, repository mutation, or publication action is part of
this design.
