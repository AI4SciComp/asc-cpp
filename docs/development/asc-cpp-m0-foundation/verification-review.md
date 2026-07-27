# Milestone 0 independent verification review

Status: Locally validated with CMake 4.1.2; hosted portability validation pending

## Independent basis

The verification suite was derived from the frozen Milestone 0 contract,
dependency and capability manifests, testing strategy, implementation plan,
and ADRs 0001, 0002, 0003, and 0018. It does not restore or adapt deleted
historical tests.

The suite assumes the lead-owned root build:

- configures project `ASCCpp` version `0.0.0` with `LANGUAGES NONE`;
- exposes the configured build-tree package directory as
  `ASC_CPP_BUILD_PACKAGE_DIR`;
- leaves `ASCCMake_DIR` available after the exact ASCCMake 0.1.0 lookup;
- installs the package skeleton when `ASC_CPP_INSTALL=ON`; and
- adds `tests` only when both CTest and `ASC_CPP_BUILD_TESTING` are enabled.

Target inventory is deferred until the end of the top-level directory, so its
result does not depend on the location of `add_subdirectory(tests)`.

## Implemented falsification

The architecture tests independently parse the checked-in YAML text with
CMake 3.25 script logic. They require exactly:

- six modules and their seven direct edges;
- the two random integration facets and the `cpp` aggregate with their exact
  closures;
- the six approved future provider facets and their exact direct edges;
- matching build/imported target names and facet owners;
- the exact provider-free component list;
- all capability owners, facets, and direct dependencies to agree with that
  graph; and
- the live component-variable graph to agree with the manifest.

Separate tests reject live production C++ files, retired public paths,
libraries, executables, target exports, imported `ASC::*` targets, or
`export(PACKAGE)` use.

Package consumers exercise the configured build tree, a non-destructive copy
of that build-tree package under a new path containing spaces, an installed
prefix, and the same prefix after relocation to another path containing
spaces. For every known provider-free component, plus an unknown component
and no component, the suite checks both non-required inspection and required
failure. Every inspection requires `ASCCpp_FOUND=FALSE`, version `0.0.0`, no
available component, and no created or imported `ASC::*` target. A dedicated
`OPTIONAL_COMPONENTS core` case also requires package-level failure, covering
the documentation/API review edge case.

The registry test combines a source-level rejection of `export(PACKAGE)` with
a fresh nested configure. It snapshots an isolated filesystem registry on
Unix-like hosts and the CMake user-package registry on Windows, configures with
registry export disabled, and requires an identical post-configure snapshot.

## Local validation evidence

The local dependency was ASCCMake `v0.1.0` at
`8a7dcbad3a97267cce59810aff24de800a3497a7`. CMake was 4.1.2. Ninja was not
installed, so the available Unix Makefiles generator was used. Both build
directories were fresh and outside the repository:

```text
cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B /tmp/asc-cpp-m0-verification.qlsPDz/debug-make \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/asc-cpp-m0-verification.qlsPDz/debug-make --config Debug
ctest --test-dir /tmp/asc-cpp-m0-verification.qlsPDz/debug-make \
  -C Debug --output-on-failure

cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B /tmp/asc-cpp-m0-verification.qlsPDz/release-make \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/asc-cpp-m0-verification.qlsPDz/release-make --config Release
ctest --test-dir /tmp/asc-cpp-m0-verification.qlsPDz/release-make \
  -C Release --output-on-failure

git diff --check -- tests \
  docs/development/asc-cpp-m0-foundation/verification-review.md
```

The final post-fix result was 6/6 tests passed in Debug and 6/6 in Release,
with no skips. The no-production-target test completed in 0.01 seconds in both
runs. The build steps succeeded with no production build target, and the
scoped `git diff --check` completed without a diagnostic.

The dependency-manifest and public-file-policy scripts were also invoked
directly with `cmake -P` before the integrated runs and completed
successfully.

## Pending integration evidence

CMake 3.25 endpoint execution, Windows/MSVC multi-config behavior, hosted
GCC/Clang/AppleClang jobs, Markdown/YAML validation outside the CMake scripts,
and CI access to private ASCCMake remain lead/hosted integration work. No claim
is made for those environments here.

## Evidence boundaries

Milestone 0 contains no production C++, numerical implementation, provider
target, or GPU language. Therefore this review makes no sanitizer, numerical,
compile-provider, GPU runtime, parity, or performance claim. Hosted compiler
and multi-config evidence remains an integration/CI responsibility.
