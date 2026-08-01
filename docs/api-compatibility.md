# ASCCpp API and compatibility policy

Status: unreleased `0.9.0` Issue 10 Feature Gate B candidate

Date: 2026-08-01

Milestone completion, validation, publication, and release are separate
decisions. This `0.9.0` candidate is neither a release nor a claim of `1.0`
source or ABI stability.

## Version policy

- An approved pre-1.0 minor may break source or ABI and must carry migration
  documentation.
- A published patch within one pre-1.0 minor preserves that minor's documented
  public contract.
- The CMake package version file uses `SameMinorVersion`.
- Downstreams should pin and review a pre-1.0 minor.
- Published tags are immutable; fixes use a new version.

For an installed `0.9.0` package, non-`EXACT` requests for `0.9` and `0.9.0`
are compatible. A request newer than the installed patch, a different minor,
or a different major is incompatible. `EXACT 0.9.0` requires the exact
candidate version.

## Separate compatibility dimensions

| Dimension | Candidate statement |
| --- | --- |
| source | Declared public headers, names, signatures, concepts, constraints, and documented behavior form the reviewed `0.9.x` C++ surface |
| ABI | Local symbols, layouts, sizes, and alignments are observations only; there is no cross-toolchain or cross-minor ABI promise |
| symbol | Shared targets request hidden project visibility and export compiled public APIs plus intentional template-support symbols; compiler/STL weak artifacts can remain observable |
| numerical | Named traversal and validation rules are contractual; floating rounding, contraction, NaN, infinity, signed zero, and provider behavior remain within documented bounds |
| random-bit | Philox4x32-10, `Uniform01`, and storage-address mappings are exact where documented |
| provider | A claim applies only to an explicit provider target, operation subset, environment, and recorded evidence label |
| file/schema | Core supplies scalar little-endian helpers; no Dense, Sparse, random-state, checkpoint, or application file schema is defined |
| package | Version metadata, known/available components, target closures, and C++20 propagation are CMake package contracts |

One dimension never implies another. Exact random bits do not promise binary
compatibility, and package compatibility does not invent a serialization
schema.

## Supported public headers

A header is public only when it belongs to a target's public CMake file set and
is installed by that file set.

| Component | Public header set |
| --- | --- |
| `core` | `asc/core.h`; `asc/core/{configuration,contracts,execution,export,extents,io,memory,result,status,types}.h` |
| `utilities` | `asc/utilities.h`; `asc/utilities/{command_line,export,timer}.h` |
| `expression` | `asc/expression.h`; `asc/expression/{expression,writable}.h` |
| `dense` | `asc/dense.h`; `asc/dense/{array,blas,evaluate,export,layout,view}.h` |
| `sparse` | `asc/sparse.h`; `asc/sparse/{blas,compressed,coordinate,evaluate,export}.h` |
| `random` | `asc/random.h`; `asc/random/{distribution,engine,export}.h` |
| `random_dense` | `asc/random/dense.h` |
| `random_sparse` | `asc/random/sparse.h` |
| `core_cuda` | `asc/core/providers/{cuda,cuda_export}.h` |
| `dense_cuda` | `asc/dense/providers/{cuda,cuda_export}.h` |
| `sparse_cuda` | `asc/sparse/providers/{cuda,cuda_export}.h` |
| `random_cuda` | `asc/random/providers/{cuda,cuda_export}.h` |
| `random_dense_cuda` | `asc/random/providers/{dense_cuda,dense_cuda_export}.h` |
| `random_sparse_cuda` | `asc/random/providers/{sparse_cuda,sparse_cuda_export}.h` |
| `cpp` | no additional header; it is a provider-free aggregate target |

Provider-neutral umbrellas do not include provider headers. Deleted
`asc/array*`, `asc/linalg*`, and `asc/cpp.h` paths are not compatibility
aliases.

The owner-scoped `asc/dense/linalg.h` and `asc/sparse/linalg.h` paths and the
`DenseTranspose` name were also removed by the approved breaking pre-1.0
migration. See the [mechanical migration guide](migration/linalg-to-blas.md).

