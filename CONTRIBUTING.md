# Contributing

asc-cpp is currently at Milestone 0, a target-free architecture and repository
foundation. Contributions must preserve that boundary until a later milestone
has its own approved contract and branch.

## Before making a change

Read:

1. the [Stage A architecture blueprint][stage-a];
2. the [ADRs][adrs];
3. the [implementation plan][implementation];
4. the [Milestone 0 contract][m0-contract]; and
5. the ownership ledger for the active milestone.

Keep changes within the assigned write scope. Preserve unrelated and
uncommitted work. Publication, merge, release, tags, and branch deletion are
separate owner-authorized actions.

## Milestone 0 boundary

Do not add:

- a file below `include/asc` or `src`;
- a production, module, facet, provider, compatibility, or fake reference
  target;
- an `array`, `linalg`, common backend, or seventh module;
- provider discovery or CUDA language enablement;
- an unapproved dependency;
- copied or mechanically translated MdeCpp source, tests, data, or tables; or
- implementation from a later milestone.

Do not restore intentionally deleted historical production files, tests,
examples, notices, instructions, `AGENTS.md`, or `generator.md`.

## Build and package policy

- Require CMake 3.25 or newer.
- Bind the released `ASCCMake` 0.1.0 API; do not invent or imitate a helper.
- Keep effects target-local and use standard CMake for the component-aware
  `ASCCpp` package skeleton.
- Do not write to the CMake user package registry.
- Keep top-level testing/install defaults distinct from subproject defaults.
- Ensure unknown and unavailable components fail without creating an
  `ASC::*` target.

Later production C++ uses strict C++20, extensions disabled, the current
Google C++ Style Guide, self-contained `.h` public headers, and `.cc` compiled
sources. Milestone 0 itself has no production C++ surface.

## Validation

Use fresh external build directories and record the exact CMake, generator,
host, and released asc-cmake identity. At minimum run Debug and Release
foundation configurations, their complete CTest suites, installation and
relocation checks, documentation validation, and:

```sh
git diff --check
```

Never describe an unavailable platform, compiler, sanitizer, or GPU as
passing. Milestone 0 has no sanitizer, numerical, provider-runtime, GPU
runtime, parity, or performance claim.

## Documentation

Live entry documentation must describe only implemented behavior. Future
targets and components must be labeled as planned or approved architecture.

The 24 bannered documents listed in the implementation plan are retained
historical bodies. Do not silently modernize them; only the normalized
superseded-state banner is part of Milestone 0.

## Provenance and dependencies

asc-cpp remains Apache-2.0. MdeCpp is behavior and defect evidence only.
External source or data requires a pinned authoritative upstream, license and
notice review, original/local path mapping, modification record, derivation
evidence, and explicit approval. See the [provenance review][provenance].

Do not place credentials, tokens, private keys, or machine-specific paths in
the repository, logs, committed presets, or examples.

[adrs]: docs/development/asc-cpp-architecture/decisions
[implementation]: docs/development/asc-cpp-architecture/implementation-plan.md
[m0-contract]: docs/development/asc-cpp-m0-foundation/milestone-contract.md
[provenance]: docs/development/asc-cpp-architecture/provenance-review.md
[stage-a]: docs/development/asc-cpp-architecture/architecture-blueprint.md
