# Milestone 5 Dependency Audit

Status: Publication Checkpoint B candidate; final validation passed

Date: 2026-07-28

## Product graph

Milestone 5 adds exactly:

```text
asc_random_dense  / ASC::random_dense  -> ASC::random;ASC::dense
asc_random_sparse / ASC::random_sparse -> ASC::random;ASC::sparse
asc_cpp           / ASC::cpp           -> all six provider-free modules and
                                          both Random storage facets
```

All three are functional or aggregate C++20 interface targets. The two facet
headers contain the complete approved type/rank-dependent generation
behavior. The aggregate contains no production behavior.

The six base module edges are unchanged. Base Random remains storage-neutral,
Dense and Sparse do not import Random or one another, and no narrower target
imports `ASC::cpp`.

## Package graph

Available components are exactly:

```text
core utilities expression dense sparse random
random_dense random_sparse cpp
```

Exact recursive closures are:

```text
random_dense  -> core;expression;random;dense;random_dense
random_sparse -> core;expression;random;sparse;random_sparse
cpp           -> core;utilities;expression;dense;sparse;random;
                 random_dense;random_sparse;cpp
```

No-component lookup requests `cpp`. Unknown required components fail.
Provider components remain unimplemented and unexported.

## External dependencies and providers

Milestone 5 adds no compile-time, link-time, or runtime third-party
dependency. Root integration continues to require exact released ASCCMake
0.1.0 and uses standard CMake for interface target export and conditional
component loading.

There is no provider option, discovery, SDK include, language enablement,
compiled provider target, dispatch, transfer, synchronization, or fallback.
GPU evidence is exactly **skipped**.

Final source/include and configured/installed-target scans pass. The exported
direct edges are exactly the three edges above. Across each of the clean GCC
Debug/static, Clang Release/shared, and minimum-CMake Release/static matrices,
all 22 package/consumer tests pass, including copied build-tree packages,
subproject use, installation, relocation through paths containing spaces,
isolated base/facet/aggregate consumers, no-component lookup, unknown
component behavior, and package-registry preservation. Provider/provenance
source scans return no match.
