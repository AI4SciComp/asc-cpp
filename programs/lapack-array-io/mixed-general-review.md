# DSGESV/ZCGESV: checked mixed general solves

The maintained optional Reference facet now executes the actual two pinned
mixed-precision drivers through `Dsgesv`/`Zcgesv` and their metadata-only workspace
queries. This is two upstream rows, four query/execute declarations, and no new
provider or fallback policy. The drivers themselves prescribe lower-precision LU,
working-precision refinement and working-precision fallback. P09 continues into
DSPOSV/ZCPOSV; this slice does not finish the programme.

## Contract and bounded review

`lapack_mixed_general.h` documents independent A/B/X layouts, live typed lower
and working scratch, raw native INTEGER pivot storage, separate caller-owned
statistics and failure-surviving reports. A is unchanged after low-factor success;
working fallback publishes LU. B is immutable. X always stages, never packs old
caller values, and publishes only after valid successful INFO/ITER/pivots. Exact
short-workspace, stale metadata, overlap and alignment rejection precede numeric
mutation. Local N=0 has absent native INFO/ITER; N>0,NRHS=0 enters the provider.

The actual source requires more than the prose N*NRHS workspace when NRHS=0:
DSGESV calls DLANGE('I') with N active WORK entries. Both drivers form
SWORK(1+N*N); the checked plan retains one extra live lower scalar when no RHS
exists. Native N*N+1 and loop terminals are checked with pure integer boundary
tests in both native limits. No fabricated descriptors or uninitialized objects
are used as bounds evidence.

Native INFO and ITER start at signed MIN. Pivots start as native-width sentinel
objects, then validate the one-based interchange convention before widening.
Invalid/unwritten/partial-width diagnostics are provider defects. Caller X and
pivots remain withheld on defects; direct A writes survive, while packed A is
withheld. Singular working fallback publishes its documented A/pivots and leaves
X unchanged. Staged X starts at NaN; underwritten or nonfinite computed X remains
visible with an accuracy warning, rather than reading indeterminate scratch or
clamping a result. Successful working LU supports the existing family-tagged
`LapackLuFactorView`/`Getrs` reuse contract; low scratch factors do not certify A.

ITER=0..30 identifies accepted low-factor refinement, including an initially
adequate solution. Negative values retain the native reason: -1 implementation,
-2 conversion range, -3 low factorization, -31 iteration limit. The pinned
DOITREF constant makes -1 unreachable in ordinary calls; a labeled synthetic
control checks its report translation. Real-provider fixtures exercise every
other branch. ITER is a provider diagnostic, not a forward-error or condition
certificate. No RCOND/FERR/BERR is fabricated. Source stopping comparisons use
working residuals and a norm threshold; overflow/underflow can qualify their
meaning. Complex residual/solution selection uses CABS1 where the source says so.

Cost is O(n³) plus O(n²*NRHS) per refinement, with at most 30 refinements and a
possible second factorization. Explicit storage is O(n²+n*NRHS). The adapter
allocates and transfers nothing. Independent mutable buffers, workspaces, reports,
statistics and permitted provider contexts are required for concurrent calls.

## Executed evidence