Names in an `internal_` namespace are implementation details even when a
public template header must declare them or a shared library must export a
support symbol. They are not downstream extension points.

The complete CUDA-enabled surface has 49 headers. A CUDA-disabled install
contains the exact 37 provider-free headers; provider headers are not installed
as unusable stubs.

## Source contract

Public headers require C++20 and are intended to parse as first includes.
Consumers must not depend on transitive provider includes, implementation
names, generated targets files, compiler extensions, or undeclared link
edges.

Recoverable failures use `Status` and `Result<T>`. `ErrorCode` is the
machine-readable category; diagnostic message and provider text are not
stable. Reading a failed `Result` or violating an active `ASC_CHECK` is a fatal
programmer error, not an exception channel.

Template constraints, enum values, compile-time rank, exact scalar
requirements, layouts/formats, ownership, alias, memory-space, and execution
rules are part of the source contract. Unsupported cases fail constraints or
return an explicit status; they do not silently convert, transfer, pack,
allocate, synchronize, or fall back.

Issue 9 adds exact checked-descriptor overloads for all classic Dense BLAS
Level 3 families and the `DenseBlasSide` enum. The pre-existing ordinary-view
float/double `Gemm` overloads remain source compatible and keep their original
bounded layout contract. No alias, forwarding header, compatibility target,
or runtime fallback is introduced.

Issue 10 extends the existing Sparse `blas.h` surface with checked S/D/C/Z
indexed-vector, CSR/CSC matrix, and triangular descriptors and operations. The
pre-existing expression-adapted float/double CSR `Spmv(alpha, A, x, beta, y)`
overload remains source compatible; the standardized accumulating overload is
selected only by its explicit transpose and descriptor arguments. Existing
Sparse names are not renamed, and no forwarding header or runtime fallback is
introduced.

`Timer::Stop` checks interval, accumulated-duration, and sample-count
arithmetic before publishing a completed sample. Overflow returns
`ErrorCode::kOverflow` and leaves the timer running with its previously
completed statistics unchanged. An observed backward steady clock returns
`ErrorCode::kInvalidState` with the same transactional behavior.
`Timer::Average` likewise rejects a sample count that cannot be represented by
the clock duration. Because the existing `Timer::Elapsed` signature has no
status channel, an unrepresentable live elapsed total or observed clock
regression saturates at `Timer::Duration::max()` instead of wrapping.

## ABI and symbol boundary

Compiled shared targets request hidden visibility for project implementation
symbols. Export macros select the public compiled surface and become empty
through the corresponding `ASC_*_STATIC_DEFINE` usage requirement for static
consumers. Public templates also require deliberately exported support
functions. Local dynamic symbol tables can contain compiler or standard-library
weak/template artifacts despite the visibility policy; those names are
observations, not supported ASCCpp APIs.

Always link the imported target. Manually naming a library file bypasses
compile definitions, C++20 propagation, and transitive link requirements.

Local ABI reports may contain compiler-specific mangled names, symbol
versions, sizes, and alignments. They do not promise interchangeability
between:

- ASCCpp `0.x` minors;
- GCC and Clang or their versions;
- libstdc++ and libc++;
- Debug, Release, sanitizer, or other compiler-option selections;
- static and shared builds;
- standard-library ABI selections;
- CUDA toolkit/host compiler pairs; or
- operating systems and architectures.

Current local ELF observations have unversioned SONAMEs such as
`libasc_core.so`; the candidate defines no product `SOVERSION` or cross-minor
side-by-side ABI contract. A baseline under [`abi/`](../abi) is enforced only
when the complete recorded environment selector matches, including operating
system/distribution and architecture, compiler identity and full version,
standard library, configuration, linkage, inspection-tool identities, and
applicable CUDA compiler, host compiler, and architecture. Any other
environment produces a non-enforcing observation or explicit skip. A broadly
similar environment never inherits a baseline pass. These records are
environment-specific evidence, not compatibility promises.

## Ownership, capacity, and lifetime

