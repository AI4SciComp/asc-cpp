# CI strategy

Status: CPU workflow candidate implemented; Milestone 8 local hardening
evidence accepted at Publication Checkpoint B and hosted evidence pending
publication

## Supply and permissions

- Pin every third-party action to a full immutable commit and record its
  upstream release.
- Use `permissions: contents: read`, do not persist checkout credentials, and
  add pull-request concurrency cancellation.
- Never print tokens, credential-bearing URLs, or configuration secrets.
- asc-cmake is private. The current repository token cannot be assumed to read
  it. A separately configured least-privilege read credential is a Milestone 0
  hosted-CI prerequisite.
- Checkout asc-cmake at exact commit
  `8a7dcbad3a97267cce59810aff24de800a3497a7`; do not clone its branch tip.

The historical workflow failed because it attempted an unauthenticated HTTPS
clone of private asc-cmake. The Milestone 1 candidate workflow uses a second
full-commit-pinned checkout and expects the approved
`ASC_CMAKE_READ_TOKEN`. Hosted execution remains blocked until repository
administration configures that least-privilege secret; this work creates no
credential.

## CPU matrix

| Job | Required coverage |
| --- | --- |
| Linux GCC / minimum CMake | Ubuntu 22.04, GCC 11.4, CMake 3.25.x, Unix Makefiles, Debug and Release |
| Linux Clang / current CMake | Ubuntu 24.04, Clang 18, current stable CMake, Ninja, Debug, format, clang-tidy, ASan/UBSan |
| Windows MSVC | Server 2022, Visual Studio 2022 x64 multi-config, Debug and Release, install/path-with-spaces |
| macOS AppleClang | macOS 15 arm64, AppleClang/Xcode 16.4, Debug, install/consumer |

Every job prints host, architecture, compiler, CMake/CTest, generator,
configuration, enabled components/providers, test totals, and skips.

## Per-job gates

- clean out-of-source configure;
- exact asc-cmake identity assertion;
- strict C++20/extensions-off assertion;
- build and CTest `--output-on-failure`;
- dependency/header/ODR/package tests;
- install/relocate/consume;
- documentation/link/manifest validation;
- bounded timeouts.

Warnings-as-errors apply to project code in strict jobs, not consumers.
Sanitizers are explicit and never leaked into installed targets.

## GPU jobs

Separate jobs report:

1. CUDA toolkit configure;
2. provider compile/link;
3. real-hardware runtime;
4. CPU/reference parity.

Local evidence has passed levels 1–3 for `core_cuda` and levels 1–4 for the
bounded dense, sparse, and random CUDA facets on CUDA 12.9.86, driver 576.83,
and one RTX 3060 Laptop GPU (compute capability 8.6). The repository has no
verified hosted GPU runner, so no GPU workflow job or hosted claim is
invented. GPU jobs must record driver, toolkit,
device, compute capability, memory, provider algorithm, workspace, and
determinism scope. A missing runner is a release blocker for a hosted GPU
capability claim, not a base CPU release blocker.

## Static analysis and style

- Current Google C++ style and the checked-in formatter configuration are the
  source of truth.
- Run clang-format and clang-tidy at pinned versions in the Clang job.
- Retain target-local GCC/Clang/MSVC warning gates from asc-cmake.
- cpplint may be a secondary style check only if its configuration matches the
  accepted policy; the historical 187-error job is not retained unchanged.
- Add dependency/source scanners and YAML/schema validation as first-class
  tests.

## Release evidence

Release notes list exact:

- commit and tag;
- OS/compiler/CMake/generator/configuration;
- components and provider facets;
- tests passed, failed, skipped, and not run;
- sanitizer/analyzer results;
- hardware/driver/toolkit for providers;
- package modes and relocation;
- known limitations.

No branch push, tag, release, artifact publication, or branch deletion occurs
from this Milestone 8 work.
