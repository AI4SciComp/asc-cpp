# ASCCpp 0.9.0 release validation

Status: Gate B local validation was completed and owner-approved on
2026-08-03. Phase 8 records hosted evidence on the release-hardening pull
request. The external publication gates below still apply.

Raw logs, CTest JUnit files, build trees, install prefixes, generated Doxygen,
and dry-run artifacts are retained outside the source tree. No machine-local
path is recorded in this report.

## Release identities

- Audited base, live `main`, and `develop`:
  `402cbac35334bb2a20e7e6afa7214efb8fad1c8f`.
- Annotated snapshot tag object:
  `03258702fe76fb53d8b08b71d838e45dc8dadd6f`; it peels to the audited base.
- Snapshot name: `snapshot/v0.9.0-candidate-20260802`.
- Implementation branch: `release/0.9.0`, created from the audited base. Gate B
  validated its worktree before any implementation commit or push, as required.
- Planned release tag: `v0.9.0`; it does not exist.

The latest hosted `main` run visible at Gate B was CI run `30745704835`, which
succeeded for the unchanged audited base. It is not evidence for this
uncommitted release worktree.

## Validated support boundary

The supported profile is the provider-free C++20 CPU reference release. It
retains the six-module architecture and the Random Dense/Sparse storage facets.
CPU Dense BLAS and Sparse BLAS are correctness-reference implementations, not
optimized third-party providers.

CUDA is experimental. Local real-device evidence is recorded below, but no
trusted hosted NVIDIA runner has validated the exact eventual release commit.
The local CUDA result therefore does not expand the supported release profile.

Compiled libraries use `VERSION 0.9.0` and `SOVERSION 0.9`. The ABI files are
observation baselines, not a cross-toolchain or cross-minor compatibility
promise.

## Tool and environment identities

| Tool or platform | Exact identity |
| --- | --- |
| Local platform | Ubuntu 22.04.5 LTS, x86-64 |
| CMake | 4.1.2; minimum-version check with 3.25.0 |
| GCC | 11.4.0 |
| Clang | 19.0.0 |
| clang-format / clang-tidy | 18.1.8 |
| Doxygen | 1.9.8 |
| actionlint | 1.7.7 |
| SBOM generator | Syft 1.50.0, SPDX JSON |
| CUDA compiler | NVCC 12.9.86 |
| CUDA analysis | Compute Sanitizer 2025.2.1.0 |
| NVIDIA device | GeForce RTX 3060 Laptop GPU, 6144 MiB, compute capability 8.6 |
| NVIDIA driver | 576.83; CUDA runtime compatibility 12.9 |

## Local verification matrix

All listed builds enabled C++20, tests, install rules, and warnings as errors.
Provider-free and sanitizer JUnit results contain zero failures, errors, or
skips.

| Area | Exact Gate B result |
| --- | --- |
| Formatting | 315 C++/CUDA files checked with clang-format 18.1.8; clean |
| Static analysis | 19 provider-free production translation units checked with clang-tidy 18.1.8 and Clang 19; clean |
| GCC 11 / CMake 4.1.2 | Debug static 219/219; Release static 219/219; Debug shared 220/220; Release shared 220/220 |
| Minimum CMake 3.25.0 / GCC 11 | Debug static 218/218; Release static 218/218 |
| Clang 19 / CMake 4.1.2 | Debug static 218/218; Release static 218/218; Debug shared 220/220; Release shared 220/220 |
| Sanitizers with Clang 19 | ASan+UBSan 173/173; standalone LSan 173/173; bounded TSan concurrency set 3/3 |
| Public headers | All 52 headers compiled self-contained and with exceptions disabled; 52-header hash manifest and 15-target/component surface passed |
| Package/consumer matrix | Static 49/49 and shared 49/49 for build tree, install, relocation, component isolation, paths with spaces, repeated lookup, metadata, downstream, and installed examples; final registry-isolation subset rerun 8/8 per linkage |
| Installed examples | Five examples configured, built, and ran against independently extracted static and shared package archives: 5/5 per linkage |
| Release contracts | 578-path release-tree manifest reproduced byte-for-byte; release-state, dependency, capability, provenance, public-surface, and negative fixtures passed |
| BLAS and Random drift | 229 BLAS coverage rows, 49 Random crosswalk rows, 33 Random decisions, and 55 Random product/data/build files regenerated without drift; negative fixtures failed as required |
| Documentation | 52/52 public headers, 1,084 documented public members, zero Doxygen warnings, 64 tracked Markdown files link-checked; HTML, XML, and tag file generated without machine-path leakage |
| ABI | GCC 11 CPU shared digest `c28bd7c30053d986543a29a3fd33ae945433e9f43777ea45d8ece2e544da14a5`; Clang 19 CPU shared digest `d46942f43db518dd83a2b2233258dc1b7f8d3f32f6312834da49d5ccf7185a7c`; GCC 11 CUDA shared digest `9a411b0034e9fbf843bec0b49132eca87ca7e74b1f9b1f44286b9a428694a600`; all observed SONAMEs are 0.9 |
| CUDA 12.9 static | 292 tests, zero failures/errors; 279 passed and exactly 13 declared `forced_no_device` return-77 probes were reported as skipped |
| CUDA 12.9 shared | 294 tests, zero failures/errors; 281 passed and the same exact 13 declared `forced_no_device` return-77 probes were reported as skipped |
| Compute Sanitizer | 18/18 real CUDA test executables had zero leaks; 17 had zero strict API/runtime errors |

