# Milestone 4 dependency, capability, license, and provenance audit

Status: complete; no unresolved dependency, capability, license, or
provenance blocker

Evidence date: 2026-07-28

## Target and package closure

```text
asc_sparse
├── direct ASC dependency: ASC::core
├── direct ASC dependency: ASC::expression
└── external link dependencies: none

build-tree alias: ASC::sparse
installed target: ASC::sparse
available package component: sparse
```

`asc_sparse` is a genuine compiled target. Its sole compiled production source
is `src/sparse/reference_linalg.cc`; storage, conversion, evaluation, and
protocol templates remain in their owning headers. Its direct and exported
ASC dependency lists must remain exactly `ASC::core;ASC::expression`.

A required `sparse` component request expands exactly to
`core;expression;sparse`. Dense is an independently available sibling and is
never imported by Sparse. Random storage facets, the aggregate, and provider
facets remain unavailable and unexported.

ASCCMake 0.1.0 is an exact configure-time CMake-language dependency, not a
consumer link dependency. Milestone 4 uses only
`asc_target_enable_cxx20`, `asc_target_enable_warnings`,
`asc_target_enable_sanitizers`, and `asc_register_test`.

## Exact production inventory

```text
include/asc/sparse.h
include/asc/sparse/compressed.h
include/asc/sparse/coordinate.h
include/asc/sparse/evaluate.h
include/asc/sparse/export.h
include/asc/sparse/linalg.h
src/sparse/reference_linalg.cc
```

The approved predecessor compatibility surface is:

```text
include/asc/expression.h
include/asc/expression/expression.h
include/asc/expression/writable.h
include/asc/dense/view.h
```

Expression gains only storage-neutral placement, writable, uniqueness, and
conservative alias-span metadata. Dense gains only a non-owning specialization
of that neutral protocol. Expression does not include Sparse or Dense; Dense
does not include Sparse.

## Capability and backend closure

Milestone 4 capabilities are limited to:

- general-rank coordinate builder/finalized owner/view;
- canonical rank-two CSR and CSC owner/view;
- named coordinate/CSR/CSC conversions;
- structure-preserving evaluation into existing Sparse structure; and
- deterministic serial CSR SpMV for `float` and `double`.

All later Random facets, optimized providers, GPU providers, broader formats,
structural expression operations, SpMM, transpose, and solvers remain
proposed. No provider option, discovery, language, SDK include/link, target,
component, dispatch, or fallback exists.

GPU evidence is **skipped**. Discovery or hardware inventory is not relabeled
as configure-tested, compile-tested, runtime-tested, or parity-tested.

## License and provenance

ASCCpp remains Apache-2.0. ASCCMake 0.1.0 is Apache-2.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

MdeCpp remains a separate GPLv3 comparison repository at
`f6294e9079262682ce63ae7ff2d8a643e658bf5d`; its pre-existing modified
`Makefile` is untouched. Milestone 4 is clean-room work from the frozen
contract and elementary sparse definitions. No MdeCpp/deleted/third-party
source, test, literal corpus, table, prose, or mechanical translation is used.

## Final validation

The stabilized, post-review candidate passed:

```text
GCC 11.4 Debug/static/current CMake:       141/141
Clang 19 Release/shared/current CMake:     141/141
GCC 11.4 Release/static/CMake 3.25.0:      141/141
Clang 19 ASan + UBSan compatible set:      125/125
Clang 19 standalone LSan compatible set:    10/10
Clang 19 standalone TSan compatible set:    10/10
```

Each complete matrix includes exact public-file, product-target, dependency,
component, build-tree, copied-build-tree, install/relocation, subproject,
paths-with-spaces, isolated Sparse consumer, unavailable-component, and
package-registry checks. Required `sparse` resolves exactly
`core;expression;sparse`. The installed Sparse-only consumer uses an external
vector adapter, links the compiled SpMV entry point, and imports no Dense,
Utilities, Random, aggregate, or provider target.

The relocated Clang shared library reports `RUNPATH [$ORIGIN]`,
`NEEDED libasc_core.so` plus platform C++ runtimes, and exactly the approved
exported `float` and `double` `SpmvReference` bridge symbols. Its installed
target records `INTERFACE_LINK_LIBRARIES
"ASC::core;ASC::expression"`.

The deterministic benchmark reports zero process allocations for both
structure-preserving evaluation and CSR SpMV. Conversion tests account for
the explicit destination buffers and prove exact partial-failure release.
There is no dense buffer, hidden workspace, transfer, provider dispatch, or
fallback.

Formatting, patch whitespace, header self-containment with exceptions enabled
and disabled, the exact M4 dependency script, and all 11 expected compile
failures pass. No local clang-tidy executable exists, so clang-tidy is skipped
locally and remains a hosted-CI gate. Native MSVC and AppleClang execution
also remain hosted-CI evidence.

GPU evidence is exactly **skipped**.
