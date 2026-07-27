# Milestone 0 portability, GPU, and performance review

Status: Complete independent review on 2026-07-26

Scope: complete Milestone 0 diff, with this report as the reviewer's only
writable repository path

## Conclusion

No unresolved portability, GPU-isolation, or performance release blocker was
found in the integrated Milestone 0 scope.

Three integration defects were found during review and corrected in their owned
scopes:

1. the build-tree package config initially computed an incorrect
   `PACKAGE_PREFIX_DIR`; and
2. the analysis policy initially named a Clang 19 check while hosted CI was
   based on Clang 18; and
3. the root project initially polluted an embedding project's install-directory
   cache even when asc-cpp installation was disabled.

Fresh independent CMake 4.1.2 and minimum CMake 3.25.0 runs passed all six
architecture/package tests after the first two corrections. The integration
lead additionally verified the third correction with a fresh subproject
configure whose cache contains no asc-cpp-created `CMAKE_INSTALL_LIBDIR`,
`CMAKE_INSTALL_DOCDIR`, or `CMAKE_INSTALL_DATAROOTDIR` entry. Hosted Windows,
macOS, and Clang execution remains pending because this branch has not been
published.
The private asc-cmake read credential remains an external hosted-CI
prerequisite.

## Reviewed contract

Milestone 0 is an architecture and repository foundation:

- project/package identity is `ASCCpp` version `0.0.0`;
- the project uses `LANGUAGES NONE`;
- there is no public C++ file, product target, export, provider, or numerical
  implementation;
- exact `ASCCMake 0.1.0` is required;
- all component and no-component package requests fail cleanly;
- package generation, installation, relocation, and negative consumers are in
  scope; and
- provider discovery, CUDA language enablement, sanitizer runtime evidence,
  numerical evidence, and benchmarks are prohibited or inapplicable.

The live component vocabulary and manifest preserve exactly six modules,
two random-owned storage facets, and the future `cpp` aggregate. They do not
create an additional module or a live target.

## CMake and package review

The root CMake:

- requires CMake 3.25 and exact `ASCCMake 0.1.0`;
- uses only standard CMake package helpers because no released asc-cmake
  product-target helper applies to a target-free milestone;
- defines no target, language, compiler flags, provider option, or provider
  lookup;
- derives local, non-cache install directories while honoring caller-provided
  `CMAKE_INSTALL_LIBDIR`, `CMAKE_INSTALL_DOCDIR`, or
  `CMAKE_INSTALL_DATAROOTDIR`;
- does not include `GNUInstallDirs` or create install-directory cache entries
  in an embedding project;
- defaults testing/install on only as a top-level project and defaults both
  off as a subproject;
- generates no target export and does not call `export(PACKAGE)`; and
- installs only config, version, and Apache-2.0 license files.

`ASCCppConfig.cmake` creates no target and leaves `ASCCpp_FOUND=FALSE` for
no-component, optional, required, known-unavailable, and unknown requests.
The package fixture exercises all nine provider-free component names, an
unknown component, optional `core`, and no-component `cpp` behavior.

The build-tree config now uses:

```cmake
INSTALL_DESTINATION "."
```

Its generated prefix is:

```cmake
get_filename_component(
  PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/" ABSOLUTE
)
```

The fixture copies that package to a different path containing spaces and
repeats every negative consumer case. The install fixture independently
installs, consumes, renames the complete prefix to a path containing spaces,
and consumes it again. No `*Targets.cmake` or binary artifact was emitted.

## Portability matrix

| Area | Evidence | Result |
| --- | --- | --- |
| CMake minimum | CMake/CTest 3.25.0, Unix Makefiles, Release | 6/6 passed |
| CMake current host | CMake/CTest 4.1.2, Unix Makefiles, Debug | 6/6 passed |
| Path spaces | producer, consumer, copied build package, install, and relocated prefix paths | passed |
| Presets | CMake 3.25.0 and 4.1.2 `--list-presets=all` | five configure, five build, three test presets parsed |
| Subproject policy | script trace plus the lead's fresh embedding-project configure | testing `OFF`, install `OFF`; no asc-cpp-created install-directory cache entries |
| Top-level policy | script trace with `BUILD_TESTING=OFF` | testing follows `OFF`, install `ON` |
| Clang format baseline | clang-format 18.1.8 config parse for `.cc` and `.h` | passed |
| Linux compiler | GCC 11.4 detected, but no C++ language or translation unit exists | compilation not applicable |
| Clang | hosted Ubuntu 24.04/Clang 18 workflow reviewed | not run locally |
| MSVC | VS 2022 x64 multi-config PowerShell workflow reviewed | not run locally |
| AppleClang | `macos-15` arm64 workflow and quoted paths reviewed | not run locally |
| Static/shared | no library target or linkage mode exists | not applicable |
| Package registry | isolated Linux filesystem registry snapshot | unchanged |
| Windows registry | CMake 3.25-compatible registry query logic reviewed | runtime not run |