The 13 CUDA skips are the complete, exact configured no-device probe set; all
device-present counterparts ran. They are expected conditional probes, not
supported-profile exceptions. No other skip occurred.

`core_cuda_native_state_test` deliberately requests an impossible allocation
to verify `ErrorCode::kAllocation`. Strict Compute Sanitizer therefore reports
that one expected `cudaErrorMemoryAllocation`. A supplemental run with API
error reporting disabled retained full memcheck/leak checking and reported
zero memory errors and zero leaks. No test was removed, filtered, or weakened.

## Public API, behavior, and numerical assessment

There is no change under `src/`. All 52 public headers received documentation;
a comment-insensitive preprocessed-token comparison found non-comment changes
in only four headers. Those changes are six approved diagnostic-string edits:

- `DenseArray` and `DenseView` diagnostics no longer carry the old M3
  development-stage prefix.
- Two Sparse owner/conversion diagnostics and two Sparse owner/builder
  diagnostics no longer carry the old M4 development-stage prefix.

No public signature, type, enumerator value, control flow, allocation contract,
or numerical operation changed. Test-only allocator observation was made
sanitizer-aware without skipping the underlying tests. The intentional binary
metadata change is the approved `VERSION 0.9.0` / `SOVERSION 0.9` policy.
ASCCpp-owned binary targets also map source/build roots to stable release names
and use origin-relative build RPATHs; this removes machine paths and
build-root-dependent ELF build IDs without changing symbols or execution.

## Package and dry-run artifact evidence

The secret-free ASCCMake acquisition pins commit
`8a7dcbad3a97267cce59810aff24de800a3497a7` and archive SHA-256
`67765391bef06c6c9a1a0c43e934d0db7a9c52876e66e0cf644042eeb0c2a5c9`.
The exact local override and repeated anonymous public acquisition passed.
The public archive contains the same 100 file payloads as the previously
audited private archive; GitHub's public packaging has the recorded public
archive identity above.

The deterministic provider-free installed archives were built in two
independent external build roots and were byte-identical under the recorded
local GCC 11 toolchain. Their tar metadata uses the fixed release epoch
`2026-08-03T00:00:00+08:00`, so a later release-commit timestamp cannot by
itself change these local identities:

- static: 81 safe archive members, including 40 CPU public headers and five
  static libraries; SHA-256
  `3c90c3b7ab1b269b0f285ae8488f37460f6369c12f793591bafcf0ed30c247d1`;
- shared: 91 safe archive members, including the same 40 headers, five exact
  `.so.0.9.0` libraries, and ten contained SONAME symlinks; SHA-256
  `9a9397632e6fbe196f47cfd9bbf66e95af02cc625dbf2d88bfdba5bf91024798`.

Both contain CMake package/version/target metadata, `LICENSE`,
`THIRD_PARTY_NOTICES`, the Joe--Kuo data and license, and all applicable CPU
components. Unsafe-link, hard-link, duplicate-path, special-member,
multiple-root, source-link, and forbidden-byte adversarial archives were
rejected. The archive inspector found no source/build-root bytes. Independent
extraction and installed-package consumption passed for both linkages.

The complete dry-run checksum set is held in the external `SHA256SUMS`, because
embedding checksums inside artifacts that contain this report is
self-referential. Every Gate B hash is worktree and recorded-toolchain
evidence, not an immutable publication identity. The release workflow must
regenerate all of them from the exact approved `v0.9.0` commit.

## Unavailable evidence and publication blockers

- `AI4SciComp/asc-cmake` is public. Its annotated `v0.1.0` tag peels to the
  approved commit, and three repeated anonymous downloads produced the exact
  recorded archive checksum.
- This repository is private. The GitHub APIs returned 403 for branch
  protection/rulesets under the current plan and 404 for the requested
  private-vulnerability-reporting and `release-publication` environment
  resources. These settings are not treated as verified.
- GitHub reports code scanning/Advanced Security, Dependabot alerts, and secret
  scanning unavailable or disabled for the current private repository. The
  release worktree contains pinned CodeQL and Dependabot configuration, but no
  hosted result exists for it.
- GitHub Actions currently allows all actions and does not enforce SHA pinning
  as a repository setting. Every external action used by this worktree is
  nevertheless pinned to a full commit SHA.
- GCC 14, the Clang 18 compiler matrix, Windows/MSVC, and macOS/AppleClang were
  not locally available. Exact clang-format/clang-tidy 18 were available.
- At Gate B there was no hosted matrix, CodeQL result, attestation, draft
  release, downloaded-draft consumption, or exact-tag result. Only later
  evidence bound to an exact reviewed commit can close those gates.
- The CUDA result remains experimental because no trusted hosted NVIDIA runner
  validated the exact eventual release commit.

There were no open pull requests or issues visible at Gate B. The source tree
contains no ignored build artifact, detected credential pattern, symlink, or
known machine-local path. The owner-authorized pre-existing ignored build tree
and unrelated ignored makefile were removed; raw validation material remains
external.

Public publication remains blocked until all repository protection, security,
hosted-matrix, draft-consumption, and later approval gates pass. No exception
is requested.
