# Expression module

`ASC::expression` is a storage-neutral, provider-free C++20 expression
protocol. It is an interface library with one direct ASC dependency,
`ASC::core`; it has no compiled object and no external dependency.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS expression)
target_link_libraries(my_target PRIVATE ASC::expression)
```

```cpp
#include <asc/expression.h>
```

The umbrella includes `<asc/expression/expression.h>` for readable expressions
and pointwise nodes and `<asc/expression/writable.h>` for storage-neutral
placement, alias-span, and writable protocols. All Expression implementation is
header-only.

## Non-intrusive participation

`ExpressionAdapter<T>` is the single customization point. An external type
participates through an explicit specialization; it does not inherit from ASC
and does not include a Dense, Sparse, Random, or provider header.

The `ReadableExpression` protocol requires the adapter to expose:

- a scalar `value_type`;
- a compile-time `rank`;
- a complete shape using Core `extent_t`;
- indexed scalar reads using Core `index_t`;
- a conservative alias query; and
- a `SparsityEffect`.

Runtime rank vocabulary uses Core `rank_t`. `ReadableExpression` describes
readable values only; the separate writable protocol is opt-in. Neither
protocol transfers ownership. An adapter whose reads depend on memory
accessibility may additionally expose `ValidateAccess`.
`ValidateExpressionAccess` invokes that optional hook for a terminal and
recursively for built-in expression nodes. An adapter without the hook remains
context-independent. This validates access only; it does not select a provider,
transfer data, or evaluate an expression.

A minimal rank-one external adapter has this shape:

```cpp
struct Pair {
  std::array<double, 2> values;
};

template <>
struct asc::ExpressionAdapter<Pair> {
  using value_type = double;
  static constexpr asc::rank_t rank = 1;
  static constexpr asc::SparsityEffect sparsity_effect =
      asc::SparsityEffect::kStructurePreserving;

  static constexpr std::array<asc::extent_t, 1> Shape(
      const Pair&) noexcept {
    return {2};
  }

  static constexpr double Read(
      const Pair& pair,
      std::span<const asc::index_t, 1> indices) noexcept {
    return pair.values[static_cast<std::size_t>(indices[0])];
  }

  static constexpr bool MayAlias(
      const Pair& pair, asc::AliasToken token) noexcept {
    return token.identity() == &pair;
  }
};
```

The specialization owns the validity contract for its shape and indices. ASC
does not bounds-check an external adapter's scalar read.

## Placement and writable participation

`ExpressionPlacementAdapter<T>` is an independent opt-in customization point
for a readable expression. It supplies:

- `Space(const T&)`, returning the expression's `MemorySpace`; and
- `Alias(const T&)`, returning conservative `ExpressionAliasMetadata`.

A readable type with that adapter satisfies `PlacedReadableExpression`.
`ExpressionSpace` and `ExpressionAlias` expose the two properties.

`WritableExpressionAdapter<T>` is another independent specialization. Its
`Shape`, `Alias`, `IsUnique`, and `Write` operations describe mutation of an
existing placed readable object. A participating type satisfies
`WritableExpression`; the corresponding public operations are
`WritableExpressionShape`, `WritableExpressionAlias`,
`WritableExpressionIsUnique`, and `WriteExpression`. `IsUnique` states whether
distinct logical coordinates map to distinct destination elements. `Write`
receives an exact-rank index span and an `ExpressionValue<T>`.

A writable adapter may optionally provide
`ValidateAccess(const T&, const ExecutionContext&)`. The public
`ValidateWritableExpressionAccess` calls it when present and otherwise falls
back to `ValidateExpressionAccess`. Access validation never transfers storage
or selects an execution provider.

The external `Pair` example can opt into both protocols without making ASC an
owner:

```cpp
template <>
struct asc::ExpressionPlacementAdapter<Pair> {
  static constexpr asc::MemorySpace Space(const Pair&) noexcept {
    return asc::MemorySpace::kHost;
  }

  static constexpr asc::ExpressionAliasMetadata Alias(
      const Pair& pair) noexcept {
    return {&pair, pair.values.data(), sizeof(pair.values)};
  }
};

template <>
struct asc::WritableExpressionAdapter<Pair> {
  static constexpr std::array<asc::extent_t, 1> Shape(
      const Pair&) noexcept {
    return {2};
  }

  static constexpr asc::ExpressionAliasMetadata Alias(
      const Pair& pair) noexcept {
    return asc::ExpressionPlacementAdapter<Pair>::Alias(pair);
  }

  static constexpr bool IsUnique(const Pair&) noexcept {
    return true;
  }

  static constexpr void Write(
      Pair& pair, std::span<const asc::index_t, 1> indices,
      double value) noexcept {
    pair.values[static_cast<std::size_t>(indices[0])] = value;
  }
};

