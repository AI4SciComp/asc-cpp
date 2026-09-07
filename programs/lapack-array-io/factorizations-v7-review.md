# V7 integration review (listed integration gates passed)

Candidate01 is frozen as tree `cdc2628229316deac47eed7fed0c18df150d1672`,
archive `e3e4a62b684927b294c23600da930f8ca98cd499e6f6eff45f222c863f536ad8`.
Its Doxygen check passes87/87 headers,1742 documented public members and zero
warnings. Both sanitizer builds fail linking band test targets: the root's
registration omitted the required existing `allocation_audit.cc` and allocator
wrapping; indefinite tests have the same missing dependency. No sanitizer tests
ran. The same registration error affects the normal provider builds. Failed
logs/harness01 remain retained under `p05-p06-factorizations-v7-01` outside
source. Corrected candidate02 adds the required test support and link options;
no adapter, public header or numerical assertion is changed for this error.

Candidate01's completed provider-free selections pass Debug277/277,
Release277/277 and shared279/279, all zero skips. Both ordinary provider
builds, like both sanitizer builds, actually fail linking the allocation audit;
no provider or sanitizer CTest pass is claimed for that snapshot. Strict Clang18
finishes all26 affected translation units per ABI: LP64 passes26, ILP64 passes23
and fails3 because each includes the same exact primitive `long` ABI assertion.
The assertion itself is preserved; root adds a narrow `google-runtime-int`
exception and explanatory FFI comment, keeping the compiler-emitted primitive
type comparison rather than weakening it to an equal-width alias. Owner frozen
files and failed logs remain unchanged. This two-comment-line amendment and its
18 mapping artifact hashes require a new candidate03 verification identity.

Candidate02 tree `d608f1626619a9bde22c4c29cee0719b5594b5cc`, archive
`5ff35a1ab299309620a5de072cbc9aa709d7ec8642f4e7774731f51ce4663c5b`,
contains only the allocation-audit registration repair relative to candidate01
product code. Both full provider suites pass415/415, zero skips; its Doxygen and
Markdown checks pass87 headers/1742 members/zero warnings. Candidate02 does
not contain the later private ABI-header comment correction, so its actual
results must remain separately identified from candidate03.

Candidate03 is tree `a7aa1b35798b6e7062be22990895360232fb4ef4`, archive
`07d4991e7fae66b51696e769f64cc7676c5dbed4e05ed49f945b9869d80ee052`.
Its87 archived public header hashes were rechecked successfully. Root started
the first auxiliary strict/sanitizer attempts before creating this candidate's
generated `lapack_build_config.h`; both sanitizer builds fail and early strict
checks fail for that missing prerequisite. Those attempt01 failures are retained
and are not numerical or successful verification evidence. Each initial strict
ABI run finishes21 pass/5 fail for that missing header. After both full provider
configurations generated their actual ABI headers, auxiliary attempt02 passes
on unchanged candidate03 source. This is a runner sequencing correction, not
a product fix.

All five exact candidate03 GNU configure/build/full CTest lanes pass:
LP64/trueILP64 each415/415, provider-free Debug/Release each277/277 and
provider-free shared Release279/279, zero skips. Root explicitly read both
relocated installed logs: each executes all ten public-only consumers, including
the three new families. The unchanged source also passes26 affected strict
Clang18 translation units per ABI, clang-format18 for46 changed C++ files,
Doxygen87 supported headers/1742 public members/zero warnings and Markdown.
Affected Clang19 ASan/UBSan attempt02 passes46/46 per ABI with all CPU
Core/Dense/registered-reference C++ instrumented; Fortran/BLAS archives and
dynamic runtimes are not instrumented. These are scoped command gates, not
full routine/mode closure or independent foreign sanitizer evidence.

Nested `build-<abi>/asc-cpp-test-workspaces/reference provider package/`
`lu-families-test.log` identities are:

| Snapshot | ABI | SHA256 |
| --- | --- | --- |
| V7-02 | LP64 | `2408a951f980558543573acde2a8ffae12ad2932aa634fdd8b30a9b6cedc01a0` |
| V7-02 | ILP64 | `3cf948f1df00e27933a84a47f6db61c132f982d80569df0eebcd76a2b08e9298` |
| V7-03 | LP64 | `5b4ba1ec9adbb48c440f659aa828b3b9ef527001ef4f2e6836c7e9d9eebed822` |
| V7-03 | ILP64 | `cbc65a1cbd1cff40275bb0b0222b34e72e097163984c8bccb97bee55014dbde5` |

The [checkpoint index](verification-checkpoints.json) preserves hash-validated
completed command records, including every failed earlier attempt. Candidate03
mapping SHA256 is
`80e087f4826466894f7d623121c7c45f279bbcbcb938b2c8544aecaeea503259`.

