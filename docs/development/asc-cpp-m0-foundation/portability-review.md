# Milestone 0 portability, GPU, and performance review

Status: Complete independent review on 2026-07-27

Scope: complete current Milestone 0 diff, with this report as the reviewer's
only writable repository path

## Conclusion

No unresolved portability, GPU-isolation, or performance blocker was found in
the bounded Milestone 0 implementation. No correction is requested.

Fresh external builds passed with current CMake 4.1.2 and minimum CMake 3.25.0.
Both used producer build paths containing spaces and passed all six
architecture/package tests. The tests include copied build-tree consumption,
installation, relocation to a prefix containing spaces, negative isolated
consumers, and user package-registry isolation.

Fresh embedding-project configurations also passed with both CMake versions.
ASCCpp testing and installation defaulted off, and no ASCCpp-created
`CMAKE_INSTALL_LIBDIR`, `CMAKE_INSTALL_DOCDIR`, or
`CMAKE_INSTALL_DATAROOTDIR` cache entry appeared.

The hosted GCC, Clang, MSVC, and AppleClang jobs have not run on this
unpublished branch. Access to private asc-cmake remains an external
publication prerequisite.

## Reviewed boundary

Milestone 0 is a target-free architecture and repository foundation:

- project/package identity is `ASCCpp` version `0.0.0`;
- the root project uses `LANGUAGES NONE`;
- exact `ASCCMake 0.1.0` is required;
- no public C++ file, library, executable, product target, target export,
  provider, or numerical implementation exists;
- known component requests, unknown component requests, and no-component
  requests report the package unavailable and create no `ASC::*` target; and
- provider discovery, CUDA language enablement, provider execution,
  sanitizers, and benchmarks are outside this milestone.

The live component vocabulary contains exactly the six future modules, the two
random-owned storage facets, and the `cpp` aggregate. Future provider facets
remain architecture metadata and are not live package components.

## CMake and package review

The root build:

- requires CMake 3.25;
- calls `find_package(ASCCMake 0.1.0 EXACT CONFIG REQUIRED)`;
- additionally asserts the discovered version equals `0.1.0`;
- uses standard `CMakePackageConfigHelpers` for a target-free package;
- defines no language, compiler flag, target, provider option, or provider
  lookup;
- derives private non-cache install paths while honoring caller-provided
  install-directory variables;
- defaults testing and installation on only at top level and off when
  embedded;
- creates no target export and does not call `export(PACKAGE)`; and
- installs only package configuration, version metadata, and the Apache-2.0
  license.

The build-tree package uses `INSTALL_DESTINATION "."`, so its generated
`PACKAGE_PREFIX_DIR` follows the package directory when copied. The package
tests consume the original and copied build-tree package, install and consume
the package, rename the complete prefix to a path containing spaces, and
consume it again. No `*Targets.cmake` or binary artifact is emitted by a fresh
Milestone 0 build.

## Exact dependency and CI review

The local asc-cmake checkout was independently verified:

```text
commit: 8a7dcbad3a97267cce59810aff24de800a3497a7
tag:    v0.1.0
state:  clean main tracking origin/main
```

The workflow:

- grants only `contents: read`;
- pins the checkout action to
  `3d3c42e5aac5ba805825da76410c181273ba90b1`;
- pins asc-cmake to the approved commit;
- disables persisted checkout credentials;
- uses a separate `ASC_CMAKE_READ_TOKEN` for the private dependency;
- contains Linux GCC/CMake-minimum, Linux Clang/current-CMake, Windows MSVC
  multi-config, and macOS AppleClang jobs; and
- does not use `pull_request_target`.

The Windows job supplies `-A x64`, builds and tests Debug and Release, and
passes the active configuration into nested installation. The macOS job uses
ordinary quoted CMake commands and no GNU-only build command. These are static
workflow findings, not hosted execution evidence.

The repository owner must provision and audit a least-privilege read
credential before hosted CI can pass. Secrets are unavailable to ordinary
fork and Dependabot pull requests; no privileged execution workaround should
be added for untrusted code.

