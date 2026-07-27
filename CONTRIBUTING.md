# Contributing to asc-cpp

asc-cpp is an architecture-first C++20 project under a staged restart. Changes
must match the current approved milestone; plans for later modules do not
authorize their implementation.

## Before changing the repository

Read:

1. the [current documentation index](docs/README.md);
2. the approved
   [six-module blueprint](docs/development/asc-cpp-architecture/architecture-blueprint.md);
3. the ADRs relevant to the proposed change; and
4. the active milestone contract and ownership ledger, when one exists.

Confirm the branch and dirty state before editing. Preserve unrelated work and
do not restore deleted historical source, tests, examples, instructions, or
notices.

## Architecture boundaries

The only modules are `core`, `utilities`, `expression`, `dense`, `sparse`, and
`random`. Direct dependencies must match the
[approved graph](docs/development/asc-cpp-architecture/decisions/0001-six-module-graph.md).
In particular:

- `expression` is storage-neutral;
- `dense` and `sparse` never depend on one another;
- `utilities`, `dense`, and `sparse` are mutually independent;
- storage modules do not depend on `random`; and
- optional providers do not leak into common public headers or base package
  requirements.

Do not add a dependency, provider, component, compatibility layer, or public
API outside an approved milestone.

## Source and API policy

Future production code uses strict C++20 with compiler extensions disabled.
Public declarations use the flat `asc` namespace. Public headers are
self-contained `.h` files with full-path include guards and direct includes;
ordinary compiled C++ sources use `.cc`. Internal namespace names contain
`internal`.

Public contracts must state applicable ownership, lifetime, aliasing, failure,
complexity, allocation, memory-space, transfer, synchronization, thread-safety,
and reproducibility behavior. Do not document an unverified provider,
performance, determinism, asynchronous, zero-copy, or no-allocation claim.

## Configure and validate

Use the exact ASCCMake release and commands in the
[README](README.md#configure-the-milestone-0-foundation). For Milestone 0, run
the complete CTest suite and:

```bash
git diff --check
```

Record exact commands, tool versions, pass/fail/skip counts, and unavailable
checks. A skipped provider or unavailable tool is not a pass.

## Documentation and provenance

Update current documentation with the behavior it actually verifies. Do not
rewrite retained historical documents into current guidance.

MdeCpp is behavioral evidence only. Its production code and tests must not be
copied or mechanically translated. Any external source or data import requires
the provenance, license, notices, immutable identity, and approval specified by
[ADR 0017](docs/development/asc-cpp-architecture/decisions/0017-third-party-provenance.md).

## Review and publication

Keep changes bounded and explain their architecture and dependency effects.
Include tests appropriate to the contract. Publication actions, releases, and
branch deletion require the approvals defined by the active milestone; do not
infer them from approval to implement.

Report suspected vulnerabilities privately as described in
[SECURITY.md](SECURITY.md).
