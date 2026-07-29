# ADR 0011: dense owners, layouts, evaluation, and alias semantics

Status: Proposed at Architecture Checkpoint A

## Context

Dense needs multidimensional ownership/views without depending on sparse or
utilities. Historical owner/view and build-global layout behavior was
ambiguous, while useful extents/mapping tests exist as prior art.

## Decision

Dense uses:

```text
Extents + LayoutMapping + Accessor -> DenseView
Buffer + unique exhaustive mapping -> DenseArray
```

- Compile-time rank with mixed static/dynamic extents.
- Named default `LayoutLeft` (column-major).
- Equal support for `LayoutRight` (row-major) and explicit non-negative
  `LayoutStride`.
- No build-global layout macro; every algorithm uses operand metadata.
- Negative strides are deferred.
- Mutable views require proven uniqueness; const views may represent repeated
  addresses only through an explicitly supported read-only mapping.
- Owners are move-only and contiguous/unique/exhaustive.
- Slices/subviews are zero-allocation non-owners.
- Reshape is zero-allocation only when count/mapping permit; otherwise it
  fails. Resize is explicit and invalidates views.

Dense evaluates storage-neutral pointwise expressions into caller-provided
destinations. Validation and alias analysis complete before mutation.
Exact same-index in-place operations may be permitted per operation. Other
overlap is conservatively rejected unless an explicit workspace/materialize
policy is supplied.

No evaluator hides packing, allocation, transfer, synchronization, or
fallback. Workspace/packing requirements are queryable before execution.
Logical iteration is independent of physical layout.

## Consequences

Column-major default preserves historical scientific convention, but
row-major is a first-class mapping and consumers must not infer the default
from provider behavior. Conservative overlap rejection may reject some safe
interleaved cases.

## Verification

Test rank/extent boundaries, left/right/padded/stride formulas, uniqueness,
holes, const views, owner lifetime, clone/resize/reshape, overlap transactions,
allocation/packing counters, and identical logical results across layouts.
