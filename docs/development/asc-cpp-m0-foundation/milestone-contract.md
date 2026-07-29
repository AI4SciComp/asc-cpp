# Milestone 0 contract: architecture and repository foundation

Status: Frozen after owner approval on 2026-07-27

Branch: `feature/asc-cpp-m0-foundation`

Authority:

1. the owner's approval to continue with Milestone 0;
2. the all-in-one runbook;
3. the approved Stage A architecture package and ADRs;
4. released asc-cmake `v0.1.0` at
   `8a7dcbad3a97267cce59810aff24de800a3497a7`.

## Predecessor state

Milestone 0 starts from clean, current `main` at
`33b261ea33616a6395c4ad3b20646093103344f7`, which matches `origin/main`.
The approved Stage A package is reconstructed from the preserved cumulative
milestone snapshot without importing any Milestone 1 or later implementation.
The cumulative Milestone 8 branch and its draft pull request remain untouched.

## Objective

Create the repository, build, package, test, documentation, and CI foundation
for the approved six-module architecture without implementing or exporting a
production API, module target, provider target, or compatibility facade.

## In scope

- all approved Stage A reports, manifests, and 18 ADRs;
- strict C++20 formatting/analysis policy;
- root CMake project and reproducible presets;
- exact ASCCMake 0.1.0 binding;
- ASCCpp component-aware package skeleton;
- architecture and negative package tests;
- install/relocation/path-with-spaces validation;
- replacement restart README and contributor/security/changelog documents;
- superseded-state banners on retained five-component documents;
- portable, least-privilege CI design;
- ownership, verification, documentation/API, and portability review records.

## Prohibited

- any file below `include/asc` or `src`;
- any production, module, facet, provider, compatibility, or fake reference
  target;
- restoration of deleted historical source/tests/examples/notices/
  instructions;
- an `array`, `linalg`, or seventh module;
- provider discovery or CUDA language enablement;
- an unapproved dependency;
- push, pull request, merge, tag, release, or branch deletion.

## Required package behavior

- Package identity is `ASCCpp`, version `0.0.0` for the unreleased foundation.
- Known future component names are validated.
- `find_package(ASCCpp)` is equivalent to requesting unavailable `cpp`.
- Requests for `core`, `cpp`, any other known component, or an unknown
  component report `ASCCpp_FOUND=FALSE` and create no `ASC::*` target.
- The build-tree and installed package are relocatable and contain no product
  target export.

## Required validation

- exact branch/repository/dirty-state preservation;
- CMake 3.25 minimum and current local CMake where available;
- Debug and Release foundation configure/build/CTest;
- exact dependency/capability manifest checks;
- no-production-file/target checks;
- build-tree and installed package negative consumers;
- relocated prefix containing spaces;
- unchanged CMake user package registry;
- Markdown/YAML/whitespace checks;
- hosted GCC/Clang/MSVC/AppleClang design, subject to private asc-cmake access;
- independent documentation/API and portability reviews.

## Stop condition

After integration, validation, and independent review, stop at Publication
Checkpoint B. Publication remains separately authorized.