- `Buffer`, `File`, Dense/Sparse owners, completion events, CUDA provider
  contexts, and generation result owners are move-only where double ownership
  would be unsafe.
- A `MemoryResource` is borrowed and must outlive every allocation made through
  it.
- Dense, coordinate, compressed, byte, and CUDA strided views are non-owning.
  Copying a descriptor never extends storage lifetime.
- Expression nodes borrow lvalue operands and own rvalue/value operands.
  Capturing a view by value still does not own its storage.
- Resize/reallocation and Sparse structural replacement invalidate affected
  views.
- Sparse evaluation and SpMV treat every values, coordinate, offset, and
  index span read by a terminal as conservative alias input. Overlap with a
  writable destination is handled transactionally even when values storage
  itself is disjoint.
- A CUDA completion event retains provider execution/completion state, not user
  arrays, views, caller workspace, memory resources, or higher-level
  Dense/Sparse provider contexts. The documented Core context, provider
  contexts, and all caller-owned objects remain alive until completion.
- If completion-event creation or recording fails after a CUDA copy, project
  kernel, or cuBLAS operation may have enqueued work, ASCCpp drains the
  affected stream before returning and preserves the original provider
  failure. A cleanup synchronization error does not replace that original
  status. This failure recovery prevents background access after a failed
  operation returns; it is not a successful-operation synchronization or a
  license to release storage before a returned successful event completes.
- Trusted device Sparse provenance states how a structure was created; it does
  not prove asynchronous production has completed.

The approved raw CUDA Random API is:

```cpp
CudaFillPhilox4x32(const ExecutionContext&, MutableMemoryView destination,
                   std::uint64_t word_count, RandomStream,
                   RandomSubsequence, RandomOffset);
```

There is no bare-pointer overload. The operation validates
`word_count * sizeof(std::uint32_t)` against `destination.size()`.
`MutableMemoryView` capacity and placement are caller declarations and must
truthfully describe live storage. CUDA pointer attributes can validate
placement/device for nonempty storage, but they do not establish the terminal
bound of every arbitrary external allocation.

Changing an ownership, capacity, invalidation, or completion rule can change
source behavior even when a signature is unchanged, so those rules belong to
the documented patch contract.

## Numerical and random compatibility

Serial Dense reductions/algebra and serial CSR SpMV use documented traversal
orders but do not promise cross-toolchain floating bit identity. CUDA algebra
and SpMV claims are bounded by their named algorithms, types, layouts, and
evidence environment. A tolerance-based parity result states its oracle and
tolerance; it is not a universal stability guarantee.

Serial `ReduceSum` checks every addition for integral element types. Signed or
unsigned overflow returns `ErrorCode::kOverflow`; no wrapped reduction value
is published. Floating reduction order and behavior remain governed by the
existing floating numerical contract rather than integral overflow checks.

The Random contract is narrower and exact:

- Philox4x32-10 round constants, lane order, key/counter mapping, and
  stream/subsequence/offset addressing are fixed;
- `Uniform01<float>` consumes the documented 24 significant word bits;
- `Uniform01<double>` consumes the documented 53-bit high/low pair; and
- Dense and Sparse facets use their documented logical/canonical address
  mappings.

A future incompatible sequence requires a separately versioned algorithm or
an approved breaking version. Native object layouts are not serialized random
state.

## Provider and downstream upgrade policy

`ASC::cpp` is provider-free. A provider is used only through its explicit
component/target and explicit execution objects. There is no provider registry,
native-handle adoption, automatic backend, or silent CPU fallback.

Before changing an ASCCpp pre-1.0 minor, a downstream should:

1. read the changelog, migration notes, support matrix, and remaining risks;
2. request only explicit package components;
3. rebuild every translation unit and shared-library boundary;
4. rerun numerical, random-bit, and enabled-provider tests;
5. rerun installed and relocated consumers; and
6. regenerate ABI/performance observations in the same environment.

See [package capabilities](package-capabilities.md), [downstream
integration](downstream-integration.md), and [extension
guidance](extension-guide.md).
