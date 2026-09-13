# P06 GELSS/GELSD implementation-owner review

This is an implementation-owner self-review, not repository-owner approval.
All eight typed S/D/C/Z GELSS/GELSD adapters are implemented and frozen as
candidate02, with the bounded verification below. The eight actual pinned
driver rows remain partially verified in the full denominator: upstream
source-safety/output gates remain open. This is not a complete P06, P07,
provider-integration or repository-owner approval claim.

## Identity and ownership

The isolated worktree is `../asc-cpp-p06-svd-least-squares`, branch
`feature/lapack-p06-svd-least-squares`, based on the tested integration commit
`31e93935f8db4ac685c2224abf7860a058040117`. This owner changes only the new
provider header, adapters/private helpers, isolated tests and this review.
The integrator owns all registrations, ABI/manifest changes and combined tests.
The preceding rank-revealing candidate02 worktree and evidence are immutable.

The exact upstream source commit is
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` (Reference-LAPACK 3.12.1).
All eight complete sources were read, including precision-specific query and
empty-case differences; the inventory independently records S/D/C/Z GELSS
and GELSD as required public drivers. The live Google C++ and Python guides
were retrieved on 2026-09-07. Existing repository compatibility rules apply.

## Confirmed source discrepancies and incomplete gates

The immutable external evidence directory is
`../asc-cpp-evidence/lapack-array-io/p06-svd-ls-source-probes-01`.
It contains two successful compile records and forty executed direct-provider
process records, not ASC CTests or completed typed-route evidence.

- Nonzero A with GELSD NRHS=0 passes the driver's argument check, then reaches
  LALSD, which rejects NRHS<1 through XERBLA. All four scalar variants in both
  true ABIs print the argument-4 error and terminate via Fortran STOP before
  normal return. The operating-system exit is misleadingly zero; the explicit
  return-marker gate exits one for all eight cases. The reproducer is
  A=diag(1,2), m=n=2, with real, sufficiently sized storage and actual queries.
  This unsafe mode must reject before ASC mutation and remains incomplete;
  no fallback or global error-handler override is permitted.
- Safe zero-A/NRHS=0 and m=0/n=3 GELSD controls return normally, all eight
  scalar/ABI cases each. These distinctions must not disappear in a blanket
  empty-input rule. GELSS with nonzero A and NRHS=0 still computes S/rank;
  all eight direct-provider controls return normally.
- GELSS documentation describes rowwise right singular vectors in A, but its
  wide high-workspace Path2a leaves LQ factors there. For
  A=[[1,0,1],[0,2,0]], B=[2,6], all eight scalar/ABI calls return INFO=0,
  rank=2, S approximately [2,sqrt(2)] and X approximately [1,3,1]. The claimed
  row-orthonormality condition fails by approximately 3.220649469, far beyond
  scalar-epsilon tolerances. These eight mathematical gates exit one. The
  adapter must expose actual overwritten A, not manufacture V^H or claim this
  required output gate passed. Independent solution verification continues.

Source probe SHA-256:
`c11fd7486147df8999d847cf57160b6a41f36a9c6c303c6eba38d0e3b4189d11`.
Normal-return gate SHA-256:
`764f3bd3426d651c884377f2ed3c92107cc3b8edb304a20628944be9ce2d425f`.
LP64 executable SHA-256:
`230a2ff757055181be1cda5a24772d9bb7a0a9a3eab8fcbb290c821b371d848b`.
True ILP64 executable SHA-256:
`a7484001a11a6c11ee9c4f8a79e3db4aae5404667370a898b5ee9cfad729dc7d`.
Exact commands, exits, binary manifests and log hashes are in each unique
`logs/<mode>-<abi>-<scalar>/record.json`.

## Single-real divide-tree storage gate

The independent actual-SLASDT probe is retained under
`../asc-cpp-evidence/lapack-array-io/p06-svd-ls-levels-probe-01`.
At N=212992 and SMLSIZ=25, both true INTEGER ABIs return LVL=13,
ND=8191 and a largest bottom subproblem of 26 rows. Both source-storage
gates exit one; actual DLASDT controls pass. This is a tree-storage failure,
not a claimed full-driver residual failure. LALSD reserves U with 25 columns
and VT with 26; LASDA's 26-row left leaf calls LASDQ with U's 26 columns and
VT's 27 columns, crossing those reserved internal slices.

Every numerical split can create a smaller subproblem. The checked execution
therefore rejects nonzero S/C GELSD A with min(m,n)>=212992, even if the full
size's logarithm happens to be benign. Actual queries and all-zero A keep their
safe paths, subject to ordinary INTEGER/storage checks. This is an explicit
incomplete required mode, not a substitute algorithm or full capability claim.

The proof evaluates every 212966 possible nontrivial subproblem N=26..212991
using the exact single-REAL Fortran expression on the attested runtime. All
have maximum bottom rows at most 25. In addition, 238 actual pinned SLASDT
calls cover ±8 around each of the fourteen relevant 26*2^p transitions,
including the failing boundary, in both true ABIs. The maximum leaf formula
is independent integer reasoning: splitting q rows creates floor(q/2) on the
left and no larger right child, so the all-left branch attains
floor(N/2^LVL). Actual tree outputs match this formula in every checked
transition case. Both `logs/proof-<abi>-01` records pass; they do not erase
the preserved `logs/tree-<abi>-s-01` failure.

Tree probe source SHA-256:
`2028475121d5545424e3e1ffeead1c4d5a6a9577f69cc913c07e82ddd7a66d11`.
LP64 executable SHA-256:
`4d56be58e5b1357e46c4f249c064c2cbda47a52bf15d401573d4e06322775122`.
ILP64 executable SHA-256:
`c5e40c77aac8aeea83182ccf0b1ca5761f7bb663859983fe501acb7076e10650`.
The first diagnostic Fortran link attempts failed because the local toolchain
alone did not resolve liblto_plugin; adding the existing system GCC support
search path corrected them. Both failed records remain present.

## Meaningful nonfinite input

The immutable direct-source probe under `p06-svd-ls-nonfinite-probe-01`
executes 64 subprocesses: two drivers, four scalars, both ABIs, and NaN/Inf
in A or B. All sixteen GELSD nonfinite-A cases stop in LASCL's argument-4
handler: OS exit zero but missing normal-return marker, so the gate exits one.
The other 48 calls return, but that is only source-return fidelity. For
example DGELSS A-NaN returns INFO=0/rank=0 with NaN S and solution; it is not
an observed numerical success. No global error-handler override is used.

This checked API now preflights finite full A and, unless A is all zero,
only the first m meaningful input B rows. Rejection is code-only kNumerical,
kNotRun/kUnchanged, absent foreign INFO, and no numerical/workspace mutation.
Queries do not read numerical inputs. Empty operations and all-zero A do not
read semantically ignored B; S, output-only B slots and padding are not
finiteness inputs. Finite input is not a universal computed-finiteness or
conditioning certificate. This policy is distinct from injected INFO tests.

Probe source SHA-256:
`5a568b7dc62be66ffb7635740209b177e258989faa8216afe40cc240470078d5`.
LP64 executable SHA-256:
`34784a53fb10c71e93b5a8eaa1ebd85d41002fcf495d63e81419ea5ce9b38cae`.
ILP64 executable SHA-256:
`9e9e2fc94faad52df1609865e5cd8134d6f8ce959d0f54da99473881b807c21f`.

## Source-derived implementation contracts

A is m-by-n; B has exactly max(m,n) rows including all solution capacity.
Only the first m rows of B are numerical input. S is contiguous underlying-real
storage of min(m,n) entries. RCOND remains an actual finite scalar, with exact
bit-pattern plan binding. GELSS uses machine precision only for negative
RCOND and also applies the source safe-minimum floor. GELSD's nested LALSD
actually replaces RCOND<=0 or RCOND>=1 with machine epsilon; the adapter must
preserve and document that discrepancy rather than silently change the cutoff.

Queries use actual source routines, verify scalar/integer/real query outputs,
and preserve all numerical operands. Scalar WORK, complex real WORK, foreign
INTEGER WORK, staged S and explicit row-A/all-B layout storage are distinct
caller-owned roles. Every nonempty workspace region must be provider-host
accessible. No implicit allocation, packing, transfer, synchronization,
precision change or alternate driver is permitted.

Successful calls publish rank/S and solution rows, plus documented tall
full-column-rank residual coordinates. Residual coordinates are not the
original residual vector and may retain source scaling. Nonconvergence keeps
raw INFO and available raw A/B/S intermediates, but does not publish a valid
rank/solution certificate. Structural errors leave every numerical destination
unchanged. Exact report and ignored-tail policies are implemented and checked,
including separately identified injected foreign failures.

## Preserved diagnostic history and safe real minimum

The first LP64 Release numerical diagnostic passed 12/12 CTests, zero skipped,
in `../asc-cpp-evidence/lapack-array-io/p06-svd-ls-diagnostic-01/logs/`
`test-lp64-release-01`. It exercises independent rational unitary bases with
known singular values, solution, optimality/nullspace conditions, rank-zero
and deficient modes, scalar/blocked dimensions, both independent A/B layouts,
minimum/preferred storage, scaling and guarded caller allocations. These are
ordinary observed tests, not extreme-count safety or complete source closure.
The initial build failed on the test helper's wrong memory-view type; its
record remains preserved. The corrected second build passed.

Pure scalar/real/integer preferred query counts match 4,608 actual source
queries per ABI in `p06-svd-ls-counts-diagnostic-01/logs/test-<abi>-02`.
The earlier LP64 query comparison failed 47 ZGELSS zero-RHS cases; exact
ZUNMLQ versus CUNMLQ query differences corrected the helper. These process
comparisons are not execution/math tests or CTest counts.

A second immutable direct-source probe at `p06-svd-ls-source-probes-02`
confirmed real GELSD's advertised MINWRK=739 for m=1,n=1000,nrhs=1 enters
GEBRD with inadequate storage. All four S/D-by-ABI normal-return gates fail;
safe LWORK=1002 returns the independently known uniform minimum-norm solution.
Complex controls at their genuine minimum 1002 also pass. No error-handler
override occurs. The ASC plan now raises the real minimum to the smaller of
the safe fallback requirement and complete source Path2a threshold, never
below source MINWRK. Actual all-scalar ASC wide-at-minimum regressions now
pass in both ABI Release lanes. Adjacent pure minima at n=736..740 and
synthetic INTEGER/rounding/cursor boundaries also pass without fake backing.

The latest pre-finiteness-change checkpoint is `build-<abi>-release-06` /
`test-<abi>-release-04`, each 21/21 CTests, zero skips. It adds exact-prototype
GNU foreign-boundary injection, malformed scalar/real/integer queries,
negative/INT_MIN/impossible-positive INFO, invalid rank, and selected A/B/S
publication with rank withheld on failure. Injected failures are not claimed
as naturally occurring source nonconvergence. Actual per-routine query and
execution counters now accompany the independent mathematical checks.
The meaningful-input finite scan, NaN-padding and real-backed singleton
extreme-stride regressions subsequently passed in both ABIs at
`build-<abi>-release-07` / `test-<abi>-release-05`, each 21/21, zero skipped.
Those logs retain their own exact source identities, not relabeled older
records. Candidate01 separately passed 21/21 Release CTests in both ABIs,
strict Doxygen and self-contained header compilation. Its strict production
analysis found a private helper over the function-size limit and duplicate
branch bodies. Candidate02 extracts the wide workspace formula and merges
the equivalent row-cursor condition, without suppression or formula changes.
Candidate01 and its failures remain immutable.

The minimum probe source SHA-256 is
`9312992958e7cf342038ecb8de25e07c5b040eebf1124cab79f9a3e9df35b77a`.
For real wide GELSD, the safe minimum is the source minimum enlarged to
admit either fallback GEBRD's `3*m+n`, or the complete Path2a threshold
`m*m+4*m+max(m,2*m-4,nrhs,n-3*m,WLALSD)`, when the crossover permits it.
Every larger LWORK up to the checked preferred capacity then either supports
fallback or selects a fully provisioned Path2a. The original preferred query
is recorded unchanged. Pure adjacent boundaries and actual all-scalar
1-by-1000 minimum-work execution are separate evidence.

## Candidate02 identities and observed verification

The immutable evidence root is
`../asc-cpp-evidence/lapack-array-io/p06-svd-ls-02`.
The full source archive SHA-256 is
`2a300b834c5d1681384ab45f5c9c3c39fdfd703f95f5a5382648cad60aa25d3c`;
its evidence-runner content identity is
`8a29beab22a9aed57ecced8f21e9387fe03a78f8be6332a405405a0fff06c17b`.
This final review is a later documentation-only update; the archive keeps
the earlier review checkpoint and is not rewritten or relabeled.

Production SHA-256 identities:

- `include/asc/dense/providers/lapack_svd_least_squares.h`:
  `dc5e912bce2aabae157be5351b1cb6afc3f9813b241c2b7d9b506a26ffe3d5b6`.
- `src/dense/lapack/reference_svd_least_squares.cc`:
  `155cc45b6bd5b552ff6645ebbfc1b59ccc3f4ddb56fd62f436f03c211f9b08ad`.
- `src/dense/lapack/internal_svd_least_squares_counts.h`:
  `c4c67de4c02499b92e291a52e355b1def75df4b707afb372fa8e8f6dc1a19ac7`.

`verification-ledger.json` SHA-256 is
`ac9629e10e810eff65900dd164c5f498a6f2f587f1669f55c79161f8edf3309a`.
It checks all forty candidate02 command records, raw artifact hashes and
unchanged source identities. It records every owned code/test hash, all
linked archives and diagnostic executables, both exact provider configs,
all eight upstream driver hashes, and the complete static-closure source
hash set. The separate final audit record is
`p06-svd-ls-handoff-01/logs/ledger-01/record.json`.

All eight runtime lanes pass 21/21 CTests with zero skips: LP64 and true
ILP64, each in Debug, Release, Clang19 ASan/UBSan and Linux libc-interposition
configurations. Actual JUnit stdout contains 1,896 successful numerical
profile records per lane: 472 in each scalar's mathematical process and one
in each of eight first-call scalar/driver processes. A profile is a tested
configuration, not a new upstream routine or separate CTest. Each lane also
includes four contract tests, four injected-failure tests and one pure-count
test. Every libc process confirms both direct libc and shared libstdc++
allocation positive controls; all 21 do so in each ABI lane.

The mathematical oracle constructs rational unitary bases with independently
known singular values and minimum-norm solutions. It checks rank-deficient,
rank-zero and full-column-rank cases, inconsistent tall optimality via
original A^H residuals, wide nullspace orthogonality, multiple RHS, both
independent A/B layouts, minimum/preferred workspace, tiny/huge scaling and
padding. Raw source/prototype fault tests separately retain exact INFO,
query/execution call counters and selected-output publication. No provider
parity result substitutes for the mathematical oracle. No naturally occurring
finite-input nonconvergence fixture was found; injected nonconvergence is
not presented as such a fixture.

Contract tests include meaningful nonfinite rejection versus ignored NaN
output/padding, exact empty and zero-A paths, original huge ASC64 singleton
strides with real backing, every workspace role's short/placement failures,
inaccessible nonempty unused roles, rank/report and live workspace/metadata
overlap, S stride/shape, operand aliasing, stale cutoff bits and modified
plans. Structural and metadata failures preserve the relevant caller state.
No synthetic extreme-size test fabricates backing storage.

Final pure query formulas match 4,608 actual pinned source queries per ABI in
`logs/test-query-counts-<abi>-01`; these are two standalone process records,
not additional mathematical CTests. Strict Clang18 production analysis passes
for both ABIs. Clang18 format for all ten owned C++ files, GCC11 C++20
self-contained/no-exception public-header compilation and actual Doxygen
1.9.8 warning-as-error generation also pass. Runtime targets compile with
`-Wall -Wextra -Werror -fno-exceptions`.

The sanitizer scope is explicit: ASC C++ adapters/tests and their selected
diagnostic C++ dependencies are instrumented; the exact pinned Fortran
archives are not rebuilt with sanitizer instrumentation. Real foreign buffer
guards still run. Linux interposition observes the declared malloc-family
symbols and shared C++ allocation calls, not every possible private runtime
allocator on every platform.

## INTEGER and conditional source-closure review

Query preflight covers the exact scalar-specific minimum/preferred/mixed
workspace formulas, ILAENV's default-REAL 1.6 crossover, DGELSD's distinct
empty query, S/C upward rounding versus D/Z/plain-real query assignment,
and every consumed floating-to-INTEGER conversion. Pure checked arithmetic
also covers dimension and workspace terminal increments, saved-L quadratic
thresholds including actual LDA, full B stride products, fallback GEBRD's
otherwise hidden preferred arithmetic and nested ORM/UNM fixed-T queries.
Actual execution caps caller LWORK at the checked preferred capacity.

The source-sensitive A cursor distinction is deliberate: GELSS's BDSQR
SCAL/SWAP walks entire VT rows even at order one, while GELSD's singleton
does not. Complex wide GELQF/GEBD2 conjugates a complete row. These are tested
with actual one-element backing and extreme legal leading dimensions, not
by claiming the backing contains a huge matrix. BDSQR's pinned iteration
bound is `6*k`, not the obsolete quadratic limit. Its LASQ branch remains
in the conservative static closure but is not selected by these driver
routes, which request right singular vectors even when GELSS NRHS=0.

LALSD/LASDA/LALSA/LALS0 tree indexes and real/complex temporary slices were
reviewed against source formulas. The integer workspace bounds cover node
tables and level products. Complex temporary slices cover both three-block
leaf multiplies and `k+2*nrhs+k*nrhs` secular-vector applications. The separate
single-REAL tree proof source SHA-256 is
`8022b770636fa9a59515c93c13489ca426666171f245ff822ea95819c126fc7d`.
Its source failure and conservative incomplete size gate remain as above.

Each ABI's conservative static graph reaches 257 objects; excluding only
XERBLA yields 256. Full and conditional graphs are both retained. Initial
closure gates failed on newly reached power/memory leaves and then on the
true-ILP64 power helper variants. Those failures were not ignored: final
`audit-svd-least-squares-closure-03.py` enumerates actual leaves, while
`audit-svd-runtime-leaves-02.py` traverses the pinned numeric machine-code
branches, including all decoded power/log IFUNC choices and their error
branches. LP64 integer powers reach libgcc's `__powisf2`/`__powidf2`; true
ILP64 instead reaches GNU runtime `_gfortran_pow_r4_i8`/`_gfortran_pow_r8_i8`.
The GNU string helper reaches only standard memory operations. Static graphs,
runtime branch audits and observed cold-call allocations remain distinct.

The XERBLA exclusion is conditional on the reviewed outer/nested structural
preconditions and supported source modes. Finite-input preflight prevents
the reproduced nonfinite-input STOP path; it is not a proof that arbitrary
finite, arbitrarily ill-conditioned arithmetic can never produce a new
internal exceptional value. No universal source-correctness or numerical
convergence theorem is claimed from these tests.

Final closure script SHA-256:
`134fc72ce996eb21af56f28f4f71ab972154ff2ee46950df4659c24e79d173d8`.
Final runtime script SHA-256:
`bf994ac22789bcd66f097ab268c163a2319166630145502d1b3f1a47f66b29b4`.
An intermediate runtime audit failed because the GNU string symbol required
its versioned ELF name; that log remains. The first sanitizer runner
invocations rejected an env wrapper before creating test records; the
corrected runs pass sanitizer variables through the runner environment and
record them in `asan-metadata.json`. Neither invocation rejection is a test.

## Remaining gates and exact next integration task

The integrator should review and import only the eleven owned new files,
atomically register the single public provider header, private adapter/helper
and isolated tests, then run installed-component and combined regression
gates against the resulting exact integration commit. The external
`p06-svd-ls-02/CMakeLists.txt` records all diagnostic source registrations,
the eight exact `--wrap` foreign symbols and the 21 test commands. No shared
registration, ABI/manifest, Random, BLAS, native QR or other module file was
changed by this slice; no owner commit or push was made.

Required incomplete items are unchanged: nonzero GELSD NRHS=0, S/C nonzero
GELSD at/above the proved tree boundary, GELSS Path2a's promised A output,
genuine finite-input nonconvergence evidence when available, repository-owner
and license/provenance approval gates, and integration/install verification.
The remaining P06 orthogonal/generalized/constrained families and full P07
SVD/GSVD/CSD remain required. This eight-driver slice neither replaces nor
reduces them. Do not restart its design or relabel the preserved failures.