## Portability matrix

| Area | Evidence | Result |
| --- | --- | --- |
| CMake current | CMake/CTest 4.1.2, Unix Makefiles, Debug | 6/6 passed |
| CMake minimum | CMake/CTest 3.25.0, Unix Makefiles, Release | 6/6 passed |
| Path spaces | producer and nested package paths | passed |
| Package relocation | copied build tree and renamed installed prefix | passed |
| Package registry | isolated home and snapshot | unchanged |
| Presets | parsed and externally configured at both endpoints | passed |
| Subproject policy | fresh embedding projects on both CMake versions | passed |
| Exact asc-cmake | immutable commit and `v0.1.0` tag | passed |
| Provider isolation | source scan plus both fresh CMake caches | passed |
| Linux compiler | no C++ language or translation unit exists | not applicable |
| MSVC | Windows workflow static review | not run locally |
| AppleClang | macOS workflow static review | not run locally |
| Clang | Linux workflow static review | not run locally |
| Static/shared linkage | no library target exists | not applicable |

The presets parse under both supported CMake endpoints: five configure, five
build, and three test presets. The `dev-debug` and `dev-release` configure
presets were additionally exercised with external binary directories and
produced the expected Debug/Release cache values.

## Exact independent commands and results

Current-CMake Debug validation:

```sh
cmake -S /home/yicai/AI4SciComp/asc-cmake \
  -B /tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-current \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m0-portability.pkU2VE/current debug build with spaces" \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-current \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-m0-portability.pkU2VE/current debug build with spaces" \
  --parallel 2

ctest --test-dir \
  "/tmp/asc-cpp-m0-portability.pkU2VE/current debug build with spaces" \
  --output-on-failure
```

Result: configure passed, build passed, 6/6 tests passed, 0 failed, 0 skipped.

Minimum-CMake Release validation:

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cmake \
  -B /tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-325 \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m0-portability.pkU2VE/minimum release build with spaces" \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-325 \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Release

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake --build \
  "/tmp/asc-cpp-m0-portability.pkU2VE/minimum release build with spaces" \
  --parallel 2

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest --test-dir \
  "/tmp/asc-cpp-m0-portability.pkU2VE/minimum release build with spaces" \
  --output-on-failure
```

Result: configure passed, build passed, 6/6 tests passed, 0 failed, 0 skipped.

Preset parsing and execution:

```sh
cmake --list-presets=all
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake --list-presets=all

cmake --preset dev-debug \
  -B "/tmp/asc-cpp-m0-portability.pkU2VE/current preset build with spaces" \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-current
cmake --build \
  "/tmp/asc-cpp-m0-portability.pkU2VE/current preset build with spaces" \
  --parallel 2

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --preset dev-release \
  -B "/tmp/asc-cpp-m0-portability.pkU2VE/minimum preset build with spaces" \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-325
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake --build \
  "/tmp/asc-cpp-m0-portability.pkU2VE/minimum preset build with spaces" \
  --parallel 2
```

Result: both versions parsed every preset; both external preset configurations
and builds passed with the expected cache values.

Subproject isolation used a fresh target-free embedding project that called
`add_subdirectory()` on asc-cpp and failed configuration if either ASCCpp
option defaulted on or any of the three install-directory cache entries
existed.

```sh
cmake -S /tmp/asc-cpp-m0-portability.pkU2VE/embedding-project \
  -B "/tmp/asc-cpp-m0-portability.pkU2VE/current embedding build with spaces" \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-current

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /tmp/asc-cpp-m0-portability.pkU2VE/embedding-project \
  -B "/tmp/asc-cpp-m0-portability.pkU2VE/minimum embedding build with spaces" \
  -DASCCMake_DIR=/tmp/asc-cpp-m0-portability.pkU2VE/asc-cmake-325
