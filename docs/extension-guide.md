# Extending ASCCpp safely

Status: unreleased `0.9.0` Milestone 8 candidate

Date: 2026-07-27

ASCCpp has a small set of explicit downstream extension points. A public type
being constructible or inheritable does not make every implementation detail
an extension point. Extensions remain ordinary downstream code and do not
become ASCCpp package components or providers.

## Supported extension points

| Extension | Public contract |
| --- | --- |
| expression reader | specialize `asc::ExpressionAdapter<T>` for a downstream type |
| expression placement | specialize `asc::ExpressionPlacementAdapter<T>` independently |
| writable expression | specialize `asc::WritableExpressionAdapter<T>` only for a total, unique, nonfailing logical destination |
| memory resource | derive from `asc::MemoryResource` and implement one truthful memory space |
| byte source/sink | derive from `asc::ByteSource` or `asc::ByteSink` and implement partial-progress semantics |
| downstream build wrapper | define a downstream-owned target that links one or more imported `ASC::` targets |

Direct use of an `internal_` namespace, private constructor/factory, generated
targets file, private symbol, or source-tree-only header is unsupported.

## Expression adapters

Specialize adapters in `namespace asc` for a downstream-owned unqualified
type. Do not add declarations to ASCCpp headers.

A readable adapter supplies:

```cpp
template <>
struct asc::ExpressionAdapter<MyVector> {
  using value_type = double;
  static constexpr asc::rank_t kRank = 1;
  static constexpr asc::ExpressionOperationCategory kOperationCategory =
      asc::ExpressionOperationCategory::kTerminal;
  static constexpr asc::SparsityEffect kSparsityEffect =
      asc::SparsityEffect::kStructurePreserving;

  static std::array<asc::extent_t, 1> Shape(const MyVector&);
  static double Read(const MyVector&,
                     std::span<const asc::index_t, 1>);
  static bool MayAlias(const MyVector&, asc::AliasToken);
};
```

The adapter is responsible for valid indexing, truthful shape, operand
lifetime, scalar behavior, and conservative aliasing. `Read` is called only
with an in-shape coordinate by a conforming evaluator, but it has no status
return and must honor its declared contract.

An adapter over byte-addressable storage should validate and retain a complete
physical-span token with `AliasToken::FromAddressSpan`, then answer alias
queries with `AliasTokensMayOverlap`. A start-address identity alone is not
enough for arbitrary subspans or strided ranges. If one token cannot describe
all regions, retain enough metadata to return `true` conservatively.

### Placement

Placement is an independent specialization:

```cpp
template <>
struct asc::ExpressionPlacementAdapter<MyVector> {
  static asc::MemorySpace Space(const MyVector&);
};
```

The value must describe actual placement. It does not authorize transfer,
host dereference, migration, or provider selection. Serial sparse algebra
requires explicit host placement. A generic serial dense expression adapter
must make its `Read` operation safe for the evaluator's direct host call.

### Writable destinations

A writable adapter supplies the same value type/rank, shape, alias token, and
a total write:

```cpp
template <>
struct asc::WritableExpressionAdapter<MyVector> {
  using value_type = double;
  static constexpr asc::rank_t kRank = 1;

  static std::array<asc::extent_t, 1> Shape(const MyVector&);
  static asc::AliasToken Alias(const MyVector&);
  static void Write(MyVector&, std::span<const asc::index_t, 1>, double);
};
```

`Write` is invoked only after the owning algorithm validates the complete
operation. It must:

- name one unique writable scalar for every in-shape coordinate;
- perform no allocation, transfer, synchronization, provider selection, or
  failure;
- agree with the readable adapter's value type, rank, shape, and indexing; and
- remain valid for the complete synchronous operation.

Do not advertise unrestricted writable participation for a sparse destination
that omits logical coordinates. The built-in coordinate SpMV destination is
accepted only when it stores every vector coordinate; an external adapter
makes that total-coverage promise itself.

Generic protocol participation does not imply support by every evaluator.
The CUDA dense and sparse evaluators intentionally accept bounded built-in
terminal/node sets and reject arbitrary external adapters before enqueue.

