# Milestone 8 Provenance Record

Status: Complete

Date: 2026-07-27

## Authorship boundary

Milestone 8 hardening tools, independent fixtures, documentation, package
integration, and review records were authored from:

- the frozen Milestone 8 contract and ownership ledger;
- the approved asc-cpp blueprint, roadmap, dependency/capability manifests,
  and ADRs 0001--0018;
- current public asc-cpp headers, CMake target properties, generated package
  exports, and locally built binaries;
- the actual released ASCCMake 0.1.0 package and documented APIs; and
- ordinary CMake, compiler, linker, ELF-tool, and Git behavior.

No production implementation, test, literal corpus, generated table, or
expected vector was copied or mechanically translated from MdeCpp, deleted
historical asc-cpp code, asc-xde, or another sibling repository.

## Mechanical observations

- `abi/public-headers.sha256` contains LF-normalized SHA-256 observations of
  the current 49 approved public headers.
- `abi/header-owners.txt` and `abi/targets-and-components.txt` record reviewed
  facts from the approved target/component graph and actual CMake file sets.
- `abi/linux-x86_64-*-shared.txt` contains local compiler/build-specific ELF
  report digests and reviewed symbol counts. Those observations do not import
  third-party code and do not promise cross-toolchain or cross-minor ABI.
- Compile time, object size, runtime benchmark, and provider results are local
  observations produced by the candidate; they are not normative external
  data.

Baseline updates are ordinary reviewed source changes. The tools never rewrite
their baselines.

## asc-xde trial boundary

The real asc-xde repository is inspected read-only only to prove the approved
commit `abcb29b51f22f40afd7f174707b7ccf83c32d4bf` and a clean before/after
worktree. The asc-cpp-owned fixture is an independently written, small
ODE/diffusion dependency trial. It copies no asc-xde implementation or API and
does not claim a completed migration.

## Prior provider provenance

The accepted Milestone 2 Philox and Milestone 7 CUDA provenance records remain
authoritative for those unchanged implementations. Milestone 8 adds no
algorithm, provider kernel, random vector, or numerical operation requiring a
new external source.
