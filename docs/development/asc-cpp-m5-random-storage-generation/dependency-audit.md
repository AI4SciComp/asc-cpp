# Milestone 5 Dependency Audit

Status: Frozen; implementation evidence pending

Date: 2026-07-26

## Approved graph

```text
asc_core          / ASC::core          -> []
asc_utilities     / ASC::utilities     -> [ASC::core]
asc_expression    / ASC::expression    -> [ASC::core]
asc_random        / ASC::random        -> [ASC::core]
asc_dense         / ASC::dense         -> [ASC::core, ASC::expression]
asc_sparse        / ASC::sparse        -> [ASC::core, ASC::expression]
asc_random_dense  / ASC::random_dense  -> [ASC::random, ASC::dense]
asc_random_sparse / ASC::random_sparse -> [ASC::random, ASC::sparse]
asc_cpp           / ASC::cpp           -> [all six modules and both facets]
```

## Forbidden Milestone 5 edges

```text
random base   -> dense | sparse | expression | utilities
dense         -> random | sparse | utilities
sparse        -> random | dense | utilities
random_dense  -> sparse | random_sparse | utilities
random_sparse -> dense | random_dense | utilities
any base/facet -> provider target or SDK
```

The source and installed target graphs, public-header includes, component
closures, isolated consumers, and provider-disabled configuration must all
match this policy. Evidence will be completed after integration and review.
