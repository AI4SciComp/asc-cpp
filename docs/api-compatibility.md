# ASCCpp API and compatibility policy

Status: unreleased `0.9.0` Milestone 8 candidate

Date: 2026-07-27

ASCCpp is pre-1.0. Milestone completion, local validation, publication, and a
release are separate decisions. The `0.9.0` candidate is neither a release nor
a claim that the surface has reached `1.0` stability.

## Version policy

- A `0.x` minor may make source or ABI changes when the change is approved and
  accompanied by migration documentation.
- A published patch within one `0.x` minor preserves that minor's documented
  public contract.
- The CMake package version file uses `SameMinorVersion`.
- Downstreams should pin the pre-1.0 minor they have reviewed.
- No published tag may be moved. A released defect is corrected in a new
  version.

For an installed `0.9.0` package, a non-`EXACT` request for `0.9` or `0.9.0`
is compatible. A request newer than the installed package, another pre-1.0
minor such as `0.8` or `0.10`, or another major is not compatible. `EXACT`
requires the exact installed version.

## Separate compatibility dimensions

| Dimension | Candidate statement |
| --- | --- |
| source | Public headers and documented names/signatures are the supported C++ surface for the reviewed `0.9.x` minor |
| ABI | Local symbol and layout observations are hardening evidence only; there is no cross-toolchain or cross-`0.x`-minor ABI promise |
| symbol | Shared builds hide implementation symbols by default and export the public compiled surface plus intentional support symbols required by public templates |
| numerical | Named serial traversal and validation rules are contractual; native floating rounding, NaN, infinity, signed zero, contraction, and provider algorithms remain within their documented boundaries |
| random-bit | The named Philox4x32-10 lane/key/counter mapping and Uniform01 bit transforms are exact contracts; storage facets add their documented logical-order mappings |
| provider | A provider claim applies only to its explicit target, operation, scalar/layout subset, and recorded evidence environment |
| file/schema | Core defines portable little-endian scalar helpers but no dense, sparse, random-state, checkpoint, or application file format |
| package | Known/available components, imported targets, closure, C++20 propagation, and `SameMinorVersion` behavior are package contracts |

One dimension never implies another. Bit-identical Philox output does not
promise ABI compatibility; an ABI report does not promise identical floating
provider results; package compatibility does not invent a file schema.

## Supported public surface

A header is public only when it is declared in an ASCCpp target's public CMake
file set and installed by that file set. The provider-neutral umbrellas do not
include provider headers.

| Component | Public headers |
| --- | --- |
| `core` | `asc/core.h`, `asc/core/configuration.h`, `asc/core/contracts.h`, `asc/core/execution.h`, `asc/core/export.h`, `asc/core/extents.h`, `asc/core/io.h`, `asc/core/memory.h`, `asc/core/result.h`, `asc/core/status.h`, `asc/core/types.h` |
| `utilities` | `asc/utilities.h`, `asc/utilities/command_line.h`, `asc/utilities/export.h`, `asc/utilities/timer.h` |
| `expression` | `asc/expression.h`, `asc/expression/expression.h`, `asc/expression/writable.h` |
| `dense` | `asc/dense.h`, `asc/dense/array.h`, `asc/dense/evaluate.h`, `asc/dense/export.h`, `asc/dense/layout.h`, `asc/dense/linalg.h`, `asc/dense/view.h` |
| `sparse` | `asc/sparse.h`, `asc/sparse/compressed.h`, `asc/sparse/coordinate.h`, `asc/sparse/evaluate.h`, `asc/sparse/export.h`, `asc/sparse/linalg.h` |
| `random` | `asc/random.h`, `asc/random/distribution.h`, `asc/random/engine.h`, `asc/random/export.h` |
| `random_dense` | `asc/random/dense.h` |
| `random_sparse` | `asc/random/sparse.h` |
| `core_cuda` | `asc/core/providers/cuda.h`, `asc/core/providers/cuda_export.h` |
| `dense_cuda` | `asc/dense/providers/cuda.h`, `asc/dense/providers/cuda_export.h` |
| `sparse_cuda` | `asc/sparse/providers/cuda.h`, `asc/sparse/providers/cuda_export.h` |
| `random_cuda` | `asc/random/providers/cuda.h`, `asc/random/providers/cuda_export.h` |
| `random_dense_cuda` | `asc/random/providers/dense_cuda.h`, `asc/random/providers/dense_cuda_export.h` |
| `random_sparse_cuda` | `asc/random/providers/sparse_cuda.h`, `asc/random/providers/sparse_cuda_export.h` |
| `cpp` | no additional header; it is a provider-free aggregate target |

Deleted historical `asc/array*`, `asc/linalg*`, and `asc/cpp.h` headers are not
compatibility aliases. The supported aggregate include is not recreated as a
legacy facade; downstreams include the module headers they use and link the
matching targets.

