# ADR 0010: storage-neutral expressions and destination-owned evaluation

Status: Proposed at Architecture Checkpoint A

## Context

The historical expression hierarchy named dense/sparse storage, inherited from
ASC bases, chose host/device execution, and wrote destinations. It could compare
flat size instead of full shape and made sparse densification implicit.

## Decision

Expression depends only on core and defines non-intrusive C++20 customization
points and constrained protocols for:

- expression participation;
- value type, compile-time rank, and shape;
- readable scalar/ranked access;
- writable destination description;
- placement/accessibility and alias description;
- operation category and sparsity effect.

It does not require ASC inheritance. An external type can participate through
the approved customization protocol.

Capture rules:

- scalars by value;
- lvalue operands through documented non-owning holders;
- rvalue nodes by value;
- never a direct reference to an rvalue;
- a view captured by value still does not own storage.

Nodes carry shape, result type, category, alias, and sparsity metadata but no
destination, provider, context, allocation, transfer, or synchronization.

v1 pointwise compatibility is exact shape plus rank-zero scalar expansion.
General trailing-axis broadcasting is deferred. Algebra operations are
descriptors or owner-module APIs, not pointwise callable nodes.

Dense and sparse own evaluation, traversal, alias handling, temporaries,
workspace, and provider selection. Expression never defines a storage-specific
`Evaluate`.

Sparsity effects are:

```text
structure-preserving, structure-filtering, structure-union/intersection,
value-dependent, densifying, destination-required
```

## Consequences

Storage evaluators may duplicate traversal logic. This preserves independence
and lets each enforce storage invariants. Arbitrary external GPU callables are
not promised; the first GPU evaluator supports an explicit built-in node set.

## Verification

Use an external non-ASC type, compile negatives, lvalue/rvalue/nested lifetime
tests, shape failure before mutation, metadata properties, zero allocation at
construction, and dense/sparse isolation scans.
