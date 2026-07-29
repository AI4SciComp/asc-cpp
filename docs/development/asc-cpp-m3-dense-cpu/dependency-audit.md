# Milestone 3 dependency, capability, license, and provenance audit

Status: Publication Checkpoint B candidate

Evidence date: 2026-07-28

## Target and package closure

```text
asc_dense
├── direct ASC dependency: ASC::core
├── direct ASC dependency: ASC::expression
└── external link dependencies: none

build-tree alias: ASC::dense
installed target: ASC::dense
available package component: dense
```

`asc_dense` is a genuine compiled target. Its sole compiled production source
is `src/dense/linalg.cc`; mapping, view, owner, evaluation, and reduction
templates remain in their owning headers. Its direct and exported ASC
dependency lists are exactly `ASC::core;ASC::expression`.

The installed component closure for a required `dense` request is exactly
`core;expression;dense`. Utilities and Random are independently available
siblings, not dependencies of Dense. Sparse, random storage facets, provider
facets, and the aggregate `cpp` component remain unavailable and unexported.

The project uses the released ASCCMake 0.1.0 APIs
`asc_target_enable_cxx20`, `asc_target_enable_warnings`, and
`asc_target_enable_sanitizers`. ASCCMake is an exact configure-time
CMake-language dependency and is not linked or exported to consumers.

## Exact production inventory

The Milestone 3 public inventory is exactly:

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
```

The compiled production inventory is exactly:

```text
src/dense/linalg.cc
```

The target-owned build file is `src/dense/CMakeLists.txt`. Configure-time and
CTest audits compare the complete observed inventory and target topology with
this contract, inspect both direct link properties, and scan production
includes.

Every Dense production include is either a C++20 standard-library header, an
`asc/core` header, an `asc/expression` header, or another `asc/dense` header.
The audit rejects Utilities, Random, Sparse, retired Array/Linalg, provider
SDK, and third-party includes. Every public header compiles as the first ASC
include and with exceptions disabled.

Milestone 3 appends `ExpressionOperation::kTerminal` after all six Milestone 2
enumerators and adds the optional recursive
`ValidateExpressionAccess(context, expression)` customization. Those are
contract-authorized predecessor compatibility corrections needed to describe
Dense leaves and to reject non-host source expressions transactionally. They
do not create a Dense dependency in `ASC::expression`.

## Capability and backend closure

Implemented Milestone 3 capabilities are limited to:

- checked rank-zero through rank-three left, right, and explicit non-negative
  stride mappings;
- unique mutable and const non-owning views and rank-preserving subviews;
- move-only host owners backed by Core `Buffer` and `MemoryResource`;
- serial pointwise evaluation and deterministic sum/minimum/maximum
  reductions; and
- serial reference `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm`
  for `float` and `double`.

The capability manifest classifies those Dense capabilities as
`runtime-tested`; every later Sparse, random-storage, optimized-provider, and
GPU capability remains proposed. No optimized CPU or GPU provider target,
option, language, SDK include, link edge, package component, or runtime path
exists.

GPU evidence is **skipped**. Toolkit or hardware inventory is not relabeled
as configure-tested, compile-tested, runtime-tested, or parity-tested
evidence.

## Direct dependency and provider audit

```sh
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m3_dependency_check.cmake
```

Result: pass. The script confirms the exact header/source inventory and scans
the production closure for forbidden sibling, historical, provider, SDK, and
third-party references.

The integrated CMake checks additionally require:

```text
asc_dense LINK_LIBRARIES           = ASC::core;ASC::expression
asc_dense INTERFACE_LINK_LIBRARIES = ASC::core;ASC::expression
```

Result: pass in every clean configure. Direct external link/runtime
dependencies remain limited to the platform C++20 standard library. No
unapproved dependency was added.

## License and provenance

ASCCpp remains Apache-2.0 and installs its root `LICENSE`. ASCCMake 0.1.0 is
Apache-2.0 at commit `8a7dcbad3a97267cce59810aff24de800a3497a7`.

MdeCpp remains a separate GPLv3 comparison repository at verified commit
`f6294e9079262682ce63ae7ff2d8a643e658bf5d`. Its pre-existing modified
`Makefile` was not touched. Milestone 3 production, verification, benchmark,
and documentation work is original implementation from the frozen contract
and approved ADRs; no MdeCpp source, test, literal corpus, generated data, or
mechanical translation was copied.

## Validation evidence

- CMake 3.25.0, GCC 11.4, Release/static, warnings-as-errors: 109/109 tests
  passed.
- CMake 4.1.2, GCC 11.4, Debug/static, warnings-as-errors: 109/109 tests
  passed.
- CMake 4.1.2, Clang 19, Release/shared, warnings-as-errors: 109/109 tests
  passed after final corrections.
- Clang 19 ASan+UBSan, Debug/static: 95/95 compatible in-tree tests passed;
  the final Dense-focused set passed 5/5.
- Standalone Clang 19 LSan and TSan compatible Dense sets: 5/5 passed under
  each sanitizer.
- Static/shared build-tree, installed, relocated, subproject, package
  component, registry-preservation, path-with-spaces, and isolated Dense
  consumers passed.
- The Clang shared install records `NEEDED libasc_core.so`,
  `RUNPATH [$ORIGIN]`, and exactly the approved 20 public linear-algebra
  overload symbols.

Exact commands, expected incompatibilities, findings, hosted-platform skips,
and performance observations are recorded in the Milestone 3 Publication
Checkpoint B report and the independent reviews.
