# P05 general-tridiagonal reference slice

Status: approved bounded design; implementation in progress, no capability or
test credit. Scope is the exact 24 required S/D/C/Z GTTRF, GTTRS, GTSV, GTCON,
GTRFS and GTSVX rows. The full denominator remains 2113 required rows out of
3551 source records. All other structured and P04–P11 scope remains required.

## Isolation and inputs

Agent worktree: `../asc-cpp-p05-tridiagonal`, branch
`feature/lapack-p05-tridiagonal`, base
`b1b78d789fb8f5366f13f86c411832df842b2755`. Only new optional headers,
private adapters/helpers, focused tests and this review are owned. Shared
headers, enums, factories, CMake, ABI, mapping and program state are untouched.
The preceding indefinite-expert worktree and frozen handoff remain unchanged.

External root is `../asc-cpp-evidence/lapack-array-io`; this slice uses
`p05-tridiagonal-01`. Exact selected source records are in
`p05-tridiagonal-01/upstream-rows.json`. Reference source commit is
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`; source-input identity is
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
Inventory SHA-256 is
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`.
LP64 provider identity is
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
true ILP64 provider identity is
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
These remain GNU11/Linux static development subsets, not full profiles.

Actual repository instructions, runbook mission/protocol/architecture,
P01/P04/P05/P11 and numerical/evidence cross-cutting requirements were reread.
Live [Google C++](https://google.github.io/styleguide/cppguide.html) and
[Python](https://google.github.io/styleguide/pyguide.html) guides were retrieved
2026-09-08; their external HTML SHA-256 values are respectively
`f681e8c1b71ed5f2420555a28b7e7120f46914cfa126e9d8ec5e6e9851512caf`
and `9b02fa0d1aa05bfc8a4b5a95d3594665124f7a0414c82a69359f4a0b2f65e1c0`.
No MdeCpp source/tests/tables/prose are used. Implementation-design approval
does not complete external owner/license gates.

## Frozen design direction

Reuse `LapackTridiagonalView<T>` for contiguous, pairwise-disjoint DL/D/DU
and `LapackTridiagonalLuStorage<T>` for explicit DU2 fill-in. No full or band
densification occurs. A new provider-specific nominal pivot type preserves
adjacent one-based sequential swaps: p[i] is i+1 or i+2 in zero-based indexing,
and the last pivot is n. It is not GETRF, a final permutation or block pivots.
The generic family enum is unchanged and reports explicitly leave
factor_family absent. Successful borrowed factor construction admits only
actual successful GTTRF provenance, validates context before reading values,
and retains caller common-origin/lifetime obligations. Reports cannot prove
that mutable buffers still contain their originating numerical values.

Typed, explicit provider query/execution operations cover actual N/T/C solves,
one/infinity-norm condition estimation, refinement and FACT=N/F expert modes.
Queries read metadata, not numerical or pivot values. Execution revalidates
borrowed pivots. Caller kInteger contains n actual provider pivot integers;
real estimators append a separate simultaneously live n-integer estimator
array. The initial LP64 run correctly failed because kPivotConversion is
fixed ASC index_t storage, not a provider-width slot; that log is preserved.
Parent review approved the established combined native-integer region,
without changing foundations or reinterpreting ASC index_t objects.
Fixed CON requires
2*n scalar entries; RFS/VX require 3*n real or 2*n complex plus n underlying
real entries. Caller-only RHS/X row-major packing preserves padding.

Source S/D GTSV enters back-substitution at B(N,1) even for NRHS=0, whereas
C/Z uses a zero-iteration loop. The approved bounded accommodation supplies
n explicitly planned caller scalar kScratch entries, initialized before the
real zero-RHS call. Actual NRHS remains zero; this is not a substituted
one-RHS problem, patch or hidden allocation. Public empty B stays untouched.
Proof must include source paths, direct foreign counters, red zones,
insufficient scratch rejection and singular controls before credit.

GTTRF completes raw factors even on positive INFO, while GTSV may stop during
elimination and does not retain GTTRF multipliers/pivots. GTSV output is never
certified as reusable GTTRF storage. GTSVX FACT=N uses actual GTTRF and returns
before X/FERR/BERR on singularity. Its prose says incomplete factorization,
but actual GTTRF completes: preserve and document the source behavior rather
than rewriting it. INFO=n+1 retains computed warning outputs, not a successful
factor certificate. Raw CON accepts singular factors and preserves RCOND=0;
active solves/refinement/FACT=F reject exact zero factor diagonals pre-call.

## Resumed implementation checkpoint

The preserved worktree was recovered at the original base without resetting,
stashing, importing or overwriting files. The initial 21 owned files and all
existing raw diagnostic log hashes are recorded in external
`p05-tridiagonal-recovered-01/recovery.json`; its complete `source/` is frozen
before resumed edits. D022's native INTEGER correction was already present.
Historical LP64 20/20, ILP64 21/21 and LP64 fault 4/4 logs did not bind the
later complete source and are not relabeled as candidate verification.

Fresh recovery runs select all 29 registered tests in both true ABIs. Each
executes 25 passes and four failures, with no skipped tests. The failures are
the preserved independent extreme mathematical checks, not missing tests.
The LP64 runner invocation omitted its optional CTest-summary flag but retained
the actual 29-case JUnit output and command exit eight; the ILP64 record also
parses selected/executed/pass/fail/skip counts. These are source-identified
recovery diagnostics, not a passing handoff.

Independent exact-prototype direct source probes are frozen under
`p05-tridiagonal-source-gates-01`. Both ABI compiles pass. All eight scalar/ABI
processes return normally and fail four mathematical checks for the scalar
`A=B=min_normal/8, X=1`: GTCON returns zero instead of analytic RCOND=1,
GTRFS returns INFO=0/FERR=Inf, and both GTSVX FACT modes return INFO=2,
RCOND=0/FERR=Inf. Unit, minimum-normal and large finite controls pass their
separate assertions. GTCON's source uses unscaled inverse solves; its scalar
inverse overflows even though the exact condition and solution are
representable. GTRFS's error-estimator path likewise exposes overflow. These
are confirmed source discrepancies, not ASC success or permission to weaken
the retained mathematical tests. Public contracts now describe the precise
source limitation and raw warning behavior. No alternate algorithm, provider
patch, global handler change or hidden scaling is introduced.

The initial strict Clang18 production checks found missing direct includes,
copy-preventing local const qualifiers, grouped declarations and an oversized
expert-driver function. Resumed edits correct the includes/declarations,
permit ordinary Status moves and extract the unchanged GTSVX publication
logic into a private helper. The 20 owned C++ files are formatted with the
repository Clang18 configuration. Strict revalidation remains the next gate;
the original format/static failures and mathematical failures stay frozen.

## Next task and remaining gates

Implement the owned typed headers, checked source-integer/workspace helpers
and actual calls, then independent factor reconstruction/residual/condition/
refinement tests, negative and injected-fault tests, direct LP64/ILP64 ABI,
first-call dynamic allocation observation, source allocation closure,
sanitizer/strict/no-exception/header/Doxygen gates. Freeze exact source and
evidence for root review/import. Integration/installed tests, central ledger
credit and actual owner/license gates remain separate. No passing capability,
commit or remote write has occurred in this slice.


## Root V11 import

Root imports all20 corrected source files byte-for-byte from `p05-tridiagonal-sentinel-01-vskus1c2/source-fixed`, preserving the original04 review above and the separate13-file correction review in `tridiagonal-info-review.md`. The independently reviewed source patch SHA256 is `7995df010e289ce6b534f140bcb14c0ca95d4972065cbd7a369a600921c0a1f4`; all404 artifacts and the corrected33test lane identities were checked in public-consumer04/root-review-audit.json. Root public consumer04 SHA256 `9379b9395ba69f32f6e6e5768ea4a9d14137862c6eed9fd97fcaaea1e4aebf65` passes both actualABIs and strict with original recorded base dependencies. Its current installed execution is pending.

All24 rows are in_progress, not verified; profile incremental-lapack-v11 has230partial reference rows,1883notstarted,2113required,20nativeimplemented_unverified. Four new headers make97public headers and15installed consumers. Root adds33 ordinary tests including the four required extreme mathematical failures and8header probes. Current full provider test count should be550, verified only after actual CTest listing/execution. No owner failure, test body, source gate or denominator is weakened.


## V11 executed implementation checkpoint

Frozen V11 tree142997ad4a4bffdf612d0a0171e9da7391a05206 archivee8ca3aa367f36ef3086edc8bf6f0adfd3c3feaba9915bd3242f19f8d26a44b0a mapping7a87e2cc08c7cb647e261c1566d5f447831d10c3c2a709bbdc6ca5f7eb59aaca. Current all-ASC-C++ ASan bothABIs each46/50: exactly4required GT extreme mathfail,zero skips; all new source,public consumers,Sylvester correction and STOP controls execute. Doxygen97headers1911members0warnings,format25Cpp,coverage/publicsurface/Markdown pass. Full5lanes and strict15TUs/ABI are running against immutable01; no final integration/full-suite pass claim.

Local implementation checkpoint may precede completion of the immutable full/strict jobs. Their recorded source remains authoritative; later Band20 changes are a separate candidate and cannot inherit a full V11 pass. No required mathematical failure is accepted as success.
