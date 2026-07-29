# Milestone 0 independent verification review

Status: Locally validated with CMake 4.1.2; hosted portability pending

Date: 2026-07-27

## Independent basis

The verification suite was derived before inspection of the lead-owned build
implementation. Its sources were the frozen Milestone 0 contract, ownership
ledger, dependency and capability manifests, backend matrix, testing strategy,
implementation plan, and approved ADRs.

The suite does not restore, translate, or adapt a deleted historical test. It
tests only the Milestone 0 architecture and repository foundation.

## Implemented falsification

The architecture scripts independently parse the checked-in manifest text
with CMake 3.25 script logic and require:

- the exact six-module graph and its seven direct edges;
- the exact two random integration facets and `cpp` aggregate closures;
- all six future provider facets as planning metadata, with exact direct
  edges and owners;
- exact build and installed target names in the manifest;
- every capability owner, facet, and direct dependency to agree with the
  graph; and
- the live Milestone 0 component variables to expose only the nine
  provider-free known components.

Separate tests reject a production file below `include/asc` or `src`, retired
five-module paths, an executable or library target, an `ASC::*` target, an
`asc_*` product target, a package target export, or `export(PACKAGE)`.
The target inventory is deferred until the end of top-level processing so a
later directory cannot evade it.

Package consumers inspect the configured build tree, a copied build-tree
package under a path containing spaces, an installed prefix, and the same
prefix after relocation. For each provider-free known component, an unknown
component, and no component, both quiet inspection and required failure are
checked. Every successful inspection requires:

- `ASCCpp_FOUND=FALSE`;
- package version `0.0.0`;
- no available component;
- the exact provider-free known-component list; and
- no created or imported `ASC::*` target.

An `OPTIONAL_COMPONENTS core` case also requires package-level failure. The
registry test scans for prohibited `export(PACKAGE)` calls and requires a
fresh nested configure to leave an isolated user-package registry unchanged.

## Resolved reconstruction finding

The first integrated run failed three of six cases because the verification
draft treated future provider facets as live Milestone 0 package components.
That contradicted the frozen boundary: provider facets exist in Stage A
planning metadata, while Milestone 0 has no provider option, discovery,
component, target, source, or runtime claim.

The tests were corrected to retain exact provider-facet manifest validation
but require no live `ASC_CPP_PROVIDER_COMPONENTS` and no provider name in
`ASC_CPP_KNOWN_COMPONENTS`. Debug and Release then passed in full.

## Local environment

| Item | Evidence |
| --- | --- |
| Host | Linux WSL2 x86_64, kernel 6.18.33.2 |
| CMake | 4.1.2 |
| Generator | Unix Makefiles, GNU Make 4.3 |
| ASCCMake | `v0.1.0` at `8a7dcbad3a97267cce59810aff24de800a3497a7` |
| Ninja | not installed |
| Clang | not installed |

## Exact local commands and results

All build directories were fresh and outside the repository:

```text
source_dir=/home/yicai/AI4SciComp/asc-cpp
verification_root=/tmp/asc-cpp-m0-verification.HuynGS
asccmake_dir=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake

cmake -S "$source_dir" \
  -B "$verification_root/debug-make" \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$asccmake_dir" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build "$verification_root/debug-make" --config Debug
ctest --test-dir \
  "$verification_root/debug-make" \
  -C Debug --output-on-failure

cmake -S "$source_dir" \
  -B "$verification_root/release-make" \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$asccmake_dir" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "$verification_root/release-make" --config Release
ctest --test-dir \
  "$verification_root/release-make" \
  -C Release --output-on-failure

architecture_dir="$source_dir/docs/development/asc-cpp-architecture"
cmake \
  -DSOURCE_DIR:PATH="$source_dir" \
  -DDEPENDENCY_MANIFEST:FILEPATH="$architecture_dir/dependency-manifest.yaml" \
  -DCAPABILITY_MANIFEST:FILEPATH="$architecture_dir/capability-manifest.yaml" \
  -P tests/architecture/check_dependency_manifest.cmake
cmake \
  -DSOURCE_DIR:PATH="$source_dir" \
  -P tests/architecture/check_public_file_policy.cmake
! rg -n '[[:blank:]]+$' tests \
  docs/development/asc-cpp-m0-foundation/verification-review.md
```

Results:

- Debug configure/build: passed;
- Debug CTest: 6/6 passed, 0 failed, 0 skipped;
- Release configure/build: passed;
- Release CTest: 6/6 passed, 0 failed, 0 skipped;
- direct dependency/capability manifest validation: passed;
- direct public-file policy validation: passed; and
- scoped trailing-whitespace validation: passed.

Both package matrices include build-tree, copied-build-tree, installed, and
relocated consumers. The isolated registry check passed in both
configurations.

## Evidence boundaries

Milestone 0 contains no production C++, numerical operation, provider target,
or GPU language. Sanitizer, numerical, provider compile, GPU runtime, parity,
and performance evidence is therefore skipped, not passed.

CMake 3.25 endpoint execution, MSVC multi-config execution, hosted
GCC/Clang/AppleClang jobs, and CI access to private ASCCMake remain lead or
hosted integration work.
