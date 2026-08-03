# ADR 0002: retain ASCCpp and use component-matched ASC targets

Status: Proposed at Architecture Checkpoint A

## Context

The historical package identity is useful, but its components were
five-module-specific. Released asc-cmake can generate one package/export per
call but does not implement conditional multi-component exports.

## Decision

Retain:

```text
package: ASCCpp
imported namespace: ASC::
```

Base build/import pairs:

```text
asc_core          / ASC::core
asc_utilities     / ASC::utilities
asc_expression    / ASC::expression
asc_dense         / ASC::dense
asc_sparse        / ASC::sparse
asc_random        / ASC::random
asc_random_dense  / ASC::random_dense
asc_random_sparse / ASC::random_sparse
asc_cpp           / ASC::cpp
```

Components match imported suffixes. No-component lookup requests `cpp`.
Unknown or unavailable required components make `ASCCpp_FOUND` false.

Provider components use owner/provider names, initially `core_cuda`,
`dense_cuda`, `sparse_cuda`, `random_cuda`, `random_dense_cuda`, and
`random_sparse_cuda`. They exist only when implemented.

Use released asc-cmake for C++20, warnings, sanitizers, options, and tests.
Use standard CMake install/export/config helpers for conditional ASCCpp
component exports. The config discovers an external provider only when its
component is requested.

## Consequences

- A CPU-only `core` or `random` consumer never discovers CUDA.
- Base components can be packaged independently.
- ASCCpp carries local component-closure logic; it does not imitate or wrap an
  unprovided asc-cmake helper.
- Adding a reusable asc-cmake feature requires a separate issue/release.

## Verification

Test build/install/relocated packages, paths with spaces, each component alone,
no-component lookup, unknown/unavailable requests, provider-disabled use, and
the installed imported-target graph.