The Visual Studio job supplies `-A x64`, builds/tests both Debug and Release,
passes `-C` through CTest, and passes the selected configuration into nested
installation. PowerShell continuations and the quoted ASCCMake path are
consistent with Windows path handling. The registry query API used by the
fixture predates the project's CMake 3.25 minimum.

The `macos-15` label currently denotes a supported arm64 GitHub-hosted image.
The workflow uses no GNU-only build command and quotes package paths.

Because the project deliberately uses `LANGUAGES NONE`, job compiler names
describe the hosted environment and do not constitute a C++ compiler test.
That is honest for Milestone 0; real C++20/warning/sanitizer evidence begins
with a milestone that owns a translation unit and target.

## Exact independent commands and results

Current-CMake Debug validation used fresh external directories:

```sh
cmake -S /home/yicai/AI4SciComp/asc-cmake \
  -B /tmp/asc-cpp-portability-resolved-current.cYatLn/asc-cmake \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-portability-resolved-current.cYatLn/asc cpp debug build" \
  -DASCCMake_DIR=/tmp/asc-cpp-portability-resolved-current.cYatLn/asc-cmake \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-portability-resolved-current.cYatLn/asc cpp debug build" \
  --parallel 2

ctest --test-dir \
  "/tmp/asc-cpp-portability-resolved-current.cYatLn/asc cpp debug build" \
  --output-on-failure
```

Result: 6 discovered, 6 passed, 0 failed, 0 skipped.

Minimum-CMake Release validation used the existing external CMake 3.25.0
environment:

```sh
/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cmake \
  -B /tmp/asc-cpp-portability-resolved-cmake325.LLOvrT/asc-cmake \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-portability-resolved-cmake325.LLOvrT/asc cpp release build" \
  -DASCCMake_DIR=/tmp/asc-cpp-portability-resolved-cmake325.LLOvrT/asc-cmake \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Release

/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake --build \
  "/tmp/asc-cpp-portability-resolved-cmake325.LLOvrT/asc cpp release build" \
  --parallel 2

/tmp/asc-cmake-3.25.pTlOcP/venv/bin/ctest --test-dir \
  "/tmp/asc-cpp-portability-resolved-cmake325.LLOvrT/asc cpp release build" \
  --output-on-failure
```

Result: 6 discovered, 6 passed, 0 failed, 0 skipped.

The formatting-policy baseline used:

```sh
/tmp/asc-cpp-clang-format18.vbbpAX/venv/bin/clang-format \
  --style=file --assume-filename=probe.cc --dump-config

/tmp/asc-cpp-clang-format18.vbbpAX/venv/bin/clang-format \
  --style=file --assume-filename=probe.h --dump-config
```

Both clang-format 18.1.8 parses passed. Local clang-tidy 18 was unavailable,
so its configuration validator was not claimed locally. Hosted CI now runs:

```sh
clang-tidy-18 --config-file=.clang-tidy --verify-config
```

before explicitly reporting that translation-unit analysis is skipped.

## Findings and resolutions

### PORT-001: incorrect build-tree package prefix

Severity: release-blocking before correction

Status: resolved

The first package implementation passed the absolute build-package directory
as `INSTALL_DESTINATION`. The generated build config consequently resolved
`PACKAGE_PREFIX_DIR` to `/usr/local`. Although Milestone 0 did not yet consume
that variable, it contradicted the build-tree relocatability foundation and
would make later path variables unsafe.

The lead changed build-tree generation to `INSTALL_DESTINATION "."`. The
verification engineer added copied-build-package path-with-spaces consumers.
Both CMake versions now generate the prefix from `CMAKE_CURRENT_LIST_DIR`, and
all copied-package cases pass.

### PORT-002: Clang analysis version mismatch

Severity: release-blocking before correction

Status: resolved

The initial `.clang-tidy` explicitly enabled
`misc-use-internal-linkage`, but LLVM documents that check as new in Clang 19,
while hosted CI installs Clang 18. The foundation engineer removed the
unsupported explicit check and bound the review to LLVM's published 18.1
inventory. The lead added hosted `--verify-config` validation so an unknown
check or option fails even before a translation unit exists.

All checked `.clang-format` options are present in LLVM 18, and the exact
18.1.8 formatter parsed the file successfully.

### PORT-003: subproject install-directory cache pollution

Severity: release-blocking before correction

Status: resolved

