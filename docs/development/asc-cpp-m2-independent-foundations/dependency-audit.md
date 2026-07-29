# Milestone 2 dependency audit

Status: passed at Publication Checkpoint B

## Audited graph

Milestone 2 adds three provider-free targets to the Milestone 1 Core target:

```text
build target       package target     kind       direct ASC dependency
asc_core           ASC::core          compiled   none
asc_utilities      ASC::utilities     compiled   ASC::core
asc_expression     ASC::expression    interface  ASC::core
asc_random         ASC::random        compiled   ASC::core
```

Utilities, Expression, and Random have no direct or transitive dependency on
one another. They have no Dense, Sparse, random-storage-facet, provider-SDK, or
GPU dependency. The only configure-time project dependency remains exact
`ASCCMake 0.1.0`; production uses only the C++20 standard library and Core.
No third-party runtime dependency was added.

`asc_utilities` and `asc_random` use the released
`asc_target_enable_cxx20`, `asc_target_enable_warnings`, and
`asc_target_enable_sanitizers` APIs. Released ASCCMake 0.1.0 deliberately
rejects interface libraries in `asc_target_enable_cxx20`, so
`asc_expression` uses the equivalent standard-CMake
`target_compile_features(... INTERFACE cxx_std_20)` contract with extensions
disabled.

## Package and component audit

Build-tree and install-tree packages export exactly:

```text
core utilities expression random
```

Requesting Utilities, Expression, or Random imports Core first and then only
the requested component target. A Core-only lookup does not import a sibling.
Dense, Sparse, `random_dense`, `random_sparse`, and `cpp` remain unavailable
and unexported. Unknown required components and unavailable required
components fail; an unavailable optional component reports false without
invalidating an otherwise valid request. A no-component lookup continues to
request the unavailable `cpp` aggregate and fails by design.

The installed compiled M2 libraries retain an origin-relative dependency on
Core: `$ORIGIN` on ELF and `@loader_path` on Apple. This is required because an
executable's ELF `RUNPATH` is not inherited when a compiled component loads
its transitive `libasc_core.so` dependency. No RPATH is added on Windows.

## Mechanical evidence

The configured test graph asserts the exact target edges and scans all ten M2
public headers and four compiled production sources for forbidden includes,
provider names, standard random engines/distributions, implementation-header
leakage, and public exception behavior.

```sh
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m2_dependency_check.cmake
git diff --check
```

Result: pass; no forbidden dependency or whitespace error.

Both final clean configurations ran
`asc_cpp.architecture.dependency_manifest`,
`asc_cpp.architecture.approved_product_targets`,
`asc_cpp.architecture.public_file_policy`,
`asc_cpp.compile.dependency_audit`, and
`asc_cpp.compile.m2_dependency_audit` successfully:

```text
CMake 3.25.0 / GCC 11.4 / Release / static / Werror: pass, 79/79
CMake 4.1.2 / Clang 19 / Debug / shared / Werror: pass, 79/79
```

Each isolated Utilities, Expression, and Random consumer configured, built,
ran from the build tree, installed to a path containing spaces, relocated, and
ran from the relocated tree. The complete subproject consumer and package
component tests also passed. All 12 package-labelled tests passed in each
final configuration, and the user package registry remained unchanged.

The portability reviewer additionally inspected the relocated shared
libraries:

```text
libasc_utilities.so: NEEDED libasc_core.so; RUNPATH [$ORIGIN]
libasc_random.so:    NEEDED libasc_core.so; RUNPATH [$ORIGIN]
```

## Result and limits

The frozen direct dependency graph and component-isolation contract pass.
There is no unapproved dependency. Provider and GPU targets remain absent, so
GPU dependency evidence is **skipped** rather than configure-, compile-,
runtime-, or parity-tested.

