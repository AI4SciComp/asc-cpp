# Extending ASCCpp safely

Status: `0.9.0` extension contract

Date: 2026-07-28

ASCCpp has a small set of explicit downstream extension points. A public type
being constructible, templated, or inheritable does not make every detail an
extension mechanism. Downstream extensions do not become ASCCpp components or
providers.

## Supported extension points

| Extension | Public contract |
| --- | --- |
| readable expression | specialize `asc::ExpressionAdapter<T>` for a downstream-owned type |
| expression placement | specialize `asc::ExpressionPlacementAdapter<T>` independently |
| writable expression | specialize `asc::WritableExpressionAdapter<T>` for a total, unique, nonfailing destination |
| memory resource | derive from `asc::MemoryResource` and implement one truthful memory space |
| byte source/sink | derive from `asc::ByteSource` or `asc::ByteSink` and implement synchronous partial-progress semantics |
| build wrapper | define a downstream-owned target that links imported `ASC::` targets |

Direct use of an `internal_` namespace, private constructor, generated targets
file, private symbol, or source-tree-only header is unsupported.

## Expression adapters

Specialize adapters in `namespace asc` for a downstream-owned unqualified
type. The following complete skeleton adapts a non-owning host vector:

```cpp
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <asc/expression.h>

struct DownstreamVector {
  // Construction guarantees that values.size() fits asc::extent_t.
  std::span<double> values;
};

namespace asc {

template <>
struct ExpressionAdapter<DownstreamVector> {
  using value_type = double;
  static constexpr rank_t rank = 1;
  static constexpr SparsityEffect sparsity_effect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(const DownstreamVector& vector) {
    return {static_cast<extent_t>(vector.values.size())};
  }

  static double Read(const DownstreamVector& vector,
                     std::span<const index_t, 1> indices) {
    return vector.values[static_cast<std::size_t>(indices[0])];
  }

  static bool MayAlias(const DownstreamVector& vector,
                       AliasToken token) noexcept {
    if (token.identity() == &vector) {
      return true;
    }
    if (token.identity() == nullptr || vector.values.empty()) {
      return false;
    }
    const auto begin =
        reinterpret_cast<std::uintptr_t>(vector.values.data());
    const auto bytes = vector.values.size_bytes();
    if (bytes > std::numeric_limits<std::uintptr_t>::max() - begin) {
      return true;
    }
    const auto address =
        reinterpret_cast<std::uintptr_t>(token.identity());
    return begin <= address && address < begin + bytes;
  }
};

template <>
struct ExpressionPlacementAdapter<DownstreamVector> {
  static MemorySpace Space(const DownstreamVector&) {
    return MemorySpace::kHost;
  }

  static ExpressionAliasMetadata Alias(const DownstreamVector& vector) {
    return ExpressionAliasMetadata(&vector, vector.values.data(),
                                   vector.values.size_bytes());
  }
};

template <>
struct WritableExpressionAdapter<DownstreamVector> {
  static std::array<extent_t, 1> Shape(const DownstreamVector& vector) {
    return ExpressionAdapter<DownstreamVector>::Shape(vector);
  }

  static ExpressionAliasMetadata Alias(const DownstreamVector& vector) {
    return ExpressionPlacementAdapter<DownstreamVector>::Alias(vector);
  }

  static bool IsUnique(const DownstreamVector&) { return true; }

  static void Write(DownstreamVector& vector,
                    std::span<const index_t, 1> indices, double value) {
    vector.values[static_cast<std::size_t>(indices[0])] = value;
  }
};

}  // namespace asc
```

The adapter owns these promises:

- `Shape`, `Read`, and `Write` use the same logical indexing;
- reads and writes are valid for every in-shape coordinate;
- placement and alias metadata describe the actual live storage;
- `MayAlias` is conservative for any address that can overlap the operand;
- `IsUnique` is true only when each logical coordinate names one writable
  scalar;
- `Write` cannot fail and performs no allocation, transfer, synchronization,
  or provider selection; and
- the vector and its underlying storage outlive every borrowed expression use.

`ExpressionAdapter` may additionally provide
`ValidateAccess(const T&, const ExecutionContext&) -> Status`. Placement does
not authorize host dereference or transfer by itself.

