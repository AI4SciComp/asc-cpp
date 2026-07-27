# Expression module

`ASC::expression` is the storage-neutral C++20 expression component in the
unreleased ASCCpp `0.9.0` candidate. Milestone 2 established participation,
scalar reads, safe operand capture, pointwise nodes, exact-shape checks, alias
metadata, and sparsity-effect metadata. Milestone 4 additively supplies
placement, writable-destination, and span-aware alias protocols needed by
storage-owned evaluation and algebra.

Expression owns no array, allocation, destination storage, evaluator, execution
context, provider, transfer, synchronization, or linear algebra. Dense and
sparse each evaluate the protocol independently.

## Build and dependency contract

```text
build target:     asc_expression
build-tree alias: ASC::expression
installed target: ASC::expression
target kind:      interface
direct ASC deps:  ASC::core
external deps:    none
```

The component is a genuine interface target because the approved C++20
concepts, customization point, holders, and nodes are templates. It does not
ship an empty compiled object.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS expression)
target_link_libraries(my_target PRIVATE ASC::expression)
```

`ASC::core` is loaded transitively. Utilities, random, dense, sparse, the
aggregate, and provider targets are absent from an isolated expression
consumer.

## Public headers

| Header | Contract |
| --- | --- |
| `<asc/expression.h>` | complete readable, placement, and writable surface |
| `<asc/expression/expression.h>` | customization, protocol, nodes, and metadata |
| `<asc/expression/writable.h>` | placement and writable-destination customization |

Both headers are self-contained. All supported declarations are directly in
`namespace asc`.

## Public protocol surface

| Name | Purpose |
| --- | --- |
| `ExpressionOperationCategory` | terminal versus pointwise operation metadata |
| `SparsityEffect` | conservative structural consequence metadata |
| `AliasToken`, `AliasTokensMayOverlap` | identity or validated byte-span alias metadata |
| `ExpressionAdapter<T>` | external and ASC expression customization |
| `ReadableExpression` | complete adapter-participation concept |
| `ExpressionValue<T>` | adapter-declared scalar value type |
| `kExpressionRank<T>` | adapter-declared compile-time rank |
| `kExpressionOperationCategory<T>` | operation-category metadata |
| `kExpressionSparsityEffect<T>` | sparsity-effect metadata |
| `ExpressionShape`, `ReadExpression`, `MayAlias` | protocol queries |
| `ExpressionPlacementAdapter<T>`, `PlacedReadableExpression` | explicit readable placement |
| `WritableExpressionAdapter<T>`, `WritableExpression` | non-intrusive destination contract |
| `ExpressionSpace`, `WritableExpressionShape`, `WritableExpressionAlias`, `WriteExpression` | placement and write queries |
| `ScalarExpression` | rank-zero arithmetic terminal held by value |
| `ExpressionReference`, `ExpressionOwner` | non-owning lvalue and owning value capture |
| `UnaryExpression`, `BinaryExpression` | pointwise node types |
| `NegateOperation`, `AddOperation`, `SubtractOperation`, `MultiplyOperation` | built-in scalar operations |
| `MakeNegate`, `MakeAdd`, `MakeSubtract`, `MakeMultiply` | validated node factories |

Callers should use the factories rather than spelling holder or node template
arguments. Node constructors are private to the expression factory machinery.
The deduced result types remain ordinary movable values. Factory constraints
reject an operand value type that does not support the selected scalar
operation.

## Non-intrusive participation

`ExpressionAdapter<T>` is the single customization point. An external type
participates by explicitly specializing the adapter for its unqualified type;
it does not inherit from an ASC base and does not include a dense or sparse
header.

A `ReadableExpression` supplies through its adapter:

- a scalar `value_type`;
- compile-time `rank`;
- an exact shape using core `extent_t`;
- indexed scalar read using core `index_t`;
- a conservative alias query; and
- a `SparsityEffect`.

The protocol assumes neither contiguity nor random-access storage, explicit
zeros, ownership, host placement, allocation, or a provider. The adapter is
responsible for preserving the external object's own lifetime and index
validity contract.

```cpp
#include <asc/expression.h>

#include <array>
#include <cstddef>
#include <span>

struct ExternalVector {
  std::array<double, 2> values;
};

namespace asc {

template <>
struct ExpressionAdapter<::ExternalVector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(const ::ExternalVector&) { return {2}; }

  static double Read(const ::ExternalVector& vector,
                     std::span<const index_t, 1> indices) {
    return vector.values[static_cast<std::size_t>(indices[0])];
  }

  static bool MayAlias(const ::ExternalVector& vector, AliasToken token) {
    auto values = AliasToken::FromAddressSpan(
        vector.values.data(), vector.values.size() * sizeof(double));
    return !values.ok() || AliasTokensMayOverlap(*values, token);
  }
};

}  // namespace asc

