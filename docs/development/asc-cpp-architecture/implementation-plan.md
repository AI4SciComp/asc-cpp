# First milestone implementation plan

Status: Proposed; do not execute before Architecture Checkpoint A approval

## Milestone

Milestone 0: architecture and repository foundation.

The working branch will be:

```text
feature/asc-cpp-m0-foundation
```

It will be created from the audited `main` only after approval. No worktree is
needed unless the dirty-state review changes.

## Scope

Milestone 0 creates a strict C++20 project shell, exact asc-cmake binding,
analysis/test policy, component-aware package skeleton, negative consumer
fixtures, and portable CI. It creates no public production header, source,
component target, compatibility facade, or provider target.

## Exact files

### Add

```text
.clang-format
.clang-tidy
CHANGELOG.md
CONTRIBUTING.md
SECURITY.md
CMakeLists.txt
CMakePresets.json
cmake/ASCCppConfig.cmake.in
cmake/ASCCppComponents.cmake
cmake/ASCCppOptions.cmake
docs/README.md
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_no_production_targets.cmake
tests/architecture/check_public_file_policy.cmake
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
```

The package config/version and license/docs may install, but a request for
`core`, `cpp`, or another unimplemented component must fail cleanly and no
`ASC::*` target may exist.

### Replace

```text
README.md
.github/workflows/ci.yml
```

README will describe the restart and approved graph without claiming a library
exists. CI will use the exact private asc-cmake commit through a least-
privilege credential and pinned actions.

### Add a superseded-state banner to retained historical documents

```text
docs/api.md
docs/architecture.md
docs/build-system.md
docs/optional-backends.md
docs/testing.md
docs/design/architecture_blueprint_v1.md
docs/design/architecture_review_v1.md
docs/design/array_design.md
docs/design/core_design.md
docs/design/linalg_design.md
docs/design/random_design.md
docs/design/utilities_design.md
docs/migration/array.md
docs/migration/core.md
docs/migration/handoff.md
docs/migration/inventory.md
docs/migration/linalg.md
docs/migration/random.md
docs/migration/utilities.md
docs/modules/array.md
docs/modules/core.md
docs/modules/linalg.md
docs/modules/random.md
docs/modules/utilities.md
```

The banner will identify the exact historical commit, state that the described
implementation is deleted/superseded, and link to this Stage A package.
Historical content remains for auditability; it is not silently rewritten into
six-module documentation.

### Preserve unchanged

```text
.gitignore
LICENSE
docs/development/asc-cpp-architecture/**
```

All 208 intentional tracked deletions remain deleted unless an exact file above
is explicitly reintroduced as new Milestone 0 content. No deleted production,
test, example, notice, `AGENTS.md`, or `generator.md` file is restored.

## CMake contract

- `cmake_minimum_required(VERSION 3.25)`.
- Project `ASCCpp`, no production language target.
- `find_package(ASCCMake 0.1 CONFIG REQUIRED)` or its reviewed local source
  consumption mode; assert version/identity in tests.
- Standard CMake generates the component-aware package skeleton.
- No call imitates a missing asc-cmake component helper.
- Top-level install/testing defaults are on; subproject defaults are off.
- Provider options do not exist until their milestone.

## Validation

Fresh external build directories:

1. print Git/CMake/compiler/host and asc-cmake identity;
2. configure Debug and Release with CMake 3.25 endpoint and current CMake;
3. build the foundation (no production library);
4. run CTest:
   - dependency/capability YAML parses;
   - exact six-module graph and forbidden edges match;
   - no production/public/provider target exists;
   - no public C++ file violates `.h`/`.cc` policy;
   - install and relocate to a path containing spaces;
   - unknown, `core`, and `cpp` required-component requests fail as designed;
   - package registry is unchanged;
5. validate Markdown links and code fences;
6. run `git diff --check`;
7. run the four-platform CPU CI matrix after the asc-cmake read credential is
   configured;
8. record exact pass/fail/skip counts and do not publish.

There are no sanitizer or numerical runtime claims in a milestone with no
production code. The Clang job still validates formatting and CMake files.

## Risks and controls

| Risk | Control |
| --- | --- |
| Reintroducing stale five-component behavior | exact no-production-target test and historical banners |
| Inventing asc-cmake behavior | bind v0.1.0; use only verified helpers and standard CMake |
| Private asc-cmake checkout fails in CI | require a least-privilege read credential before publication; never log it |
| Package skeleton appears to provide modules | negative component consumers and imported-target audit |
| Dirty deletions are accidentally restored | before/after name-status manifest and lead diff review |
| Formatting tools absent locally | pinned hosted Clang job; absence remains explicit locally |
| Architecture YAML/docs drift | schema/graph/link tests |

## Rollback

Before a commit, rollback is removal of only the exact Milestone 0 additions
and reversal of the listed replacements/banners; the 208 deletions remain
untouched. After an approved commit, rollback is a reviewed `git revert`, never
`reset --hard`, force-push, or branch deletion. Package/CI credentials are
external configuration and are removed separately by their owner if the
milestone is abandoned.

## Exit and next gate

Milestone 0 stops at Publication Checkpoint B with a complete diff and clean
validation record. It does not automatically begin Core Milestone 1. Core gets
its own frozen contract, ownership ledger, implementation/verification wave,
and approval.
