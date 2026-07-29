# Milestone 6 Documentation and API Review

Status: documentation complete against the implemented public headers;
integration/runtime evidence pending lead and verifier

Date: 2026-07-28

Role: independent documentation and API review

## Scope and authority

The review read the frozen Milestone 6 contract, ownership ledger, provenance
record, relevant accepted ADRs, provider manifests/matrix, existing Core and
Dense module guides, and the implemented public headers. It did not inspect or
copy MdeCpp, deleted asc-cpp CUDA code, or Milestone 8 implementation.

The review changed only its assigned files:

```text
docs/modules/core.md
docs/modules/dense.md
docs/development/asc-cpp-m6-gpu-core-dense/documentation-api-review.md
```

No production, CMake, test, manifest, package, provider, or later-milestone
file was changed by this role.

## Public API review

### Core and Core CUDA

The provider-neutral `ExecutionContext` and `CompletionEvent` evolution
remains SDK-free. Serial construction and serial `CopyBytes` remain available.
Contexts are copyable immutable handles; events are move-only. `Query()` and
`Wait()` return structured failures and do not expose a CUDA type.

`<asc/core/providers/cuda.h>` exposes exactly:

```text
CudaDeviceCount
CudaMemoryResource::Create
CreateCudaExecutionContext
RecordCudaEvent
```

The memory resource is noncopyable and nonmovable, accepts only pinned-host,
device, or managed space, and retains a stable address for its non-owning
`Buffer` users. Device selection is an explicit signed ordinal. No public
signature exposes a runtime, stream, event, or SDK handle.

The module guide now distinguishes:

- serial overlap-safe host `CopyBytes`;
- CUDA exact self-copy as a no-op event and partial-overlap rejection;
- provider execution state retained by an event versus user storage that the
  event does not retain;
- `Query()` versus event-local `Wait()`;
- event destruction versus operation completion;
- pageable-host copy caveats;
- resource, owner, view, context, and event lifetimes; and
- provider/native diagnostic details versus stable ASC error codes.

### Dense storage

`DenseArray::Create` remains resource-first, host-only, and
value-initializing. The new factory matches the frozen extents-first API:

```text
DenseArray::CreateUninitialized(extents, resource, layout)
```

It allocates an exact unique/exhaustive owner span without touching elements.
The guide states that reading uninitialized storage is a caller error and that
the resource must outlive the owner.

`DenseView::At` now recognizes pinned-host storage as host-accessible while
continuing to reject device and managed dereference. Serial expression
evaluation remains deliberately restricted to ordinary host storage; the
documentation does not broaden that policy merely because pinned storage is
dereferenceable.

`Clone` is documented as the synchronous named deep-copy boundary: it
allocates, enqueues explicit `CopyBytes`, waits for the returned event, and
publishes an owner only on success.

### Dense CUDA

`<asc/dense/providers/cuda.h>` remains free of CUDA/cuBLAS SDK declarations.
It exposes the move-only `DenseCudaContext`, its provider-neutral execution
context accessor, bounded `CudaEvaluate`, and exactly:

```text
CudaCopy
CudaScal
CudaAxpy
CudaGemv
CudaGemm
MatrixOperation::kNone
MatrixOperation::kTranspose
```

The guide records the exact float/double and rank bounds, device-placement
requirement, accepted one-level expression forms, external/nested expression
rejection, logical order, alias rules, and no hidden host evaluation.

Copy/Scal/Axpy are distinguished from cuBLAS Gemv/Gemm. The latter require
compatible column-major mappings and checked provider-width narrowing;
layout-right and arbitrary-stride matrices are unsupported. `beta == 0`
does not require initialized output values.

All provider operations return `Result<CompletionEvent>`. The guide requires
the Dense context, execution context, resources, owners, views/expression
nodes, and referenced bytes to remain valid until completion. It also states
that one Dense provider context is not concurrently mutable and that
independent contexts/streams may execute independently only when storage does
not race.

Dot, Nrm2, reductions, factorization, solvers, general external expression
evaluation, hidden workspace, native-handle adoption, mixed/complex
precision, batching, fast math, and later CUDA facets remain explicit
exclusions.

## Configuration and component review

Both module guides use the exact option and minimum provider version:

```text
ASC_CPP_ENABLE_CUDA=ON
CUDAToolkit 12 or newer
caller-owned CMAKE_CUDA_ARCHITECTURES
```

The examples request the exact components and targets:

```text
core_cuda  / ASC::core_cuda
dense_cuda / ASC::dense_cuda
```

The documented closures are:

```text
core_cuda:
  core
  core_cuda

dense_cuda:
  core
  expression
  dense
  core_cuda
  dense_cuda
```

The guides also state that CUDA is default-off, that enabling it builds both
facets, and that provider-free or no-component package requests do not
discover CUDA or import provider targets. Missing required toolkit/compiler/
runtime/cuBLAS inputs are configuration failures, not silent provider
disablement.

## Findings and resolutions

### Resolved: pinned-host Dense dereference