int main() {
  ExternalVector vector{{1.0, 3.0}};
  auto expression = asc::MakeAdd(vector, 2.0);
  if (!expression.ok()) {
    return 1;
  }

  constexpr std::array<asc::index_t, 1> index = {1};
  const double value =
      asc::ReadExpression(*expression, std::span<const asc::index_t, 1>(index));

  return value == 5.0 &&
                 asc::MayAlias(*expression, asc::AliasToken::FromIdentity(
                                                vector.values.data()))
             ? 0
             : 1;
}
```

The adapter's `Read` operation returns its declared `value_type`. Invalid or
incomplete adapters fail `ReadableExpression` constraints. The alias query
reports `true` if the fixed storage span cannot be represented, because
`false` must guarantee non-overlap.

## Placement and writable destinations

Placement and writing are separate opt-in customizations. A placed readable
type specializes `ExpressionPlacementAdapter<T>` with:

```text
static MemorySpace Space(const T&)
```

`PlacedReadableExpression<T>` requires the independent
`ReadableExpression<T>` contract and an explicit `MemorySpace`. Placement is
metadata only. It does not authorize host dereference, transfer storage,
select a backend, or make the expression writable.

A destination additionally specializes `WritableExpressionAdapter<T>` with:

```text
using value_type = ...
static constexpr rank_t kRank = ...
static array<extent_t, kRank> Shape(const T&)
static AliasToken Alias(const T&)
static void Write(T&, span<const index_t, kRank>, value_type)
```

`WritableExpression<T>` requires matching readable/writable value types and
ranks. An owning algorithm separately checks that
`WritableExpressionShape(object) == ExpressionShape(object)` before mutation.
`WriteExpression` is called only for an already validated logical coordinate.
The adapter promises that the coordinate names one unique writable scalar and
that writing cannot allocate, transfer, synchronize, select a provider, or
fail. If a type cannot make that promise for every coordinate an algorithm may
request, it must not advertise unrestricted writable participation.

Top-level-volatile descriptor objects do not satisfy `ReadableExpression`,
`PlacedReadableExpression`, or `WritableExpression`. A top-level-const object
may remain readable and placed, but is not writable. Element-level mutability
is a storage-module contract independent of descriptor cv-qualification.

The writable adapter normally stores or derives its alias token when the view
is validated. `AliasToken::FromAddressSpan` is fallible, while `Alias` itself
returns a token without failure; a dynamic external view should therefore
validate and retain its span token in its own factory rather than discovering
an invalid range during a write.

## Rank and shape

Rank is compile-time metadata expressed with core `rank_t`. Ranked expressions
carry one `extent_t` per dimension. Compatible ranked operands must have equal
rank and every equal extent.

Arithmetic scalar terminals have rank zero and are captured by value. A
rank-zero scalar may expand against one ranked operand. Two rank-zero operands
produce rank zero. General broadcasting, trailing-axis alignment, implicit
reshape, and storage conversion are not provided.

Factories return `Result` when full-shape comparison can fail. A failed
factory publishes no node and performs no destination mutation.

## Capture and lifetime

Capture is determined at factory construction:

- arithmetic scalar terminals are stored by value;
- lvalue expressions are stored through a documented non-owning reference;
- rvalue expressions and nested nodes are stored by value; and
- no node stores a direct reference to an rvalue.

The owner of an lvalue operand must outlive every node read that refers to it.
Moving a node transfers its value-held operands. Capturing a non-owning view by
value keeps the view descriptor alive but never extends the lifetime of the
storage it views.

`ExpressionReference` rejects direct construction from both mutable and const
rvalues. `ExpressionOwner` owns its expression value. These public holders
make the deduced node lifetime visible; factories choose between them from the
operand value category.

Copying an `ExpressionReference` copies one non-owning pointer. Copying an
`ExpressionOwner` or a node copies every value-held operand and therefore may
inherit arbitrary copy cost or allocation from an external operand type; it
is available only when those operands are copyable. Moving transfers
value-held operands and copies non-owning reference identities.

Factories snapshot operand shape into the node. A borrowed lvalue operand must
therefore keep both its object lifetime and a compatible shape/index contract
for every subsequent node read. Resizing, reshaping, or otherwise invalidating
the operand's indexed-read contract invalidates the node even if the operand
object itself remains alive.

Expression construction does not read the complete operands, allocate a
result, evaluate a destination, select a provider, transfer data, or
synchronize.

## Pointwise nodes

Milestone 2 contains exactly these built-in factories:

```text
MakeNegate
MakeAdd
MakeSubtract
MakeMultiply
```

Nodes expose scalar result type, compile-time rank, exact shape, indexed scalar
read, operation category, conservative alias propagation, and sparsity effect.
Reads are scalar composition only; they do not materialize or cache an array.

Built-in operations use ordinary C++ unary/binary operator rules and
`decltype` result promotion; the expression module adds no independent numeric
promotion table. Signed integral negation, addition, subtraction, and
multiplication require a representable result. Otherwise the ordinary C++
signed-overflow rule applies, including undefined behavior. Floating-point
NaN, infinity, rounding, and contraction behavior follows the participating
native scalar type and compiler mode; the module adds no reproducible-math or
exception-status layer.

The built-in sparsity classifications are:

| Operation | Sparsity effect |
| --- | --- |
| negation | `kStructurePreserving` |
| addition/subtraction | `kStructureUnion` |
| multiplication | `kStructureIntersection` |
| any scalar-expanded binary operation | `kValueDependent` |

The full public `SparsityEffect` vocabulary is:

```text
kStructurePreserving
kStructureFiltering
kStructureUnion
kStructureIntersection
kValueDependent
kDensifying
kDestinationRequired
```

The extra values allow external expressions and future storage evaluators to
state conservative behavior without introducing a storage dependency.

## Alias metadata

`AliasToken` is opaque, non-owning metadata. It can represent:

- an identity created with `FromIdentity(pointer)`; or
- a validated half-open byte span created with
  `FromAddressSpan(pointer, bytes)`.

`FromAddressSpan` returns `Result<AliasToken>`. A null address is valid only
for zero bytes. A nonempty null range returns `kMemoryAccess`, and an endpoint
that cannot be represented in `uintptr_t` returns `kOverflow`. A zero-byte
span never overlaps another token, including itself.

`AliasTokensMayOverlap(left, right)` is symmetric:

- two spans use half-open byte-range intersection;
- a span and identity treat the identity as one address point;
- two identity-only tokens compare their identities; and
- any comparison containing a zero span is false.

`AliasToken::operator==` deliberately compares identity only, including when
the tokens also contain span metadata. Use `AliasTokensMayOverlap` for
span-aware questions. Identity-only metadata is sufficient only when exact
identity fully captures the type's alias domain or partial overlap is
impossible by construction.

`MayAlias(expression, token)` remains conservative:

- `true` means evaluation must allow for an overlapping referenced object;
- `false` guarantees that the expression does not reference that token.

An external adapter whose storage covers a byte range should publish the
complete validated physical span and implement `MayAlias` with
`AliasTokensMayOverlap`. An identity-only start address is insufficient when
the external range starts before a later overlapping ASC range: the identity
point is outside the ASC span even though subsequent bytes intersect it.
Strided storage uses a conservative physical span including holes. If storage
has disjoint regions that one token cannot represent, the adapter retains
enough metadata to answer `MayAlias` conservatively.

Built-in nodes propagate the query to their operands. ASC root dense and
sparse views publish validated physical value spans. A dense subview retains
its root span, conservatively reporting possible overlap between physically
disjoint sibling subviews rather than missing a shared-root overlap. Alias
tokens retain no pointer lifetime and authorize no dereference.

Expression does not decide whether an evaluator can operate in place, allocate
a temporary, or reject overlap. That policy belongs to the destination storage
module.

## Cost, failure, and concurrency

| Surface | Ownership and lifetime | Failure | Cost |
| --- | --- | --- | --- |
| adapter metadata/read | follows external type's contract | adapter-defined index preconditions | shape query and scalar read costs are adapter-defined |
| placement/write query | follows external destination's contract | constraints reject incomplete adapters; owning algorithm validates runtime metadata | adapter-defined constant or metadata-query cost |
| node factory | values or non-owning lvalue references | incompatible shape returns `kShape` | adapter-defined shape queries plus rank-linear validation/comparison; no result-storage allocation; external operand move/copy behavior still applies |
| node scalar read | node and operands must remain valid | index validity follows operand protocol | operation-tree work for one scalar; no cache |
| alias-span creation | token retains no storage | invalid null range or address overflow returns status | constant-time checked address arithmetic |
| alias query | opaque token is not retained | conservative Boolean result | constant-time token comparison plus operation-tree traversal |

Immutable expression nodes may be read concurrently only when every referenced
operand permits concurrent const reads. Concurrent mutation of a referenced
operand is the caller's responsibility.

## Deferred work and provenance

There is no expression-owned destination, evaluation, reduction, algebra
descriptor, general broadcasting, result-type selection, materialization,
temporary policy, provider dispatch, or arbitrary GPU callable. Dense and
sparse types remain absent from expression headers; their own headers
specialize the neutral protocol.

The implementation is project-owned and follows
[ADR 0010](../development/asc-cpp-architecture/decisions/0010-expression-protocol.md)
and the frozen Milestone 2 and Milestone 4 contracts.
No MdeCpp/deleted asc-cpp expression source or tests are copied.