static_assert(asc::PlacedReadableExpression<Pair>);
static_assert(asc::WritableExpression<Pair>);
```

These adapters do not extend `Pair`'s lifetime. The caller must keep the object
and its storage alive and must provide valid indices whenever it invokes the
adapter operations. An evaluator may reject a nonunique writable destination
before mutation.

## Alias identity and byte spans

`AliasToken` is an opaque identity supplied by the caller or owning storage
layer. `MayAlias(expression, token)` is conservative:

- `true` means aliasing is possible or unknown; and
- `false` guarantees that the expression does not reference that token.

`ExpressionAliasMetadata` always contains an optional identity. The
one-argument constructor records identity only; the three-argument constructor
also records a byte-span start and size. A zero-sized span is empty. If a
nonempty span has a null address or its integer address range overflows, overlap
testing is conservatively true.

`ExpressionMayOverlap(source, destination)` reports possible overlap when
identities match, byte spans overlap, or the readable adapter's legacy
`MayAlias` query accepts the destination identity. Missing byte spans preserve
the identity-token contract rather than asserting disjoint storage.

Tokens and alias metadata do not own or retain storage. They do not make
concurrent access safe and do not replace an evaluator's destination-side
overlap policy.

## Sparsity effects

`SparsityEffect` has exactly these values:

- `kStructurePreserving`;
- `kStructureFiltering`;
- `kStructureUnion`;
- `kStructureIntersection`;
- `kValueDependent`;
- `kDensifying`; and
- `kDestinationRequired`.

The metadata describes what evaluation could do to a structure; Expression
does not itself evaluate or allocate one.

## Capture and lifetime rules

Operand capture is explicit and safe against direct rvalue references:

- arithmetic scalar terminals are copied and have rank zero;
- lvalue expressions are stored through non-owning references;
- rvalue expressions and nested nodes are moved or copied by value; and
- no node directly references an rvalue temporary.

An lvalue operand must outlive every node and scalar read that references it.
Moving or destroying a referenced object can invalidate the node. Capturing a
non-owning view by value extends the view object's lifetime, not the underlying
storage lifetime.

Nodes own captured scalar values and rvalue node state. They do not own
referenced lvalues, aliases, or any storage reached through a captured view.

## Pointwise nodes and shapes

The approved factories are:

- `MakeNegate`;
- `MakeAdd`;
- `MakeSubtract`; and
- `MakeMultiply`.

Negation preserves rank and exact shape. Binary ranked operands must have
identical compile-time rank and an identical extent in every dimension.
Shape incompatibility fails during construction, before a node is published.
`MakeNegate` returns its infallible node directly. The three binary factories
return `Result` because shape compatibility can fail.

Rank-zero scalar expansion is the sole exception:

- scalar with ranked operand produces the ranked shape;
- ranked operand with scalar produces the ranked shape; and
- two scalar operands produce rank zero.

There is no length-only compatibility, trailing-axis broadcasting, singleton
axis expansion, implicit reshape, or runtime-rank broadcasting.

## Node metadata

Nodes expose their result scalar type, compile-time rank, exact shape, indexed
scalar read, operation category, conservative alias result, and sparsity
effect.

The public queries are `ExpressionShape`, `ExpressionRead`, `MayAlias`,
`ExpressionSparsityEffect`, and `ExpressionOperationCategory`.
`ExpressionOperation` distinguishes `kExternal`, `kScalar`, `kNegate`, `kAdd`,
`kSubtract`, `kMultiply`, and `kTerminal`. The first six values retain their
foundational modules ordinals. Dense views use `kTerminal` as storage-backed readable
expression terminals.

| Node | Sparsity effect |
| --- | --- |
| Negate | `kStructurePreserving` |
| Add | `kStructureUnion` |
| Subtract | `kStructureUnion` |
| Multiply | `kStructureIntersection` |
| Any scalar-expanded binary operation | `kValueDependent` |

Alias queries propagate conservatively from every referenced operand. A scalar
terminal aliases no caller token.

```cpp
Pair pair{{1.0, 2.0}};
auto sum = asc::MakeAdd(pair, 3.0);
if (!sum.ok()) {
  return 1;
}

const std::array<asc::index_t, 1> index{1};
const double value = asc::ExpressionRead(
    *sum, std::span<const asc::index_t, 1>(index));
// value == 5.0; sum holds a non-owning reference to pair.
```

## Allocation, evaluation, and thread safety

Node construction and scalar reads perform no result allocation, destination
mutation, data transfer, synchronization, execution-context selection, or
provider dispatch. Holding an rvalue operand by value may run that operand's
ordinary C++ move/copy operation; it is not result materialization.

Expression objects contain no shared global state. Concurrent reads of one
node are safe only when all referenced expressions and storage support the
same concurrent reads and are not being moved, mutated, or destroyed.
Expression supplies no synchronization for referenced storage.

## Deliberately absent

Through Sparse, Expression defines no:

- storage owner or materialized result;
- evaluator, assignment operation, traversal policy, or temporary;
- reduction or algebra descriptor;
- general broadcasting or implicit shape conversion;
- execution or provider selection, transfer, or fallback; or
- Dense or Sparse dependency.

The writable protocol describes an existing destination but does not create,
allocate, resize, or own one. Owning storage modules decide traversal, alias
resolution, temporaries, workspace, execution, and provider selection.

The [frozen Sparse contract][contract] is authoritative if a historical
expression page implies broader evaluation or storage behavior.

[contract]: ../architecture/decisions/0010-expression-protocol.md