```

Result: both passed; cache scans found no enabled ASCCpp option and no
ASCCpp-created install-directory entry.

Static source/cache assertions additionally passed:

```sh
rg -n \
  'find_package\\(ASCCMake 0\\.1\\.0 EXACT CONFIG REQUIRED\\)' \
  CMakeLists.txt

provider_pattern='CUDA|CUDAToolkit|BLAS|LAPACK|OpenMP|Eigen'
provider_pattern+='|MKL|TBB|HIP|ROCm'
! rg -n \
  "find_package\\((${provider_pattern})|enable_language\\((CXX|CUDA|HIP)" \
  CMakeLists.txt cmake tests/package tests/architecture \
  .github/workflows/ci.yml

cache_pattern='CMAKE_(CXX|CUDA|HIP)|CUDAToolkit|CUDA|BLAS|LAPACK'
cache_pattern+='|OpenMP|Eigen|MKL|TBB|SYCL|HIP|ROCm'
! rg -ni \
  "^(${cache_pattern})[^=]*=" \
  <each-fresh-CMakeCache.txt>
```

Result: exact ASCCMake requirement present; no provider/language discovery
found; both fresh caches were provider- and compiler-discovery-free.

## CPU, GPU, and provider evidence

CPU provider evidence: not applicable. Milestone 0 implements no serial
memory, execution, or numerical provider.

GPU evidence classification: `skipped`.

Reason: Milestone 0 prohibits provider discovery and implementation. No GPU
language, option, dependency, source, target, export, compile, runtime
operation, or parity oracle exists. No toolkit or hardware inventory is used
to infer support.

The repository has a pre-existing ignored `build/` directory containing
outputs from earlier cumulative work. It is not part of the tracked Milestone
0 diff, was not modified during this review, and is not evidence for this
milestone. All reported validation used fresh external directories.

## Sanitizer and performance evidence

Sanitizer evidence: `skipped`. There is no C++ translation unit, executable,
or production target to instrument. This is not a sanitizer pass.

Performance evidence: not applicable. There is no operation, allocation,
packing, transfer, densification, synchronization, or numerical kernel to
measure. A benchmark would fabricate later-milestone work.

## Findings

### PORT-001: build-tree and installed package relocation

Severity: informational

Status: passed

The build-tree config derives its prefix from its current package directory,
and the installed config uses the standard relocatable package helper. Copied
build-tree and renamed installed-prefix consumers passed at both CMake
endpoints, including paths containing spaces.

### PORT-002: embedding-project isolation

Severity: informational

Status: passed

ASCCpp defaults testing and installation off as a subproject and uses local
fallback install paths rather than publishing `GNUInstallDirs` cache entries.
Both fresh embedding configurations passed.

### PORT-003: private asc-cmake hosted access

Severity: external publication prerequisite

Status: open outside the repository

The workflow design is least-privilege and pins both checkout and asc-cmake
identities. The repository owner must configure `ASC_CMAKE_READ_TOKEN` before
hosted CI can execute. This requires external configuration, not a source
change.

## License, dependency, and provenance boundary

- `LICENSE` remains Apache License 2.0.
- No deleted notice, MdeCpp production/test source, provider SDK material,
  generated data, or third-party production dependency was restored.
- The only build dependency is exact released ASCCMake 0.1.0.
- Optional provider names remain planning metadata and do not leak into the
  live package component list or configuration.

## Remaining risks

- Hosted Linux Clang, Windows MSVC, and macOS AppleClang execution remains
  pending publication. Static workflow review is not execution evidence.
- `ASC_CMAKE_READ_TOKEN` must be configured and audited before hosted CI.
- Windows registry behavior has been reviewed but not executed locally.
- Clang-tidy translation-unit analysis, compiler C++20 conformance, warnings,
  sanitizers, linkage modes, providers, and performance must be validated only
  when a later approved milestone introduces the relevant targets.
- The ignored `build/` tree can confuse an unaudited workspace inventory and
  must not be cited as Milestone 0 evidence.

These are evidence limits or future-milestone obligations. None justifies
adding production code to Milestone 0.