This unreleased working integration extends committed v6
`31e93935f8db4ac685c2224abf7860a058040117`. It registers twelve band Cholesky,
eighteen classic indefinite and eight rank-revealing reference routes, not
complete P05/P06 families. All2113 required rows remain incomplete:162 are
in-progress,1951 not started, zero fully verified. Twenty native operations
remain implemented-unverified and are not reference coverage. The87 supported
headers include three new optional Dense-owned provider headers, never an
aggregate dependency or seventh module. Candidate01/02 mapping SHA256:
`cdb70b4ab596342ae968d9b075c986c84815f264f1b1e985a75adcae26f2999d`.

Root reviewed the complete frozen code and test files and full reviews before
import: fourteen band files with the eight-file ignored-imaginary amendment,
fourteen indefinite files and fifteen rank files. Rank's four authorized
foundation additions append a distinct factor enum and provide final column
permutation validation/conversion; the prior enum values and LU semantics are
unchanged. All implementation artifact hashes were refreshed after these
additions. Exact owner identities, source audits, retained failures and scoped
test limitations remain in the [band review](band-cholesky-review.md),
[imaginary-component amendment](band-imaginary-review.md),
[indefinite review](indefinite-review.md) and
[rank review](rank-revealing-review.md).

The root ran the rank candidate02 read-only evidence audit:31 command records
match frozen source content
`0da250f905dc402440383795819076dae3e4ebf8f848da48260052b53e1d6baf`
and archive
`75d2f50c80b06e1c03522a9133eb051782f2ff1dde2a3d241911f103e5b736e0`.
Eight actual ABI/configuration scoped selections pass17/17 each, zero skips.
Their720 successful scalar/routine profile events per lane are not720 routines
or universal mode certificates. Original zero-row local-substitution candidate01
is not imported or credited to the corrected source. The independent
fixed-zero-column GELSY mathematical gate remains failed.

## Root-authored public consumers

All files below use only public ASC headers and local public-only support.
Initial direct diagnostics link the reviewed new adapter with older existing
ASC archives; they are distinct from the now-passed relocated package checks.
Rank also
compiles the actual amended provider-free foundation source. Both true ABIs
and strict Clang18 pass the stated final attempts. No foreign archive is
instrumented in these direct diagnostics and no complete allocation claim is
inferred from using caller arrays.

| Consumer | Final source SHA256 | Direct attempt |
| --- | --- | --- |
| Band Cholesky | `9a9ef42ff55981fdf31cd37d59ad2a454df3ecbbcd1bf4fd6e900c377fdb99e8` |03, amended provider source |
| Classic indefinite | `e07b930ba0fb46ad0aa419a91f9cfea3625c9d550def9e85059efe865af10175` |02 |
| Rank revealing | `4e06ad17d218e9fd929b8702e86e287f1f6de2615ba6fa7bd360cf3a5cedf15a` |02 |

Raw logs remain under the external evidence root's `logs/` directory, using
`band-consumer-*`, `indefinite-consumer-*` and `rank-consumer-*` names. Earlier
strict failures remain retained. Indefinite's unchanged solve checks were
extracted from an oversized test function; rank's unused/missing direct includes
and nested conditional were corrected without weakening assertions. Band
attempt03 uses the corrected ignored-component adapter, unlike attempts01/02.

Band checks include blocked/unblocked reconstruction, solves/reuse, padding
and source-specific partial imaginary-diagonal publication. Indefinite checks
include exact paired-pivot 2x2 block reconstruction and known-solution residuals
with immutable reusable factors. Rank checks reconstruct A*P by applying
reflectors from the left in reverse order, exercise the public permutation
APIs, and check a known rank-two least-squares solution, normal residual and
known nullspace under independent layouts and minimum/preferred workspaces.
Insufficient-workspace failure preserves rank, A, B and pivots before foreign
entry. These finite fixtures do not close upstream exceptional numerical modes.

## Remaining verification and scope

All listed integration lanes above bind the same frozen product identity;
root compared every tracked product file outside `programs/` with that archive
before committing. Preserve those results separately from later slices and
complete exact contract/mode-normalized evidence. The root
indefinite ABI probe executes real symbols and full-width sentinels; exact
compiler-emitted prototype comparison remains the separately audited external
lane, not a claim that integration regenerated those prototypes.

New band experts, indefinite experts and SVD least-squares are separate active
slices, not part of this frozen review candidate. Remaining structured,
orthogonal, advanced SVD/spectral/matrix-equation, specialized/XBLAS/deprecated
and full P11 gates are still required. Known mathematical, process-safety,
shared-provider/platform, owner-review and license gates are not waived.
