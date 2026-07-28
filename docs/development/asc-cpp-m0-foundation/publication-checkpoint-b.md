# Milestone 0 Publication Checkpoint B

Status: Complete local candidate; publication not authorized

Date: 2026-07-27

Branch: `feature/asc-cpp-m0-foundation`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

## Scope result

The bounded Milestone 0 contract is satisfied. The candidate is a target-free
architecture, repository, build, package, test, documentation, and CI
foundation. It contains no production C++ header or source, library,
executable, component target, provider target, compatibility facade,
benchmark, or later-milestone implementation.

The cumulative Milestone 8 branch and its draft pull request were preserved
unchanged. No commit, push, pull-request mutation, merge, tag, release, or
branch deletion occurred during this milestone.

## Changed files and reasons

The staged candidate contains 288 changed paths: 54 additions, 32
modifications, and 202 deletions. The exact per-path inventory is produced by:

```sh
git diff --cached --name-status main
```

The bounded groups and reasons are:

- `.clang-format` and `.clang-tidy`: add strict C++20 Google-derived
  formatting and analysis policy for future production work.
- `CMakeLists.txt` and `CMakePresets.json`: replace the legacy C++ project
  with the target-free `ASCCpp 0.0.0` foundation and five presets.
- `cmake/ASCCppComponents.cmake`, `cmake/ASCCppOptions.cmake`, and
  `cmake/ASCCppConfig.cmake.in`: define the nine provider-free future names,
  top-level/subproject options, and negative relocatable package.
- `.github/workflows/ci.yml`: replace unpinned legacy CI with
  least-privilege, immutable-checkout Linux, Windows, and macOS jobs.
- `docs/development/asc-cpp-architecture/**`: add the approved package,
  18 ADRs, manifests, backend matrix, strategies, provenance, roadmap, and
  M0 plan.
- `docs/development/asc-cpp-m0-foundation/**`: add the frozen contract,
  ownership ledger, four role reports, and this checkpoint.
