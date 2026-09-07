# Verification summary

No ASC LAPACK numerical routine or C++ array persistence capability is
verified at this checkpoint. The separate verified foundation slices are:

| Slice | Actual evidence | Scope limit |
| --- | --- | --- |
| Frozen provider-free baseline | Debug and Release each 219/219 passed, zero skips | Commit `46412183b2ae86101b2361c52376a8db8efff264` only |
| Complex Dense storage snapshot | Debug/Release each 27/27 Dense/Random tests; ASan+UBSan 3/3; GCC11/Clang19 no-exception compilation | Snapshot `49665b0ae3636df234acbd94c3cb62f66639e7b2604d14789c47a9ca9a3c4959`; combined-tree integration still pending |
| Source inventory | 17 extraction tests and exact offline regeneration pass | 3551 qualified identities; 2113 required; reviewed ASC semantic mappings still pending |
| Coverage validator | 28 independent synthetic tests pass | Rejects missing rows and unsupported verification claims; not numerical evidence |
| Independent ASC format codec | 16 Python fixture tests pass | Normative/fixture oracle only, not production C++ parser, bounded-memory or rollback proof |
| External reference LP64 dependency | Release static, all four precisions, 111/111 upstream CTests passed with zero skips | No ASC wrapper evidence; XBLAS disabled; true ILP64 not tested |

External indexes: `baseline-46412183/index.json` and
`p01-scalar-02/index.json` beneath `../asc-cpp-evidence/lapack-array-io`.
Upstream logs are under `logs/lapack-lp64-*`; final dependency attestation is
pending. Native LU, checked foundations and bounded printers are under active
implementation/integration, not completed package claims.

## Integrated foundation/display checkpoints

The first implementation commit is
`a797316baffc223bb58acbf3ec6d7539e168b0d9`, tree
`bab2720c9069e613428a827fe5d9c52234a7fd65`. Its new foundation and display
code was tested in tree `832edcc11bd1d29f5cce7a93d3989c1b308669de`:
Debug and Release each passed **244/244 CTests with zero skips**, including
installed component isolation, BLAS and Random regressions. The later tree
adds only explicit Doxygen return tags and matching ABI hashes; strict
Doxygen then passed **60/60 headers, 1199 public members, zero warnings**, and
the link checker checked 73 Markdown files. These exact identities are kept
distinct; no full-suite run on the documentation-only revision is inferred.

[Sanitized checkpoint records](verification-checkpoints.json) retain actual
counts, command exits, source content identities and artifact hashes. The first
live-tree diagnostic run failed 9/241 integration checks (Python working
directory and changing/stale header inventories); these failures were fixed,
not disabled. The first strict Doxygen run failed 46 return-tag checks, which
were corrected without reducing documentation strictness. Failed logs remain
external alongside passing evidence.

Both LP64 and true global ILP64 reference dependencies now pass 111/111
upstream CTests. Their source is the same exact pinned commit. Build identities:
LP64 `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
ILP64 `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
XBLAS is disabled in both; ASC ABI, foreign allocations, runtime/link-map,
provider component and routine/mode gates remain separate and pending.

All **2113** required numerical rows currently remain unimplemented in the
coverage ledger, with zero reference/native callable or verified rows. No
advanced/specialized family, optional dependency or legal mode is removed from
the full-profile requirement.

Required gates remain the complete P00–P11 runbook, including native and
reference numerical reconstruction/failure tests; LP64 and true ILP64 provider
ABI tests; array malformed-input/rollback/resource tests; no-hidden-allocation,
transfer or densification evidence; BLAS/Random regressions; installed component
isolation; strict full-profile closure; platform, sanitizer, docs/style and
provenance evidence. Zero tests, missing tools and unexpected skips are failures
or missing evidence, never passes.
