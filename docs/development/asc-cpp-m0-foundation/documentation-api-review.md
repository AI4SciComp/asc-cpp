# Milestone 0 documentation and API review

Status: Complete for local integration review on 2026-07-26

## Review boundary

This review covers the exposed Milestone 0 documentation, CMake package
contract, project options, presets, component vocabulary, and target/API
claims. It uses the frozen milestone contract, approved Stage A blueprint,
implementation plan, ASCCMake consumption report, release roadmap, and ADRs
0001, 0002, 0003, 0017, and 0018.

The review changed only the documentation reviewer's assigned paths. Root
CMake, package templates, presets, tests, workflows, tool policy, historical
bodies, and intentionally deleted files were read-only.

## Exposed foundation surface

| Surface | Reviewed Milestone 0 contract |
| --- | --- |
| Project/package | `ASCCpp`, unreleased `0.0.0` |
| Language/targets | CMake project with `LANGUAGES NONE`; no production target or public C++ API |
| Build dependency | `find_package(ASCCMake 0.1.0 EXACT CONFIG REQUIRED)` |
| Project options | `ASC_CPP_BUILD_TESTING`, `ASC_CPP_INSTALL` |
| Presets | `dev-debug`, `dev-release`, `test-debug`, `test-release`, `install-test` |
| Known future components | `core`, `utilities`, `expression`, `dense`, `sparse`, `random`, `random_dense`, `random_sparse`, `cpp` |
| Available components | none |
| Package variables | `ASCCpp_VERSION`, `ASCCpp_KNOWN_COMPONENTS`, `ASCCpp_AVAILABLE_COMPONENTS` |
| No-component lookup | treated as required `cpp`, then rejected as unavailable |
| Any component lookup | package reports `ASCCpp_FOUND=FALSE`; requested components report not implemented or unknown |
| Imported targets | none; no `ASC::*` target is created |
| Providers/GPU | no option, discovery, component, target, source, or runtime claim |

The future component vocabulary matches the exact six-module graph and two
random-owned facets. `cpp` is only a future aggregate. The documentation does
not present any of these names as implemented.

## Documentation disposition

- `README.md` is a restart/foundation guide with exact local ASCCMake 0.1.0
  configuration, presets, options, negative package behavior, test scope, and
  approval gates.
- `CHANGELOG.md`, `CONTRIBUTING.md`, and `SECURITY.md` are intentionally
  bounded to an unreleased foundation.
- `docs/README.md` separates current Stage A/Milestone 0 material from
  historical documents.
- Each of the 24 retained historical documents has the same superseded-state
  notice, identifies historical HEAD
  `33b261ea33616a6395c4ad3b20646093103344f7`, and links to the approved Stage A
  blueprint.
- Historical bodies were not rewritten. Their old API, target, provider,
  testing, and provenance statements remain evidence only under the banner.
- Deleted historical production, build, test, example, instruction, and notice
  files were not restored.

## Findings

### DAPI-001: optional component requests initially did not fail the package

Severity: release-blocking before correction

Status: resolved

The first reviewed `ASCCppConfig.cmake.in` set every requested component's
`_FOUND` variable to false but relied on `check_required_components`. A lookup
containing only `OPTIONAL_COMPONENTS` could therefore leave
`ASCCpp_FOUND=TRUE`, contrary to the frozen rule that every component request
must fail in Milestone 0.

The lead corrected the package config to set `ASCCpp_FOUND=FALSE` for every
lookup after component diagnostics are assembled. This preserves the
no-component-to-`cpp` rule and makes required, optional, known-unavailable, and
unknown requests consistently negative. Explicit optional-component fixture
coverage was requested from independent verification.

### DAPI-002: Stage A status headers retain checkpoint-time wording

Severity: informational

Status: accepted within the preservation contract

Some preserved Stage A files still say “Proposed at Architecture Checkpoint A”
or “awaiting Architecture Checkpoint A approval.” The owner subsequently
approved Stage A, and the frozen Milestone 0 contract records that authority.
The approved implementation plan also requires
`docs/development/asc-cpp-architecture/**` to remain unchanged in Milestone 0.

