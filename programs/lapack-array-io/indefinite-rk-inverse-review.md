# RK indefinite inverse contract review

Status: **implemented; required numerical and native WORK contract gates remain blocked**.
Programme status: **FULL_PROGRAM_INCOMPLETE**.

The twelve S/D/C/Z SYTRI_3/SYTRI_3X and C/Z HETRI_3/HETRI_3X sources are pinned
at `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Full source/hash records, sixteen
dependencies and 24 actual GNU Fortran 11.4 emissions are external under
`continuation-20260912-01/rk-inverse-prerequisite-01`. The six TRI_3 interfaces
occur in pinned `lapack.h`; TRI_3X needs private emitted GNU/Linux declarations.
Only input-pointee const qualification is erased in ABI comparisons. The
CSYTRI_3X documentation definition mistakenly says COMPLEX*16; actual source,
argument contract and compiler emission agree on single complex.

## Reviewed storage and workspace

Mutable square A contains D's diagonal and strict unit U/L. Immutable exact-n
same-scalar E contains upper D(i-1,i) or lower D(i+1,i). Immutable exact-n
kRook pivots have adjacent negative pairs with each entry's own directional
target. The common same-provider/scalar/triangle TF2_RK/TRF_RK origin is a
caller precondition; completed singular factors are admitted. Classic and
interleaved ROOK representations and their existing factories are unchanged.
SY uses transpose symmetry, including complex SY; HE uses conjugate transpose.
Packing preserves complete raw Hermitian diagonal coefficients.

TRI_3X copies every E entry into WORK before checking singularity, including
slots the prose says are not referenced. All n entries must be live/readable.
Ignored slots may contain quiet NaNs and are not numerically validated. A's
other triangle/padding and E/pivots stay unchanged. Only selected A is published.

Active WORK has `(n+nb+1)*(nb+3)` T entries. Every pinned TRI_3 uses nb=1 and
always calls TRI_3X for n>0. S/C return SROUNDUP_LWORK of the product; D/Z
convert it to the real WORK component. The adapter checks integer/rounded
return bounds; driver compute WORK(1) must match the rounded result with zero
imaginary component. Driver plans use the raw minimum and rounded preferred
count. TRI_3X takes explicit nb>0 (also at n=0), accepts nb>n within checked
bounds, and has no LWORK/query/returned-WORK contract. Native nb=0 would fail
to advance the block loop, so ASC rejects it explicitly.

Active calls also need n private provider INTEGER pivots and row-major A needs
n*n live T layout entries. Column-major A and E are direct. Metadata-only
queries perform no foreign call or array reads. Plans bind exact variant,
block size, scalar/provider ABI, triangle/symmetry, shapes, original/effective
strides, E increment and pivot family. Original LDA must fit native INTEGER,
even if unused. Source block dimensions, nested upper TRTRI past-last block,
SWAPR terminal cursors, driver LWORK and all byte products/sums are checked.
N=0 completes without workspace, numerical access or a provider call.

All operands, scratch and live metadata are disjoint and CPU-accessible.
Metadata alias rejection preserves report; other preflight resets it first.
Structural failure preserves numerical buffers/scratch. Operations allocate
nothing, transfer nothing and change no global state. Concurrent calls can
share immutable E/pivots/providers/plans with private A/workspace/report.

## INFO and native blockers

Native argument checks are -1 UPLO, -2 N, -4 LDA and driver -8 LWORK. Full-width
INFO starts at INTEGER minimum after preflight. A source-consistent positive
INFO names an exact full-scalar zero 1-by-1 D entry: upper scan n..1, lower 1..n.
E has already been copied, but A is unchanged because the scan precedes TRTRI.
The adapter reports singular/documented partial with raw INFO and zero-based
index, preserving original selected factors; no inverse exists. No 2-block
singularity or finiteness scan is added. INFO=0 records completion only.
Missing/partial/negative/inconsistent INFO, changed private pivots or wrong
driver WORK(1) are provider defects. Packed A is withheld; direct A may already
have changed. No kernel scaling or Hermitian diagonal normalization is added.

`BLOCK-INDEFINITE-RK-INVERSE-EMPTY-WORK`: in each ABI, all sixty driver queries
and twelve empty execution storage guards pass. SSY/DSY/CHE query and execute
return WORK(1)=1 at n=0. CSY/ZSY/ZHE query returns 8, but normal execution returns
INFO=0 without writing WORK(1), failing six required documented-contract cases
per ABI. Source fidelity passes separately. ASC's empty noncall does not
constitute a native contract fix.

`BLOCK-INDEFINITE-INVERSE-RANGE` applies to this family: each ABI runs 1,080
native cases, 900 with representable exact inverses and 180 nonrepresentable
controls. Forty required mathematical cases fail: all four complex classes,
both triangles and driver/X nb=1/2/3/64, for a 2-block with E=(.75*max)*(1+i).
All input, padding, workspace and full-width INFO guards pass. GNU Fortran
source-expression traces isolate symmetric E/E producing NaN and Hermitian
ABS(E) overflowing before complex inverse-D arithmetic; .25*max controls pass.
Assertions require finite inverse entries, analytic accuracy and both-sided
residuals. Ordinary producer-based controls also retain the exact-Hermitian-diagonal
blocker; independent accuracy/residuals and native byte fidelity pass.

The 360 small guarded mathematical cases per ABI cover all twelve interfaces,
both triangles, n=0/1/2/3, singular 1-blocks and five block variants. Original
failed source-location, C++ snapshot declaration, extraction and record-path
attempts remain preserved alongside corrected runs. No test was weakened.

## Final bounded evidence

Twelve actual static/shared LP64/true-ILP64 Release/Debug/ASC-ASan+UBSan profiles
each execute 213 required processes: 182 pass and 31 gates fail, with zero skips.
Ten ordinary Hermitian gates fail 5,216 exact imaginary-diagonal assertions;
twenty complex range gates fail 1,920 mathematical assertions. The native empty
WORK contract process additionally retains six failed cases. All native
fidelity processes pass separately. Four TSan profiles pass all thirty
concurrency processes each. Canonical totals are 2,676 processes, 2,304 passes,
372 required failures, 85,632 failed mathematical assertions and 72 native
empty-WORK failures. No failure is waived or marked WILL_FAIL.

Ordinary controls run 88 cases per scalar/block variant using both RK producers,
triangles/layouts, n=0/1/2/3/7/67, singular factors, nontrivial permutations and
scaled cases. Independent long-double Gauss-Jordan inversion, both multiplication
orders and exact Hermitian diagonal assertions remain unchanged. Range controls
run 144 cases each: 120 representable inverses and 24 nonrepresentable controls.
Singular factors, other triangle/padding, ignored-E NaN bytes, public pivots,
workspace guards and allocation checks are retained.

Fault tests run 360 cases per scalar/block variant, including sixty empty
noncalls and fifteen INFO/private-pivot/returned-WORK fault modes. The complex
driver scratch WORK seed has two negative components to detect incomplete
writes. Structural tests reject 160 cases per variant and exercise protected
A/E/P queries, empty noncalls, explicit NB0 rejection, original stride bounds,
native block/LWORK and rounded WORK bounds. Concurrent controls use both factor
producers in sixteen groups, four workers and eight repeats, with read-only
protected E/pivots and private A/work/report. Each variant has 768 worker native
calls, 128 empty noncalls and 128 stale-plan rejections, plus serial singular
and producer controls.

Four isolated, relocated static/shared LP64/ILP64 installed consumers pass
3,360 cases each using public headers/package targets and both RK producers.
The installed controls include empty, scalar, paired, singular and scaled
systems. All four installed production libraries match the final Release
producers after the normal CMake RPATH installation transform. Twenty strict
translation units are bound to current source and recursive include hashes;
four standalone header checks pass. Final direct probes pass 360 guarded
mathematical cases per ABI against all 24 original GNU-emitted declarations.
Twenty-four dynamic symbols are added per ABI and none removed.

Package checks pass four manifest contexts and six fixtures; architecture and
dependency checks pass. The actual CI selector contains 1,227 processes and
includes all 213 new runtime and two header processes. Warning-free Doxygen
covers 151 public headers and 2,621 documented members. Twenty exact family
source/build files are frozen in `rk-inverse-final-source.json`; final profiles
are under `rk-inverse-{static,shared}-{release,debug,sanitizer,tsan}-{lp64,ilp64}-final`.
A runtime interruption stopped each shared sanitizer run after 190 completed
tests. Original terminal case results and verbose logs were preserved; only
23 unfinished tests per ABI were resumed. Those two canonical profiles use
explicitly derived composite JUnit/ledgers, with the original whole-CTest exit
status left unknown. Static TSan was later interrupted after 27 terminal passes
per ABI; those results are preserved, with only the three unfinished tests
per ABI resumed. The two shared TSan profiles were previously unstarted. All
four composite ledgers retain unknown original whole-CTest exit statuses. The first resume listing rejected
an unsupported CTest regex before any test ran; its log is preserved.

Final source, command, compiler/provider, strict, ABI and installed evidence is
bound by `rk-inverse-final-audit/audit.json` and the delivery audit. ASC/tests
are instrumented; pinned Fortran/BLAS internals are not. This does not admit
another operating system or provider.

Original failed source-location, native-probe declaration, extraction and
record-path attempts remain preserved. Initial range fixture wiring omitted
E/producer arguments; its failed build precedes the corrected evidence.
Deliberate partial complex-byte writes required explicit memcpy intent and a
trivial-copy assertion. Test-only strict fixes extracted existing checks into
helpers and corrected direct includes, naming and declarations. The independent
header oracle listed 151 headers but its count/diagnostics still said 150;
four original context failures precede the exact count correction. No numerical
assertion, case, guard, tolerance or provider arithmetic was weakened. Original
full engineering LastTest logs were recovered before later CTest runs because
successful JUnit output was capped at 1,024 bytes.

All twelve normalized routes remain `implemented_unverified`, with 240 reviewed
triangle/layout/TF2_RK-TRF_RK-origin/block-size modes. The three named numerical
and native contract gates remain open. Earlier provider/numerical, XBLAS,
CodeQL alert, platform and complete normalized-execution-record blockers remain
intact. No Reference row is verified. Continue SYSV_RK/HESV_RK drivers and their
dependencies. Programme status remains **FULL_PROGRAM_INCOMPLETE**.