- `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, and
  `docs/README.md`: replace legacy live claims with target-free guidance.
- The 24 historical documents named in `implementation-plan.md`: add one
  normalized nine-line banner; the bodies remain byte-identical.
- `tests/CMakeLists.txt`, `tests/architecture/**`, and `tests/package/**`:
  replace legacy product tests with six architecture and negative-package
  tests.
- `include/asc/**`, `src/**`, legacy tests, examples, config generation, old
  CMake helpers, `AGENTS.md`, `generator.md`, `CPPLINT.cfg`, and
  `THIRD_PARTY_NOTICES`: preserve the approved clean restart and provenance
  disposition by removing the deleted implementation and unsupported claims.

`LICENSE` and `.gitignore` are unchanged. The ignored pre-existing `build/`
directory contains cumulative work from another branch, was not modified, and
is excluded from all M0 evidence.

## APIs, targets, and direct dependencies

### Public C++ surface

None. There is no file below `include/asc` or `src`.

### Build and imported targets

None. In particular, no `asc_*`, `ASC::*`, executable, library, export set, or
provider target exists.

### CMake and package surface

- Project/package: `ASCCpp`, unreleased version `0.0.0`.
- Options: `ASC_CPP_BUILD_TESTING` and `ASC_CPP_INSTALL`.
- Package variables: `ASCCpp_VERSION`, `ASCCpp_KNOWN_COMPONENTS`,
  `ASCCpp_AVAILABLE_COMPONENTS`, and requested component `_FOUND` variables.
- Known future package components: `core`, `utilities`, `expression`, `dense`,
  `sparse`, `random`, `random_dense`, `random_sparse`, and `cpp`.
- Available components: none.
- No-component lookup is an implicit required `cpp` request and fails.
- Known, unknown, required, optional-only, and no-component lookups all report
  the M0 package unavailable and create no target.

### Direct dependency changes

- Build dependency: exact released `ASCCMake 0.1.0` at
  `8a7dcbad3a97267cce59810aff24de800a3497a7`.
- Production, public, target, runtime, provider, and test-library
  dependencies: none.
- CI action: `actions/checkout` pinned to
  `3d3c42e5aac5ba805825da76410c181273ba90b1`.
- No unapproved dependency was added.

No ASCCMake product-target helper is invoked because M0 creates no target.
The package uses standard `CMakePackageConfigHelpers`, as approved by the
ASCCMake consumption review.

## Exact commands and results

### Repository and dependency preflight

```sh
git fetch --prune origin
git rev-parse main origin/main feature/asc-cpp-m0-foundation
git -C /home/yicai/AI4SciComp/asc-cmake rev-parse HEAD
git -C /home/yicai/AI4SciComp/asc-cmake describe --tags --exact-match HEAD
```

Result: passed. `main`, `origin/main`, and the M0 branch base were
`33b261e`; ASCCMake was clean `v0.1.0` at `8a7dcba`.

### Final current-CMake Debug validation

```sh
cmake -S /home/yicai/AI4SciComp/asc-cmake \
  -B "/tmp/asc-cpp-m0-final.1lBPj0/asc cmake" \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m0-final.1lBPj0/debug build with spaces" \
  -DASCCMake_DIR="/tmp/asc-cpp-m0-final.1lBPj0/asc cmake" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-m0-final.1lBPj0/debug build with spaces" --parallel 2

ctest --test-dir \
  "/tmp/asc-cpp-m0-final.1lBPj0/debug build with spaces" \
  --output-on-failure
```

Result: passed, 6/6 tests, 0 failed, 0 skipped.

### Minimum-CMake Release validation

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cmake \
  -B /tmp/asc-cpp-m0-minimum.bAltNB/asc-cmake \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cpp \
  -B /tmp/asc-cpp-m0-minimum.bAltNB/release \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-minimum.bAltNB/asc-cmake \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Release

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build /tmp/asc-cpp-m0-minimum.bAltNB/release --parallel 2

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir /tmp/asc-cpp-m0-minimum.bAltNB/release \
  --output-on-failure
```

Result: passed, 6/6 tests, 0 failed, 0 skipped.

Independent verification additionally ran fresh CMake 4.1.2 Debug and Release
builds at `/tmp/asc-cpp-m0-verification.HuynGS`; both passed 6/6.
Independent portability repeated CMake 4.1.2 Debug and CMake 3.25.0 Release
from paths containing spaces; both passed 6/6.

### Install, package, relocation, and consumers

```sh
cmake --install \
  "/tmp/asc-cpp-m0-final.1lBPj0/debug build with spaces" \
  --prefix \
  "/tmp/asc-cpp-m0-final.1lBPj0/manual install prefix with spaces"
```

Result: passed. The manual prefix contains exactly:

```text
lib/cmake/ASCCpp/ASCCppConfig.cmake
lib/cmake/ASCCpp/ASCCppConfigVersion.cmake
share/doc/ASCCpp/LICENSE
```

CTest package cases passed for:

- the original build-tree package;
- a copied build-tree package in a different path containing spaces;
- the installed package;
- the complete installed prefix renamed to another path containing spaces;
- every known future component, an unknown component, optional-only `core`,
  and no component; and
- an isolated user package-registry snapshot.

These are negative isolated consumers because M0 intentionally exports no
usable component.

### Subproject isolation

```sh
cmake -S /tmp/asc-cpp-m0-subproject-probe \
  -B /tmp/asc-cpp-m0-subproject-build.MkKv7x \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-integration-fixed.IOMnP5/asc-cmake
cmake --build /tmp/asc-cpp-m0-subproject-build.MkKv7x --parallel 2
```

Result: passed. `ASC_CPP_BUILD_TESTING=OFF`, `ASC_CPP_INSTALL=OFF`, and no
ASCCpp-created `CMAKE_INSTALL_LIBDIR`, `CMAKE_INSTALL_DOCDIR`, or
`CMAKE_INSTALL_DATAROOTDIR` cache entry exists.

### Policy, manifest, documentation, and diff gates

```sh
clang-format-19 --style=file --assume-filename=probe.cc --dump-config
clang-format-19 --style=file --assume-filename=probe.h --dump-config
cmake --list-presets=all
git diff --cached --check
```

Result: passed. Five configure presets, five build presets, and three test
presets were reported. YAML parsing, live-document links and fences, all
24 normalized banners, historical body preservation, and scoped whitespace
checks passed.

Clang-tidy executable validation was skipped locally because no clang-tidy
binary is installed. Hosted CI statically validates `.clang-tidy` with
Clang 18. No translation-unit analysis is possible in target-free M0.

## Sanitizer and runtime evidence

- AddressSanitizer: `skipped`.
- UndefinedBehaviorSanitizer: `skipped`.
- ThreadSanitizer: `skipped`.
- LeakSanitizer: `skipped`.

Reason: no C++ translation unit, executable, or production target exists.
These are not sanitizer passes.

## CPU, GPU, and provider evidence

CPU provider evidence: not applicable. No serial memory, execution, or
numerical provider exists.

GPU evidence classification: `skipped`.

No GPU language, option, discovery, dependency, source, target, export,
compile, runtime operation, or parity oracle exists. Local toolkit or hardware
inventory is not ASCCpp provider evidence. M0 is not configure-tested,
compile-tested, runtime-tested, or parity-tested for GPU.

## Independent findings and resolutions

1. Verification initially treated the six future provider facets as live M0
   package components. Three of six integration tests failed. Tests were
   corrected to retain provider-facet manifest validation while requiring the
   live package to expose only the nine provider-free future names. Debug and
   Release then passed 6/6.
2. Documentation found one retained historical-body link from
   `docs/migration/inventory.md` to the intentionally deleted
   `THIRD_PARTY_NOTICES`. It is accepted informational evidence under the
   body-preservation and provenance contract. All live, new, and banner links
   pass; the notice was not restored.
3. Portability found package relocation and subproject isolation passed at
   both CMake endpoints. No corrective source change was requested.
4. Hosted access to private asc-cmake requires `ASC_CMAKE_READ_TOKEN`. This is
   an open external publication prerequisite, not a repository correction.

No unresolved production, package/API, verification, documentation,
portability, GPU-isolation, or performance blocker remains locally.

## Performance evidence

Not applicable. There is no operation, allocation, packing, transfer,
densification, synchronization, or numerical kernel to measure. Adding a
benchmark would implement a later-milestone surface.

## Remaining risks

- The unpublished branch has not run the hosted Linux Clang, Windows MSVC, or
  macOS AppleClang jobs. Static workflow review is not execution evidence.
- `ASC_CMAKE_READ_TOKEN` is absent from the current repository Actions
  configuration and must be provisioned as a least-privilege read credential
  before hosted CI can pass.
- Windows user-package-registry behavior is statically reviewed but not
  locally executed.
- The ignored local `build/` tree contains cumulative later-milestone
  artifacts. It is preserved unrelated work and must not be cited as M0
  evidence.
- Clang-tidy translation-unit analysis, C++ compiler conformance, warnings,
  sanitizers, static/shared linkage, providers, and performance become
  testable only in later approved milestones with real targets.

## Proposed remote and branch-cleanup actions

No remote or cleanup action is authorized at Checkpoint B.

After separate publication approval and after provisioning
`ASC_CMAKE_READ_TOKEN`, the exact proposed publication commands are:

```sh
git commit -m "Establish asc-cpp milestone 0 foundation"
git push --set-upstream origin feature/asc-cpp-m0-foundation
gh pr create \
  --base main \
  --head feature/asc-cpp-m0-foundation \
  --title "Establish asc-cpp milestone 0 foundation" \
  --body-file docs/development/asc-cpp-m0-foundation/publication-checkpoint-b.md
```

The staged candidate should be committed without amendment after approval.
No tag is proposed because the roadmap states that M0 is not a library
release.

The existing draft cumulative M8 pull request and
`feature/asc-cpp-m8-hardening-downstream` branch should remain untouched
unless the owner separately decides their disposition. No local or remote
branch is currently proposed for deletion. Before any later cleanup, audit
merge ancestry, unique commits, and worktree occupancy one branch at a time.
