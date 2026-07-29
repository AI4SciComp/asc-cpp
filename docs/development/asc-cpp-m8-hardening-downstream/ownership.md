# Milestone 8 Ownership Ledger

Status: Frozen

Date: 2026-07-28

Branch: `feature/asc-cpp-m8-hardening-downstream-r2`

All write scopes are disjoint. Every role preserves the exact cumulative
Milestones 0--7 predecessor and unrelated owner work. No role may commit,
push, merge, tag, release, mutate a pull request, delete a branch, add an
unapproved dependency, modify another repository, or implement a later
milestone.

The earlier `feature/asc-cpp-m8-hardening-downstream` branch and pull request 1
are read-only. No role may check out, modify, rebase, merge, push, close, or
delete them.

## Lead integrator

Exclusive write scope:

```text
CMakeLists.txt
CMakePresets.json
.github/workflows/ci.yml
cmake/**
src/**/CMakeLists.txt
tests/**/CMakeLists.txt
tests/architecture/**
tests/compile/CMakeLists.txt
tests/consumer/CMakeLists.txt
tests/package/**
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/migration/handoff.md
docs/modules/core.md
docs/modules/utilities.md
docs/modules/expression.md
docs/modules/dense.md
docs/modules/sparse.md
docs/modules/random.md
docs/development/asc-cpp-architecture/**
docs/development/asc-cpp-m8-hardening-downstream/milestone-contract.md
docs/development/asc-cpp-m8-hardening-downstream/ownership.md
docs/development/asc-cpp-m8-hardening-downstream/preflight.md
docs/development/asc-cpp-m8-hardening-downstream/dependency-audit.md
docs/development/asc-cpp-m8-hardening-downstream/provenance-record.md
docs/development/asc-cpp-m8-hardening-downstream/publication-checkpoint-b.md
```

Responsibilities: branch/contract/ledger, actual ASCCMake use, version and
component package integration, root/shared CMake, architecture manifests, test
registration, complete diff review, clean matrix execution, final evidence
classification, and Checkpoint B. Only the lead edits root, shared CMake,
package, architecture, current module-guide version examples, or handoff
files. The six module guides were added to this scope before editing after the
documentation reviewer reported their stale `find_package(ASCCpp 0.7 ...)`
examples.

## Production hardening agent

Exclusive write scope:

```text
tools/hardening/**
abi/**
docs/development/asc-cpp-m8-hardening-downstream/production-self-review.md
```

Responsibilities: implement repository-native API/header/target/symbol/ABI
inventory and reporting tools plus reviewed local baselines. Tools are
read-only with respect to source and product artifacts, deterministic,
portable within the declared local support matrix, and introduce no product
dependency. The agent may inspect but not edit product headers/sources, CMake,
tests, package files, general docs, or another repository. Any product defect
is reported to the lead.

## Independent verification agent

Exclusive write scope:

```text
tests/hardening/** except tests/hardening/CMakeLists.txt
tests/downstream/** except tests/downstream/CMakeLists.txt
benchmarks/hardening/**
docs/development/asc-cpp-m8-hardening-downstream/verification-design.md
docs/development/asc-cpp-m8-hardening-downstream/verification-review.md
```

Responsibilities: independently design and implement package metadata/version,
public API, installed-header, symbol, compile-time/object-size, and
asc-xde-shaped trial checks. It freezes independent oracles before inspecting
production hardening tools and uses only public downstream APIs. It does not
edit production tools, product code, CMake/package integration, architecture
files, general docs, or another repository.

## Documentation and API review agent

Exclusive write scope:

```text
docs/support-matrix.md
docs/api-compatibility.md
docs/package-capabilities.md
docs/extension-guide.md
docs/downstream-integration.md
docs/performance.md
docs/development/asc-cpp-m8-hardening-downstream/documentation-api-review.md
```

Responsibilities: document and independently audit package metadata,
source/ABI/provider compatibility boundaries, the support matrix, extension
rules, asc-xde-shaped trial, performance envelope, ownership/lifetime
implications, and unreleased status. API/package defects are reported rather
than documented around.

## Portability/GPU/performance review agent

Exclusive write scope:

```text
docs/development/asc-cpp-m8-hardening-downstream/portability-review.md
```

Responsibilities: independently audit full-matrix coverage, C++20 and CMake
portability, ELF-symbol evidence limits, sanitizer coverage, CUDA/provider
isolation, package relocation, performance methodology, downstream trial,
toolchain/platform skips, and exact GPU evidence labels. It edits no
production, verification, CMake/package, benchmark, or general documentation
file.

## Conflict rule

If a role needs work outside its scope, it reports the exact file or symbol
and reason. Only the lead may make a shared integration change or revise
scope, and any revision is recorded before the write.