The initial root project unconditionally assigned
`CMAKE_INSTALL_LIBDIR` as a cache variable and included `GNUInstallDirs`.
That modified an embedding project's cache even when
`ASC_CPP_INSTALL=OFF`, violating the Milestone 0 subproject-isolation
contract.

The lead replaced those global/cache mutations with private local variables.
They honor caller-provided install-directory values and otherwise select
`lib` and `share/doc/ASCCpp` without publishing cache entries. A fresh
embedding-project configure completed with asc-cpp testing and installation
disabled, and its `CMakeCache.txt` contains none of
`CMAKE_INSTALL_LIBDIR`, `CMAKE_INSTALL_DOCDIR`, or
`CMAKE_INSTALL_DATAROOTDIR`. Top-level installation still places package
metadata under `lib/cmake/ASCCpp` and the license under
`share/doc/ASCCpp`.

### PORT-004: private asc-cmake hosted access

Severity: external publication prerequisite

Status: open outside the repository

The workflow checks out the exact private asc-cmake commit with
`ASC_CMAKE_READ_TOKEN`, minimum `contents: read` workflow permission, a
full-commit-pinned checkout action, and `persist-credentials: false`.
The repository owner must configure a least-privilege read credential before
hosted CI can pass. Fork and Dependabot pull requests do not receive ordinary
secrets by default; no privileged `pull_request_target` workaround should be
introduced to execute untrusted code.

## CPU/GPU/provider evidence

The host exposes:

- CUDA compiler/toolkit 12.9.86;
- NVIDIA GeForce RTX 3060 Laptop GPU, compute capability 8.6;
- driver 576.83;
- CUDA runtime, cuBLAS, cuSOLVER, cuSPARSE, and cuRAND libraries;
- system BLAS/LAPACK; and
- no HIP/ROCm compiler or runtime.

This inventory is not asc-cpp provider evidence. The clean CMake cache contains
no CXX, CUDA, CUDAToolkit, BLAS, LAPACK, OpenMP, Eigen, MKL, TBB, HIP, ROCm, or
SYCL discovery entry. No provider source, option, target, export, compile,
runtime operation, or parity oracle exists.

GPU evidence classification: **skipped**.

Reason: Milestone 0 explicitly prohibits provider discovery and implementation.
It is not configure-tested, compile-tested, runtime-tested, or parity-tested.

CPU provider evidence is likewise not applicable: no serial numerical or
memory provider is implemented in this milestone.

## Sanitizer and performance evidence

Sanitizer execution is skipped because there is no C++ translation unit,
executable, or production target to instrument. This is not a sanitizer pass.

Performance evidence is not applicable. There is no operation, allocation,
packing, transfer, densification, synchronization, or numerical kernel to
measure. Adding a benchmark would fabricate a later-milestone surface.

## License, dependency, and provenance review

- `LICENSE` remains the unchanged Apache License 2.0.
- No deleted notice, MdeCpp source/test, provider SDK material, generated
  table, or third-party production dependency was restored.
- The only build dependency is exact released ASCCMake 0.1.0.
- The workflow action is pinned to full commit
  `3d3c42e5aac5ba805825da76410c181273ba90b1`.
- Optional provider names remain planning metadata only and do not leak into
  the live package component list or configuration.

## Remaining risks

- The hosted GCC/Clang/MSVC/AppleClang matrix has not run on this unpublished
  branch. Static Windows/macOS review is not runtime evidence.
- `ASC_CMAKE_READ_TOKEN` must be configured and audited before publication.
- Clang-tidy analysis, compiler C++20 conformance, warning flags, sanitizers,
  static/shared linkage, and provider isolation must be exercised when a later
  approved milestone introduces real targets.
- The local host has no MSVC, AppleClang, Clang compiler, clang-tidy, Ninja,
  HIP, or ROCm runtime, so none is reported as locally passed.

These are honest evidence limits or later-milestone obligations, not reasons
to add production code to Milestone 0.

## Authoritative portability references

- [CMake package configuration helpers](https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html)
- [CMake host-system information](https://cmake.org/cmake/help/latest/command/cmake_host_system_information.html)
- [LLVM 18.1.8 clang-format options](https://releases.llvm.org/18.1.8/tools/clang/docs/ClangFormatStyleOptions.html)
- [LLVM 18.1.8 clang-tidy checks](https://releases.llvm.org/18.1.8/tools/clang/tools/extra/docs/clang-tidy/checks/list.html)
- [LLVM 19.1 clang tools release notes](https://releases.llvm.org/19.1.0/tools/clang/tools/extra/docs/ReleaseNotes.html)
- [GitHub-hosted runner images](https://github.com/actions/runner-images)
- [GitHub Actions secret behavior](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows)
