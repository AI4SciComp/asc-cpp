# ADR 0001: freeze six modules and random-owned integration facets

Status: Proposed at Architecture Checkpoint A

## Context

The owner requires exactly `core`, `utilities`, `expression`, `dense`,
`sparse`, and `random`. The deleted implementation combined storage and
expressions in `array`, algebra in `linalg`, and storage generation in random.
Those boundaries violate the restart contract.

## Decision

Freeze these direct dependencies:

```text
core       -> []
utilities  -> [core]
expression -> [core]
dense      -> [core, expression]
sparse     -> [core, expression]
random     -> [core]
```

Add two random-owned integration facets, not modules:

```text
random_dense  -> [random, dense]
random_sparse -> [random, sparse]
```

`ASC::cpp` is a convenience aggregate of all six provider-free modules and the
two random facets. It is never a dependency of a narrower target and contains
no optional provider.

There is no public `array`, `linalg`, common backend, or mixed-storage module.
Dense and sparse each own storage, expression evaluation, CPU linear algebra,
and GPU linear algebra.

## Consequences

- Some neutral vocabulary moves down to core/expression; shared storage code
  does not.
- Mixed operations use neutral descriptors and destination-owned evaluation.
- Provider facets remain owned by a module and do not change the module count.
- Duplication is preferable to a forbidden sibling edge until a truly neutral
  abstraction is proven.

## Verification

Audit source includes, build targets, installed imported targets, and isolated
consumers against `dependency-manifest.yaml`. Any forbidden edge fails CI.
