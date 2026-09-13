# Bounded original LU output-pivot correction

The adapter's three output-native-pivot buffers started with zero. On the
actual little-endian true ILP64 route, a low32 positive pivot write leaves zero
in the high32 half and passes the existing [i+1, rows] validation. The old-code
controls executed the complete pinned native numerical call with a separate
full-width pivot destination, retained the original INFO pointer and all
numerical arguments, and modified only the outer returned pivot publication.
Each S/D/C/Z process reproduced 144 false INFO=0 successes and 144 accepted
singular partial results. Omit-all and omit-last controls already passed in
both ABIs. Old LP64 therefore passed 4/4; old ILP64 failed 4/4, with every
failure retained. This does not claim a defective pinned provider or an
undetected LP64 omission.

The production patch initializes only those three output buffers with the
minimum full native signed integer. A short low32 write now leaves a negative
full-width value. Existing bounds checks reject it without signed negation,
return kProvider with kPartialResult/kUnusable, retain native INFO (including
actual zero and positive singular diagnostics), publish no caller pivots, and
perform no row-major unpack. Column-major arrays have already been written by
the valid native call and remain marked unusable. Input-pivot conversion in
GETRS/GETRI is unchanged. Allocation, array lifetime, exact scratch size,
preflight order, empty quick returns, plan identities and public headers are
unchanged. No provider/error-handler/registration changes are included.

## Exact prerequisites and ownership

The read-only root recovery is c8ae2f2fbf5578e2dd8bb8d47334c1c8f08bafb4,
feature/lapack-array-io, with its actual dirty status and diff preserved.
`recovered-source.json` identifies the exact already-corrected INFO bytes:
GETRF/GETRS02 reference_lu.cc and expert INFO03 reference_lu_expert.cc. The
expert prerequisite is explicitly overlaid into this external snapshot;
root/shared files and the previous INFO evidence directories are untouched.
The correction's diff is relative to these dependencies, not the older root
expert TU. Exactly two production TUs and four additive test/support files
are owned. All original tests and both prior INFO suites are preserved.

## Pinned-source review

`pinned-pivot-sources.json` binds all 16 actual S/D/C/Z native sources to
Reference-LAPACK 3.12.1 commit 6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca;
the source checkout is unmodified. In all four GETRF sources, lines 157–170
return for empty shapes or delegate to GETRF2. Lines 181–190 factor each
panel and adjust every pivot by J-1 before LASWP. ILAENV lines 279–287 returns
NB=64 for both real and complex GE/TRF; the 67x65, 65x67 and 67x67 fixtures
therefore exercise the source-derived blocked branch. No block-size override
or upstream patch is used.

S/D GETRF2 lines 174/190 write the one-row/one-column pivots; lines 226,
249–257 cover both recursive subproblems and add N1 to the second pivot block.
C/Z equivalents are lines 176/192, 228, 251–259. The second recursive block
still runs after a positive singular INFO. GETF2 S/D writes JP into each
IPIV(J) at lines 172–173; C at 167–168 and Z at 173–174. The loop continues on singularity.
Real code uses its safe-minimum scaling branch, complex GETF2 uses CRSCL/ZRSCL;
those numerical differences do not alter the full native integer pivot
assignment. Every scalar's GESV lines 166–172 delegates to GETRF and invokes
GETRS only on INFO=0; NRHS=0 still factors. All output pivots are native
INTEGER before ASC range checking and value widening; there is no raw-array
reinterpretation as ASC index_t.

The wrapper nesting guard prevents the fault from affecting recursive LU,
blocked panel results, or the GESV solve. Its signatures have 32 static type
checks against actual pinned LAPACK declarations (real and wrapper for each
of 16 routines). The real routine receives all original arguments except the
outer pivot destination, including the exact original INFO destination.
Observed actual pivot count and [i+1,rows] validity are checked for every
entry; all source-required entries, including the last rectangular pivot,
are published by the positive control. Diagnostic storage is bounded and
allocation-free. Short-write tests intentionally describe little-endian
ELF behavior and make no other-platform claim.

## Executed fixtures and evidence scope

Each scalar executes 612 LP64 or 1,020 true ILP64 profiles: GETRF, GETRF2,
GETF2 on scalar/square/tall/wide/empty shapes and padded row/column layouts;
GESV n=0/1/4/67, all independent A/B layouts, NRHS=0/1/2, nonsingular and
singular results. Full, omitted-all, omitted-last, and ILP64 short-all and
short-last destination writes are distinct. Every real call retains genuine
native INFO=0 or the expected singular INFO. Positive controls independently
check P*A=L*U, known solutions, pivot canaries, matrix padding and workspace
guards. GESV factors are reused in 144 N/T/C GETRS profiles per scalar, with
factor/pivot immutability and known-solution checks. C and C++ allocation
probes remain active. Every foreign-calling main has the exact common
NormalReturnGuard as its first local; completion markers plus unchanged
normal-return guard tests prevent false success from Fortran termination.

`audit_profiles.py` independently enumerates the complete expected profile
sets, uniqueness, actual call counts, native pivot validity/last value,
retained INFO, incoming/output full-width values, expected statuses and
reusable solves. It explicitly expects only the reproduced width/report/
publication failures on old ILP64. Every other numerical/storage assertion
must pass; old LP64 and corrected profiles must have no failure.

