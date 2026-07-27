# Verified asc-cmake consumption contract

Status: Approved Stage A binding; consumed by the Milestone 1 core candidate

Verified date: 2026-07-26

## Release identity

| Field | Verified value |
| --- | --- |
| repository / remote | `/home/yicai/AI4SciComp/asc-cmake`, `git@github.com:AI4SciComp/asc-cmake.git` |
| release | `v0.1.0`, published 2026-07-24 |
| release commit | `8a7dcbad3a97267cce59810aff24de800a3497a7` |
| annotated tag object | `620b2e912ac5bac7561e09529a65cce965ebc920` |
| package name | `ASCCMake` |
| minimum CMake | 3.25 |
| package compatibility | `SameMinorVersion`, architecture independent |
| package variables | `ASCCMake_VERSION`, `ASCCMake_MODULE_DIR` |
| imported target namespace | none; this is a CMake-language package |
| license | Apache-2.0 |

Release evidence:

- merged [PR 1](https://github.com/AI4SciComp/asc-cmake/pull/1);
- successful post-merge
  [CI run 30099434997](https://github.com/AI4SciComp/asc-cmake/actions/runs/30099434997);
- published
  [GitHub release v0.1.0](https://github.com/AI4SciComp/asc-cmake/releases/tag/v0.1.0).

## Canonical consumption modes

1. Installed or configured build-tree package:

   ```cmake
   find_package(ASCCMake 0.1 CONFIG REQUIRED)
   ```

2. Direct source with `add_subdirectory`.
3. Local `FetchContent` with an explicit `SOURCE_DIR`.
4. Reviewed allowlist vendoring containing the complete module closure,
   `VERSION`, and `LICENSE`.
5. Selective direct module inclusion when intentionally required.

Installed/configured lookup loads the aggregate `ASCCMake.cmake`. The package
does not mutate `CMAKE_MODULE_PATH`.

## Exact public API

```text
asc_target_enable_cxx20(TARGET <target> [PUBLIC])

asc_target_enable_warnings(TARGET <target> [AS_ERRORS])

asc_target_enable_sanitizers(
  TARGET <target>
  [ADDRESS] [UNDEFINED] [THREAD] [LEAK])

asc_register_test(
  NAME <name>
  TARGET <executable>
  [ARGUMENTS ...] [LABELS ...] [TIMEOUT <seconds>]
  [WORKING_DIRECTORY <path>] [ENVIRONMENT <key=value>...])

asc_install_package(
  PACKAGE <name>
  EXPORT <export-set>
  NAMESPACE <namespace::>
  VERSION <numeric-version>
  TARGETS <targets...>
  CONFIG_TEMPLATE <path>
  [COMPATIBILITY <mode>]
  [INCLUDE_DESTINATION <relative-path>]
  [RUNTIME_DESTINATION <relative-path>]
  [LIBRARY_DESTINATION <relative-path>]
  [ARCHIVE_DESTINATION <relative-path>]
  [CONFIG_DESTINATION <relative-path>])

asc_add_project_options(
  TARGET <new-interface-target>
  [WARNINGS] [WARNINGS_AS_ERRORS]
  [SANITIZERS <ADDRESS|UNDEFINED|THREAD|LEAK>...])
```

No other helper is part of the release contract.

## Behavior that asc-cpp may rely on

- `asc_target_enable_cxx20` sets C++20, requires the standard, disables
  extensions, and adds `/permissive-` for genuine MSVC. It rejects aliases,
  imported/utility/object/interface targets.
- Warnings are target-local: GCC/Clang/AppleClang use
  `-Wall -Wextra -Wpedantic`; MSVC uses `/W4`; errors are opt-in.
- Sanitizers are target-local and probed. Thread cannot combine with Address
  or Leak. Unsupported requests fail rather than disappear.
- `asc_register_test` registers one existing local executable and never
  creates or downloads a test target.
- `asc_install_package` installs supported targets and public HEADERS file
  sets, creates separate build/install configs and exports, remains
  relocatable, and does not write the user package registry.
- `asc_add_project_options` creates only the caller-named interface carrier and
  only the requested policies.

## Options and defaults

asc-cmake owns exactly:

| Option | Top-level default | Subproject default |
| --- | --- | --- |
| `ASC_CMAKE_BUILD_TESTING` | follows `BUILD_TESTING` | `OFF` |
| `ASC_CMAKE_INSTALL` | `ON` | `OFF` |

It does not own asc-cpp provider, scalar, linkage, example, analysis, or
feature options.

Presets are `dev-debug`, `dev-release`, `test-debug`, `test-release`, and
`install-test`.

## Fresh local validation

An external Release build used CMake 4.1.2, GCC 11.4, and Unix Makefiles:

```text
61 discovered
59 passed
0 failed
2 skipped: ThreadSanitizer and LeakSanitizer runtime probes
install-and-consume passed
```

The build directory was under `/tmp`, outside both repositories.

Released hosted evidence on the exact release commit:

| Platform | Toolchain | Configurations | Result |
| --- | --- | --- | --- |
| Ubuntu 22.04 | GCC 11, CMake 3.25.0 | Debug, Release | 61/61 completed without failure in each |
| Ubuntu 24.04 | Clang 18.1.3, CMake 4.4.0, Ninja | Debug, format, clang-tidy | 61/61 completed without failure |
| Windows Server 2022 | VS 2022/MSVC, CMake 3.31.6 | Debug, Release multi-config | 61 completed without failure; six documented sanitizer skips per configuration |
| macOS 15 arm64 | AppleClang 17/Xcode 16.4, CMake 4.4.0 | Debug | 61 completed without failure; LeakSanitizer skipped |

Every hosted job used the full-commit-pinned checkout action
`3d3c42e5aac5ba805825da76410c181273ba90b1`.

## Limitations affecting ASCCpp

asc-cmake v0.1.0 deliberately does not:

- define product libraries, sources, aliases, or dependencies;
- discover CUDA, OpenMP, BLAS/LAPACK, Eigen, MKL, or another provider;
- implement ASCCpp component semantics;
- generate one conditional export per package component;
- support multiple `asc_install_package` calls for the same package in one
  binary directory;
- expose a CUDA-language helper or static-analysis runner;
- accept interface targets without a public HEADERS file set;
- accept object libraries in `asc_install_package`;
- solve arbitrary nested build-interface target expressions;
- allow isolated multiple asc-cmake versions in one configure process.

`asc_install_package` creates one targets file for one export set. Loading that
file imports every target in the set. Conditional ASCCpp provider components
must not make their SDK dependencies unconditional.

## Approved consumption decision

Milestone 0 will:

- bind exactly `ASCCMake 0.1` and record commit/tag evidence;
- use `asc_target_enable_cxx20`, warnings, sanitizers, project options, and
  test registration as applicable;
- use standard target-oriented CMake for ASCCpp target definitions, dependency
  discovery, component closure, conditional exports, and package config;
- use `configure_package_config_file`, `write_basic_package_version_file`,
  `install(TARGETS ... EXPORT ...)`, and `install(EXPORT ...)` directly for
  the multi-component ASCCpp package;
- not wrap or imitate `asc_install_package`;
- open a bounded asc-cmake issue only if a reusable multi-component export
  abstraction is later justified.

This is the runbook-approved “standard CMake in asc-cpp” response to a missing
capability. No asc-cmake API is invented.