Current navigation calls the package approved and points to the frozen
contract, while this review treats the older status lines as checkpoint-time
provenance. A future explicitly authorized documentation-maintenance change
may normalize those headers; they are not a production API or release claim.

### DAPI-003: one historical body links to a deleted notice

Severity: informational

Status: accepted within the preservation contract

The local-link check reports
`docs/migration/inventory.md -> ../../THIRD_PARTY_NOTICES`. That notice is one
of the intentionally deleted historical files, and ADR 0017 explicitly
prohibits restoring it as authorization. The superseded-state notice identifies
the document as historical; changing its body or restoring the notice would
violate this reviewer's Milestone 0 scope.

All links added or replaced in the current guides, documentation index, review,
and superseded-state notices resolve locally.

### DAPI-004: installed repository guides had broken relative links

Severity: release-blocking before correction

Status: resolved

The first reviewed install rule copied `README.md` and renamed
`docs/README.md` into a flat installed documentation directory. Their
repository-relative links then pointed to files or paths absent from that
directory. The lead removed both repository guides from the install set.

The package now installs only its config, version file, and standalone license.
Repository documentation remains in its source-tree structure, where its local
links resolve.

### DAPI-005: no-production-target validation initially hung

Severity: validation-blocking before correction

Status: resolved

The first clean CMake 4.1.2 review run passed the manifest check, then hung in
`asc_cpp.architecture.no_production_targets`. Independent verification traced
this to a catastrophic regular expression in the root-project declaration
check and replaced it with a bounded extraction plus normalized assertions.

The documentation reviewer reran the same external build after the correction:
all six tests passed, including the target audit and all three package tests.

## Contract checklist

- Naming and dependency claims match the six approved modules and two facets.
- No deleted `array`, `linalg`, compatibility, or MdeCpp-derived API is
  advertised as current.
- No public C++ type, function, header, namespace member, ownership, lifetime,
  error, complexity, allocation, aliasing, execution, or thread-safety contract
  exists to review in Milestone 0.
- No component, provider, GPU, performance, sanitizer, numerical,
  reproducibility, zero-copy, asynchronous, or no-allocation capability is
  overstated.
- Exact ASCCMake identity and lookup behavior are documented without inventing
  an ASCCMake helper.
- The Apache-2.0 and clean-room provenance statements match ADR 0017; no deleted
  notice is presented as current authorization.
- Milestone completion, publication, release, and Core Milestone 1 are
  presented as separate approval gates.

## Documentation validation

Scoped local validation established:

- all links added by Milestone 0 documentation resolve;
- the only missing local target is the intentionally deleted notice recorded
  in DAPI-003;
- fenced code blocks are balanced;
- no new trailing whitespace exists;
- `git diff --check` reports no scoped error;
- `cmake --list-presets` reports exactly the five documented presets;
- all 24 retained documents contain one normalized identical banner; and
- each historical-document diff contains only the banner and its separating
  blank line: two additions, no deletion.

An external tests-disabled configure, build, and install also succeeded with
CMake 4.1.2 against ASCCMake `v0.1.0` at
`8a7dcbad3a97267cce59810aff24de800a3497a7`. The relocated prefix contained a
space and installed exactly the package config, package version file, and
license; it installed no repository guide or product target.

The documentation reviewer's corrected Debug build passed 6/6 tests in 0.89
seconds. The lead also independently reported fresh Debug 6/6 with CMake 4.1.2
and Release 6/6 with CMake 3.25.0. Those suites include known, unknown,
required, optional, and no-component requests in build, installed, and
relocated package states.

External web links and the historical documents' substantive claims were not
re-certified. Their bodies are retained evidence, not current guidance.

## Review conclusion

The exposed documentation and package vocabulary are honest for an unreleased
foundation. DAPI-001, DAPI-004, and DAPI-005 were corrected outside this
reviewer's write set and rechecked. There is no unresolved documentation/API
release blocker in the reviewed scope; Publication Checkpoint B still depends
on the full integration evidence required by the frozen milestone contract.