Names containing an `internal_` namespace remain implementation details even
when their declarations must appear in a public template header or their
support symbols must be visible from a shared library. They are not extension
points, and source or ABI compatibility does not cover direct downstream use
of them.

## Source contract

Public headers are C++20, self-contained, and intended to parse independently.
Consumers must not depend on include order, transitive inclusion of a provider
header, a compiler extension, an undeclared direct dependency, or an
implementation namespace.

The public API uses `Status` and `Result<T>` for recoverable errors. Diagnostic
message and provider text are not stable; `ErrorCode` categories are the
machine-readable contract. Accessing a failed result's value and violating an
`ASC_CHECK` contract are fatal programmer errors, not a public exception
channel.

Template constraints, enum values, compile-time rank, exact scalar
requirements, layout/format restrictions, and ownership rules are part of the
source contract. A call that is intentionally unavailable for a type should
fail constraints or return the documented status rather than select an
implicit conversion, provider, or fallback.

## ABI and symbol boundary

Compiled shared targets use hidden visibility by default. Public export macros
select import/export behavior for shared builds and become empty through the
matching `ASC_*_STATIC_DEFINE` usage requirement for static consumers.
Downstreams should link imported targets so these definitions and C++20
requirements propagate correctly; manually naming library files bypasses that
contract.

The local ABI report may record exported symbols, object sizes, alignments,
target kinds, output names, and compiler/linker identities. Those observations
do not promise binary interchangeability across:

- different ASCCpp `0.x` minors;
- GCC and Clang, or different compiler versions;
- libstdc++ and libc++;
- Debug, Release, sanitizer, or compiler-option combinations;
- static and shared builds;
- `_GLIBCXX_USE_CXX11_ABI` or another standard-library ABI selection;
- CUDA toolkit/host compiler combinations; or
- operating systems and architectures.

Public templates can instantiate code in a downstream translation unit. Their
implementation may call intentionally exported support functions. The
presence of such a symbol does not make its `internal_` C++ name a supported
source-level API.

## Ownership and lifetime compatibility

Ownership is explicit:

- `Buffer`, `File`, dense/sparse owners, completion events, and CUDA provider
  contexts are move-only where double ownership would be unsafe.
- A `MemoryResource` is not owned by buffers or numerical owners and must
  outlive every allocation made through it.
- Dense, coordinate, compressed, byte, and CUDA strided views are non-owning.
  Copying a descriptor never extends storage lifetime.
- Expression nodes borrow lvalue operands and own rvalue/value operands.
  Capturing a view by value still does not own its storage.
- A CUDA completion event retains provider completion state, not arrays,
  views, workspaces, contexts, or memory resources. All referenced storage and
  required provider objects remain alive until completion.
- An asynchronous producer's trusted sparse provenance proves how structure
  was created; it does not prove that production has completed.

Changing these rules can change source behavior even if a function signature
does not change, so they belong to the documented patch contract.

## Numerical and random compatibility

Serial dense reductions and algebra and serial CSR SpMV use their documented
logical traversal orders. They do not promise cross-toolchain bit equality
for native floating arithmetic. CUDA algebra and SpMV are bounded by their
named provider path and evidence environment. A tolerance-based parity result
must state its oracle and tolerance; it is not a universal numerical-stability
guarantee.

The random base has a stronger bit contract:

- Philox4x32-10 round count, multipliers, Weyl constants, lane order, and
  stream/subsequence/offset mapping are exact;
- float Uniform01 consumes the most-significant 24 bits of one word;
- double Uniform01 consumes the most-significant 53 bits of a high/low word
  pair; and
- dense and sparse facets use their documented logical/canonical word
  addressing.

A future incompatible sequence requires a separately versioned algorithm or
an explicitly approved breaking version. Native object layout of random
structs is still not a serialization format.

## Provider compatibility

`ASC::cpp` is provider-free. A provider is used only by linking its explicit
facet and constructing its explicit context/resource where applicable. There
is no automatic backend selection, native-handle adoption, provider registry,
or silent CPU fallback.

GPU evidence uses exactly `configure-tested`, `compile-tested`,
`runtime-tested`, `parity-tested`, or `skipped`. See the [support
matrix](support-matrix.md) for their meanings. Evidence for one toolkit,
device, operation, scalar, layout, or topology is not generalized to another.

## Downstream review rule

Before updating an ASCCpp pre-1.0 minor, a downstream should:

1. read the changelog and migration notes;
2. request only its explicit package components;
3. rebuild every translation unit and shared-library boundary;
4. rerun its numerical/random/provider compatibility tests;
5. rerun installed and relocated package consumers; and
6. regenerate any local ABI observation instead of comparing binaries from
   unlike environments.

See [downstream integration](downstream-integration.md) for the package
workflow and [extension guidance](extension-guide.md) for supported
customization points.