All lanes rebuild both affected production TUs and the tests. Release and
scoped Debug borrow hashed V11 Release Core/Dense libraries; this is not a
full Debug dependency build. ASan/UBSan borrows V11's fully instrumented ASC
C++ archive; pinned LAPACK/BLAS and Fortran runtime remain uninstrumented.
`borrowed-source-closure.json` confirms that the borrowed Core/Dense source
closure differs only in the two rebuilt TUs. Unlinked V12 additions are not
verified by this bounded harness. Dependencies are hashed before and after.
No full-program, installed/shared, other-platform, BLAS/Random, optional
XBLAS or remaining mathematical acceptance claim follows from this slice.
Earlier SVD/source-invariant and full-program required math failures remain
open under their preserved identities.

## Final executed results

Final02 re-executed old LP64 4/4 pass and old ILP64 4/4 fail, preserving the
1,152 accepted short-write cases across S/D/C/Z: 576 false successes with
actual INFO=0 and 576 accepted singular partial outputs. All normal and
omission controls passed; every failure belongs to the precise expected
width/report/publication assertions. No mathematical or guard assertion is
relaxed or removed. The independent mode audit passed all eight runs.

Corrected Release, scoped Debug and ASan/UBSan each pass 26/26 per ABI (six
lanes), with zero skips. These include all original reference LU, layout,
expert, counts and normal-return guard tests, both unchanged INFO suites,
and four new pivot scalar processes. Per scalar, each LP64 run records
612 profiles/432 genuine factor-or-driver calls, split 216 INFO=0 and 216
singular; each ILP64 run records 1,020 profiles/720 genuine calls, split
360/360. Each scalar also completes 144 reusable GETRS profiles. Both layouts,
independent GESV A/B layouts, all NRHS options, empty calls, final rectangular
pivots, source-derived blocked sizes and raw partial singular outputs remain
explicit.

Whole-file format passes all six correction files. All eight Clang18 strict
checks pass (two production and two test TUs per ABI, with the repository
configuration and unchanged warnings-as-errors). The final audit passes:
33 configure/build/test/strict/format records, plus the independent profile
and identity audit records; exactly one source identity per old/fixed variant;
48 external dependency hashes unchanged; 606 compiler dependency files and
226 built artifacts bound. All recorded source-before and source-after hashes
match the frozen source. No owner process remains running.

Attempt01 remains frozen with its exact old/fixed Release evidence and the
wrapper strict failure: the observation helper's INFO pointer could be const.
Revision02 changes only that internal parameter. Every mode/assertion and
both production TUs are byte-identical; the native routine still receives
the original mutable INFO pointer through the wrapper lambda. Fresh02
results above are not inferred from01. `test-revision-delta.json` checks this
single-line test-only delta.

The parent independently committed both prerequisites while this snapshot was
running. `root-dependencies-committed.json` verifies actual commit bytes:
GETRF/GETRS bb4ce940a677fe95b0db0bd696565a6691286110 and expert
38e085650505e7f0f8ee9740911970a1a1bc24e6. Recorded original recovery remains
unchanged. `integration-registration.cmake` is an unapplied four-test snippet;
root review/integration and a fresh composed source run remain separate.

Root independently read all six final correction files and the three production
deltas, existing preflight/publication paths, frozen LU contract and exact
profile parser. The independent audit
`p04-lu-output-pivot-root-review-01/audit.json`
(SHA-256 `c6307258f923d5d9b5c9ad5315d66ad4e28136f930eef7147c5b748eef5878f7`)
checks 35 executed records, all eight exact profile sets, 48 external dependencies,
606 compiler-read inputs, 226 build artifacts, both full source snapshots,
and actual committed INFO prerequisites. Root imported the exact six files and
registered four additive tests. Current composed integration is pending;
previous full-project results do not verify these changed production bytes.

Current root integration completed on frozen tree
`3bdf77d1415c91d5ef0195fd7ba3982f718fa361`, archive SHA-256
`2e5d79606358192f05ef6985bc4f256df7ca88a98708bc4ab078f49b5bb1df28`.
All six LP64/true-ILP64 Debug, Release and ASC-only ASan/UBSan lanes pass
26/26 tests, zero skips. Each lane rebuilt all 53 CPU Core/Dense/provider
production translation units; the pinned foreign archives remain uninstrumented.
The exact 612/1020 pivot profiles and 144 reusable solves per scalar pass,
and both original INFO mode matrices remain passing. Format, coverage and
public-surface checks pass. Eight historical strict checks apply through
byte-identical actual compiler-read sources, headers and generated ABI config;
no fresh root strict or full-project run is claimed.

The completed audit `p04-lu-output-pivot-integration-01/completion-audit-01/audit.json`
(SHA-256 `dd759232887f9ed737e9177cf837b4570a8b97b12eecd6087ac6b91ed63cdd6e`)
binds all 21 executed records, 925 frozen source files, actual compile inputs,
object hashes, flags, raw logs and all three diagnostic profile matrices.
Native acceptance tests, GB20 additions and all normalized mathematical/platform
requirements remain independent work.
