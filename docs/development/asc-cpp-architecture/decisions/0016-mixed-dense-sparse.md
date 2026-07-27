# ADR 0016: explicit destination for mixed dense/sparse operations

Status: Proposed at Architecture Checkpoint A

## Context

Mixed operations can tempt a dense-to-sparse or sparse-to-dense module edge,
automatic result-type choice, or silent densification. All are forbidden.

## Decision

There is no mixed-storage module and no dense/sparse dependency.

- Dense and sparse terminals participate through expression-neutral
  customization.
- The destination owner performs evaluation.
- Mixed operations require a caller-provided dense destination, sparse
  destination/builder, or an explicitly named conversion into one.
- Result storage is never inferred from operand order.
- Sparse-preserving/filtering work may use sparse evaluation.
- Union/intersection/value-dependent structural work requires an explicit
  sparse builder and workspace.
- Densifying work is rejected by sparse evaluation and requires an explicit
  dense destination.
- Sparse algorithms accept neutral vector/matrix operands; dense types can
  satisfy them without a sparse include of dense.
- Allocation, conversion, complexity, aliasing, and memory-space requirements
  are queryable/validated before mutation.

Function templates may instantiate with both concrete types in a user
translation unit only through the neutral protocol. No installed target gains
a sibling edge.

## Consequences

Mixed calls are more verbose but cannot surprise the caller with storage,
complexity, or allocation. Some convenient result-returning overloads are
deliberately absent.

## Verification

Build/consume each storage without the other; instantiate mixed operations when
both are present; inspect preprocessed includes and imported targets; test huge
sparse inputs under allocation limits; ensure densifying operations fail
without a dense destination.