An external adapter should normally omit the optional `operation` member so
`ExpressionOperationCategory` reports `kExternal`. Categories such as
`kTerminal`, `kNegate`, and `kAdd` describe ASCCpp's recognized built-in
storage and node shapes; assigning one does not turn an external type into
that built-in shape.

Generic protocol participation does not imply support by every evaluator. The
CUDA Dense and Sparse evaluators deliberately accept bounded built-in
terminal/node sets and reject arbitrary external expression adapters.

## Memory resources

A `MemoryResource` object is borrowed by every `Buffer` and numerical owner
allocated through it. A custom implementation must:

- return one stable and truthful `MemorySpace`;
- honor the requested byte count and power-of-two alignment or fail with
  `Result`;
- return `nullptr` for a successful zero-byte allocation;
- return a non-null, sufficiently aligned pointer for a successful nonzero
  allocation;
- deallocate through the corresponding allocator family without throwing; and
- remain alive until all dependent owners release their allocations.

The resource implementation owns its concurrency policy. Callers must
serialize a resource instance that is not safe for concurrent allocation and
deallocation.

Declaring `kDevice` or `kManaged` does not register a provider or make the
storage CUDA-compatible. CUDA operations validate available pointer attributes
and device placement, but CUDA Runtime does not provide a general terminal
allocation-bound query. A `MutableMemoryView` byte capacity, Dense/Sparse view
span, and any external allocation extent are truthful caller contracts.

## Byte sources and sinks

`ByteSource::ReadSome` and `ByteSink::WriteSome` are synchronous and may make
partial progress. Implementations must:

- treat a zero-sized request as success without touching its address;
- return zero from `ReadSome` only at end of stream;
- not use successful zero-byte `WriteSome` as a would-block result;
- report operational errors with `Status`/`Result`; and
- keep borrowed handles and storage alive for the call.

`ReadExact` and `WriteAll` build on those rules. They cannot roll back bytes
already read into or accepted from caller storage.

## Downstream wrapper targets

A downstream can combine components without changing ASCCpp:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense sparse)

add_library(my_numerics)
target_sources(my_numerics PRIVATE my_numerics.cc)
target_link_libraries(my_numerics PUBLIC ASC::dense ASC::sparse)
```

Use a downstream namespace for aliases and exports. Do not create an `ASC::`
target, modify imported target properties, include a generated ASCCpp targets
file directly, or install an altered ASCCpp config.

## Ownership and asynchronous work

A wrapper storing a Dense, Sparse, byte, or CUDA strided view must document the
owner and every invalidating operation. Expression nodes borrow lvalues and
own rvalues; an lvalue adapter's shape/storage must not change while a borrowed
node is in use.

For CUDA work, retain until the returned event completes:

- the Core execution context;
- every operand and destination allocation;
- memory resources needed for eventual deallocation;
- caller workspace;
- the applicable Dense/Sparse provider context; and
- external state referenced by the operation.

Destroying an incomplete event does not synchronize and does not make early
storage release safe. A trusted Sparse view is a provenance statement, not a
completion primitive.

If completion-event creation or recording fails after a CUDA copy, project
kernel, or cuBLAS operation may have enqueued work, ASCCpp drains that
operation's affected stream before returning and preserves the original
provider error even if cleanup synchronization also fails. This failure-path
cleanup does not change the successful asynchronous lifetime contract above.

`MemoryResource`, `ByteSource`, and `ByteSink` are polymorphic C++ interfaces.
Passing a downstream-derived object across a shared-library boundary requires
a mutually compatible compiler, standard library/runtime, build mode, and
ASCCpp binary. The pre-1.0 package makes no cross-toolchain or cross-minor ABI
promise.

## Changes that require architecture approval

These are not downstream extension mechanisms:

- a new installed component, `ASC::` provider, or provider registry;
- native stream/handle adoption, hidden dispatch, or fallback;
- a new numerical operation, scalar family, storage format, runtime-rank
  owner, broadcasting rule, or compatibility facade;
- DLPack, distributed, automatic-differentiation, OpenMP, Eigen, BLAS/LAPACK,
  oneMKL, TBB, SYCL, HIP, or CUDA Driver integration; or
- a new serialized state or file schema.

Such work needs a bounded approved milestone, direct-dependency and capability
contracts, tests, documentation, provenance, and provider evidence.