Initial review found that `DenseView::At` accepted only `MemorySpace::kHost`
although the frozen contract requires pinned-host views to remain explicitly
host-accessible. The file was initially absent from specialist ownership. The
lead assigned the bounded change to production; final review verified that
`At` now accepts host and pinned-host while rejecting device and managed
spaces. Serial expression access remains host-only.

### Resolved: uninitialized-owner parameter order

Initial production used the existing resource-first constructor convention for
`CreateUninitialized`. The frozen contract explicitly specifies
extents-first. The lead confirmed that ordering as normative, and production
changed both layout overloads to extents-first. The guide example compiles
against the corrected signature.

### Resolved: exact scalar qualification and direct includes

Review instantiated `CudaEvaluate` with a volatile destination and reproduced
a hard template-body failure caused by stripping cv-qualification before the
support decision. Production constrained the public template to exactly
unqualified `float` or `double`, matching the contract. It also added the
direct `<concepts>` and Core status includes required by the header's own
names.

### Resolved: optimized scalar-fill evaluation

An optimized Release build-tree consumer of the documented scalar
`CudaEvaluate` example initially exited with SIGSEGV in
`CudaEvaluateErased`. Production identified an optimization-sensitive
lifetime bug: a range-for over an `initializer_list` produced through a
conditional expression outlived that temporary backing array. The code now
validates left and right overlap explicitly without the temporary list.

After a fresh provider rebuild and consumer relink, the same `-O3 -DNDEBUG`
component consumer exits 0. This finding is retained because an unoptimized
guide smoke alone did not expose it.

### No remaining API defect

After those corrections, no unresolved public API, ownership, dependency, or
documentation defect was found. Unsupported external expression adapters
compile as readable expressions and return the unsupported path rather than
requiring an undocumented adapter member.

## Example and header validation

The Core guide event example and the Dense guide device-owner/scalar-fill
example were compiled as enclosing `Status`-returning functions. The Dense
example includes the provider headers plus `<asc/dense.h>` because the provider
header intentionally does not import the base Dense owner umbrella.

Commands used the equivalent of:

```sh
g++ -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only -

/usr/bin/clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only -
```

Each compiler was run once normally and once with:

```text
-fno-exceptions -fno-rtti
```

Result: pass, 8/8 example translation checks (Core and Dense under both modes
and both compilers).

Additional GCC and Clang compile traits passed for:

- noncopyable/nonmovable `CudaMemoryResource`;
- move-only `DenseCudaContext`;
- extents-first `CreateUninitialized`;
- accepted unqualified float destination;
- rejected const, volatile, and integral destinations; and
- an external readable adapter without an `operation` member compiling to the
  supported API's explicit unsupported path.

The documented source-build configuration was also exercised:

```sh
cmake -S . -B build/m6-doc-review-make \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON

cmake --build build/m6-doc-review-make \
  --target asc_core_cuda asc_dense_cuda -j2
```

Result: pass with GCC 11.4.0, CUDA compiler/toolkit 12.9.86, architecture 86,
and warnings as errors.

Separate Release build-tree package consumers then used the exact documented
`find_package(... COMPONENTS core_cuda)` / `ASC::core_cuda` and
`find_package(... COMPONENTS dense_cuda)` / `ASC::dense_cuda` pairs. The Core
consumer created a context, recorded an event, queried it, conditionally
waited, and exited 0 on the local NVIDIA GeForce RTX 3060 Laptop GPU.

The optimized Dense component consumer created a device resource, an
uninitialized 1024-element device owner, an execution/Dense context, enqueued
the documented scalar `CudaEvaluate`, waited for its event, and exited 0 after
the resolved lifetime fix.

These are documentation smoke tests, not the complete runtime/parity suite.
Installed-component consumption, relocation, complete runtime coverage, and
numerical parity remain verification/integration evidence.

## Evidence labels and remaining risks

The guides keep evidence claims independent:

```text
core_cuda:
  configure-tested
  compile-tested
  runtime-tested

dense_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested
```

This documentation review establishes none of those GPU labels by itself.
Publication Checkpoint B must attach exact toolkit/compiler, driver, device,
compute capability, architecture code, build mode, operations, layouts,
sizes, tolerances, and all skips to each claim.

Remaining user-facing risks are inherent and documented:

- asynchronous lifetimes are caller-enforced;
- a view is non-owning and an event does not retain user storage;
- destroying an event does not complete work;
- pageable-host copy submission has no host-nonblocking guarantee;
- provider layouts and expression forms are intentionally narrower than the
  provider-free CPU surface;
- native codes and diagnostic messages are not stable control-flow APIs; and
- pre-1.0 source/ABI compatibility follows the 0.6 release-line policy.

## Provenance and boundary

The documentation is original project-owned text derived from the frozen
contract, accepted ADRs, current project-owned public APIs, and the declared
CUDA/cuBLAS capability boundary. It imports no third-party sample, source,
table, literal, benchmark framework, or prose.

No Sparse CUDA, Random CUDA, Milestone 7, or Milestone 8 behavior is
documented or implied.