## Memory resources

A custom `MemoryResource` owns no allocations on behalf of ASCCpp; buffers
borrow the resource object. The implementation must:

- return one stable, truthful `MemorySpace`;
- honor requested byte count and alignment or return a failed `Result`;
- make zero-byte behavior consistent with the public resource contract;
- deallocate with the exact corresponding allocator family;
- make `Deallocate` nonthrowing; and
- remain alive until every buffer/owner allocated through it has released its
  storage.

The resource implementation also owns its concurrency policy. A caller must
serialize an instance that is not safe for concurrent allocation/deallocation.

Declaring `kDevice` or `kManaged` does not register a new execution provider.
CUDA operations still validate the actual CUDA pointer/device using the
approved Runtime provider. Exact allocation-subspan validation is available
for `CudaMemoryResource` allocations through internal registration; for an
arbitrary external resource, the declared span remains the caller's
allocation-bound contract.

## Byte sources and sinks

`ByteSource::ReadSome` and `ByteSink::WriteSome` may make partial progress.
Implementations must:

- treat a zero-sized request as success without touching its address;
- return zero from `ReadSome` only for end of stream;
- not use successful zero-byte `WriteSome` as a temporary would-block result;
- report operational errors with `Status`/`Result`; and
- keep any borrowed external handle or storage alive for the call.

`ReadExact` and `WriteAll` build on those rules. They cannot roll back bytes
already read into or accepted from caller storage.

## Downstream wrapper targets

A project may create a target that expresses its own capability:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense sparse)

add_library(my_numerics)
target_sources(my_numerics PRIVATE my_numerics.cc)
target_link_libraries(my_numerics PUBLIC ASC::dense ASC::sparse)
```

Use a downstream namespace for aliases and exports. Do not create new
`ASC::...` targets, rewrite imported target properties, include ASCCpp's
generated targets files directly, or install an altered ASCCpp config package.

An ASCCpp package component is an architecture-controlled product facet. A
downstream wrapper is not added to `ASCCpp_KNOWN_COMPONENTS`, and a custom
CUDA kernel is not an `ASC::` provider merely because it consumes an ASCCpp
view.

## Ownership and asynchronous extensions

Non-owning descriptors never extend storage lifetime. A downstream adapter or
wrapper that stores a dense/sparse/byte view must document which owner keeps
the allocation alive and what invalidates the descriptor.

Expression nodes borrow lvalues and own rvalues. If an adapter allows shape or
storage mutation, the application must not mutate those properties while a
borrowed expression node is in use.

For CUDA work, an ASCCpp completion event owns provider completion state only.
The downstream must retain:

- every operand and destination allocation;
- all memory resources required for their eventual deallocation;
- caller workspace;
- the applicable dense/sparse provider context; and
- any external state used by the operation

until the event reports or waits to completion. Destroying an incomplete event
does not synchronize and does not make storage release safe.

## Binary boundary

`MemoryResource`, `ByteSource`, and `ByteSink` are polymorphic C++ extension
points. Crossing a shared-library boundary with a downstream-derived object
requires a mutually compatible compiler, standard library, runtime library,
build mode, and ASCCpp binary. The pre-1.0 package makes no cross-toolchain or
cross-minor ABI promise. Prefer rebuilding both sides from the same reviewed
package environment.

## What requires a new approved milestone

The following are not downstream extension mechanisms for changing ASCCpp:

- a new installed component or `ASC::` provider;
- a native stream/handle adoption API or provider registry;
- hidden provider dispatch or fallback;
- new numerical operation, scalar family, storage format, runtime-rank owner,
  broadcasting rule, or compatibility facade;
- DLPack, distributed, automatic-differentiation, OpenMP, Eigen, BLAS/LAPACK,
  oneMKL, TBB, SYCL, HIP, or CUDA Driver integration; or
- a new serialized state or file schema.

Such work requires architecture approval, an exact dependency/capability
contract, tests, documentation, provenance, and provider evidence. It must not
be smuggled in through an implementation namespace or package overlay.