Raw records remain outside the repository under
`master-continuation-20260910-01/`. Each run stores actual configured IDs/names,
commands, exit statuses, source snapshots and JUnit; missing executables and
unexpected skips fail the runner. Pinned Reference-LAPACK 3.12.1 remains commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`.

The provider identities are LP64
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
and global ILP64
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
The latter uses the actual BUILD_INDEX64/-fdefault-integer-8 preparation and
liblapack64/libblas64 archives, not just an altered C++ typedef. Compiler, linker,
provider and sanitizer settings come from each existing prepared profile.

| Evidence | Executed result |
| --- | --- |
| Debug LP64 `mixed-debug-lp64-06`; all other Debug/Release/ASC-only sanitizer lanes `mixed-{mode}-{abi}-02` | Eight tests pass per lane, IDs 221–228; no skips or sanitizer findings. Two ordinary, two required mathematical, two fault, one observation and one public example. |
| `mixed-tsan-{abi}-01` | Both real-provider scalar/concurrency tests pass, IDs 221/223. ASC code is instrumented; provider code is not. Existing process-local `setarch x86_64 -R` workaround is retained. |
| `install-mixed-{abi}-01` | Both actual Release installs relocate, build and run the copied public example using only exported targets. Separate provider-free Dense consumer builds/runs without LAPACK, BLAS, libgfortran or libquadmath runtime dependencies. The runner's final copied print label says “PT”; its exact commands and mixed executable/JUnit identify the actual consumer. |
| `mixed-debug-{abi}-headers01` | Both normal and no-exceptions public-header probes pass, IDs 761/762. |
| `mixed-integration-{abi}-01` | Eight actual configured architecture/header/documentation-consistency/package commands pass per ABI with fresh guarded scratch. Full package checks include copied mixed/PT/robust/native-I/O examples and provider controls. |
| `mixed-style-02` and `mixed-final-style-01` | Strict source, all new test/example translation units and formatting pass after bounded first-party corrections. |

The ordinary matrix covers n=0,1,3,9; NRHS=0,1,3; all eight independent A/B/X
layouts; real and nontrivial complex fixtures; initial low success, refinement,
range fallback; a positive working matrix collapsed to singularity in low
precision; working singularity; and an n=8 Hilbert iteration-limit fixture.
Independent known solutions and backward residuals use original rounded inputs.
The Hilbert fixture has only a justified backward-error assertion, without an
ill-conditioned forward-accuracy claim. Scalar A=B tests use denorm_min,
2*denorm_min, minimum normal and maximum finite; exact X=1 passes for both
scalars/layouts, with unchanged 128*epsilon accuracy requirements.

The Linux observation protects real initialized containing X/pivot arrays with
PROT_NONE until validated wrapped native entry restores access. All A/B/X/pivot
numeric arrays stay protected for metadata queries, stale/short rejections and
local N=0. Forked intentional pre-native reads must signal SIGSEGV; ordinary
native output writes then pass. Both scalars, layouts and zero-RHS provider calls
run. There is no unsafe signal-handler resumption, write-only-page assumption or
foreign-code instrumentation claim. Fault state is thread-local; concurrency
uses ordinary provider calls with independent mutable state.

## Corrections and limits

All failed attempts remain retained. `mixed-debug-lp64-01` failed test compilation
because its setup omitted the required execution context. Attempt02 built and
ran but had a faulty “every ordinary system must refine” assertion, and selected
two not-yet-built fault executables. Dyadic smaller fixtures can satisfy the
actual source stopping comparison at ITER=0; the corrected assertion requires
refinement for the n=9 fixture that needs it. All original fixtures, numerical
properties and tolerances remain. Attempt03 failed compilation of an
uninitialized metadata array; explicit empty memory views fixed it. Attempts04
and05 passed. Style01/02 findings and their original source snapshots are kept;
the final tidy correction changes fixture expression spelling, not its values.

The original native20/I/O and robust PPSVX acceptance scopes remain unchanged.
Reference PPSVX's five causes and PTTRS's reciprocal overflow remain genuine
separate failed requirements. No mixed-driver success promotes those rows.
Wider compiler/platform/shared-provider admission and normalized full programme
acceptance remain incomplete. The two mixed rows receive partial registration,
with zero strict complete/verified Reference credit and the 2,113 denominator.

The separate full Doxygen build `mixed-documentation-01` failed on a PT review
link added after the prior PT documentation run. The fix changes that unresolved
local link to the already-published immutable PT review URL. The failed record
remains; the previous zero-warning Doxygen result is not attributed to every
later documentation edit. Fresh hosted PT run34505487098 at fb8c86 confirms
25/29 selected tests pass in each ABI, exactly four PT mathematical failures,
zero skips, and111/111 upstream provider tests per ABI. CodeQL passes; hosted
CI status is recorded separately from these scoped family results.

Fresh `mixed-documentation-02` generation and contract audit both pass.
`mixed-final-record-checks-01` passes programme tool tests, scoped coverage,
regeneration comparisons for the complete backlog and XBLAS closure, Python
formatting and whitespace checks. Hosted PT CI runs34505487053/34505480024
finished with only the strict-documentation job failed; other jobs passed.
Those failures remain recorded and do not certify the subsequent fixed tree.
