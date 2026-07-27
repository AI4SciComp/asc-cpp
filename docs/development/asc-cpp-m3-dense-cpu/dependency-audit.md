# Milestone 3 Dependency Audit

Status: Complete at Publication Checkpoint B

Date: 2026-07-26

## Approved graph

```text
asc_core        / ASC::core        -> []
asc_utilities   / ASC::utilities   -> [ASC::core]
asc_expression  / ASC::expression  -> [ASC::core]
asc_random      / ASC::random      -> [ASC::core]
asc_dense       / ASC::dense       -> [ASC::core, ASC::expression]
```

The first four rows retain their Milestone 2 contracts. Milestone 3 adds only
the fifth row.

## Forbidden Milestone 3 edges

Dense production must not include or link:

```text
ASC::utilities
ASC::sparse
ASC::random
ASC::random_dense
ASC::random_sparse
ASC::cpp
any retired array/linalg target
any provider target or SDK
```

Core, utilities, expression, and random retain their predecessor dependency
ceilings and cannot acquire a dense edge.

## Mechanical enforcement

- `tests/compile/dependency_check.cmake` freezes the complete public and
  compiled production file inventory, scans direct includes by owner, rejects
  provider SDK names, and rejects retired paths.
- `tests/architecture/check_approved_product_targets.cmake` accepts only the
  five implemented product target/alias pairs and verifies compiled versus
  interface target kinds.
- `tests/compile/CMakeLists.txt` inspects the direct build and public link
  properties. `asc_dense` must expose exactly
  `ASC::core;ASC::expression`.
- package and dense-only consumers inspect the imported target graph and prove
  that a dense request imports core and expression but no sibling/provider.
- public-header compilation and the isolated dense consumer prove that no
  utility, sparse, random, provider, or retired header is required.

## Observed result

The final build-tree target inventory contains exactly these implemented
product pairs:

```text
asc_core       / ASC::core
asc_utilities  / ASC::utilities
asc_expression / ASC::expression
asc_random     / ASC::random
asc_dense      / ASC::dense
```

`asc_expression` is an interface target. The other four are compiled static or
shared libraries according to `BUILD_SHARED_LIBS`. No sparse, random-storage,
aggregate, retired, or provider target exists.

The dense build and installed targets expose exactly:

```text
INTERFACE_LINK_LIBRARIES = ASC::core;ASC::expression
INTERFACE_COMPILE_FEATURES includes cxx_std_20
```

The direct-include scan accepts dense includes of core/expression/dense only.
It found no utilities, sparse, random, retired array/linalg, optional-provider,
or SDK include in dense production.

The following clean complete matrices each passed **108/108**:

- CMake 4.1.2, GCC 11.4, Debug, static;
- CMake 4.1.2, Clang 19.0, Debug, shared;
- minimum CMake 3.25.0, GCC 11.4, Release, static.

Their 14 package-labeled cases cover build-tree and copied-build-tree component
requests, install and relocation into paths containing spaces, exact available
and unavailable components, no-component failure, optional unavailable
components, package-registry isolation, subproject use, and isolated
build-tree/relocated consumers. A dense-only request imports exactly
`ASC::dense`, `ASC::core`, and `ASC::expression`; it does not import
utilities, random, sparse, any random-storage facet, the aggregate, or a
provider.

The separately instrumented Clang 19 AddressSanitizer plus
UndefinedBehaviorSanitizer matrix passed **94/94** eligible non-package tests.
Package/consumer tests are intentionally kept in the non-instrumented matrices
so sanitizer runtime linkage does not become an exported dependency.

No forbidden edge, leaked dependency, package-registry mutation, or
unimplemented target was observed.
