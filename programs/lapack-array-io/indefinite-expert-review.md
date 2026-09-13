# P05 classic indefinite expert slice

Status: the bounded 24 source routes are implemented and have frozen standalone
LP64 and true ILP64 verification. They are not registered or credited in the
central coverage ledger by this isolated work. Integrated and installed
component gates remain the integration owner's next task. This is not complete
P05 or full-provider capability: the denominator remains 2113 required rows
(3551 total source records), including all other required indefinite variants.

The exact scope is S/D/C/Z SYSV, SYCON, SYRFS, SYSVX and C/Z HESV, HECON,
HERFS, HESVX. Source-derived identities, classifications, declarations and
per-source hashes for these 24 rows are retained in the external
`p05-indefinite-expert-02/upstream-rows.json`, SHA-256
`3f6e978dfbb5b8c5619342d703061e762d03e33ee761cd5386c9e8d3561b88ae`.
Source or archive symbol presence is not capability evidence.

## Isolation and immutable identities

All external locators below are relative to
`../asc-cpp-evidence/lapack-array-io`, outside the source tree.
The agent-owned sibling worktree `../asc-cpp-p05-indefinite-expert` is on
`feature/lapack-p05-indefinite-expert`, based on integration commit
`31e93935f8db4ac685c2224abf7860a058040117`. This slice changes no shared
registration, ABI, coverage, schema, commits or remote state.

The nineteen new code/header/test files are frozen in
`p05-indefinite-expert-02/implementation-files.json`, SHA-256
`2df894c28a79ca93415375d3225e58e2ef55017f581032cf39b179145e548b78`.
The tested source archive `p05-indefinite-expert-02/source.tar` has SHA-256
`9e0b87ec98d798159a45c9faede2e030c7981695c44415e203381dd4db173940`.
This final review is the twentieth new file and deliberately postdates that
archive; its earlier in-archive review records the pre-verification state.

Fourteen frozen classic-factorization dependency files were copied byte-exact.
Their manifest `p05-indefinite-05/implementation-files.json` has SHA-256
`5edb2c17ec6935511d136737dfab0ace235adf70c4922b339ca3a5f525d8542c`.
They remain immutable inputs, not newly authored files. The only strict-check
overlay discussed below is external and is not present in this worktree or
the tested source archive.

Reference-LAPACK 3.12.1 source commit is
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, unsigned annotated tag
`5ebe92156143a341ab7b14bf76560d30093cfc54`. Source-input manifest is
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`;
inventory SHA-256 is
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`.
Provider identities are LP64
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
and true ILP64
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
These are GNU 11/Linux static development subsets, deprecated enabled and
XBLAS disabled, not full profiles. Read-only attestation validation passed
both widths against the exact source lock and installed archive records.

Actual repository instructions, runbook mission/execution/architecture,
P01/P04/P05/P11 and applicable numerical/evidence cross-cutting sections were
reread. Live [Google C++](https://google.github.io/styleguide/cppguide.html)
and [Python](https://google.github.io/styleguide/pyguide.html) guides were
retrieved 2026-09-07. Accepted repository compatibility rules are unchanged.
The integration owner's bounded design approval is not license approval or
completion of an owner-review gate.

## Implemented public and mutation contracts

New optional headers are `lapack_indefinite_condition.h`,
`lapack_indefinite_refinement.h` and `lapack_indefinite_driver.h`.
All actual scalar cases have typed query/execution overloads, explicit
provider, caller-owned workspace and mandatory failure-surviving report.
There are 60 documented overloads, not 60 upstream rows.

- SYSV/HESV overwrite selected A, output classic raw paired pivots, and
  overwrite B. Actual source TRS versus TRS2 selection follows caller LWORK;
  TRS2's temporary factor encoding is restored.
- SYCON/HECON consume raw selected factors, classic raw pivots and finite
  nonnegative original one-norm. The raw interface preserves upstream's
  singular-factor RCOND=0 behavior.
- SYRFS/HERFS preserve original A, factors, pivots and B; improve caller X
  and output separate real FERR/BERR vectors.
- SYSVX/HESVX expose FACT=N and separately named FACT=F execution/query
  overloads. N outputs AF/pivots; F preserves them. A/B are immutable.
  X/FERR/BERR/RCOND are outputs. These routines have no FACT=E mode.

Raw factor inputs retain the caller's common-origin and lifetime obligations.
A report cannot prove current buffer contents or cryptographically bind their
origin. The copied factor-view factory remains TRF/TF2-only: this slice neither
changes it nor synthesizes an originating TRF report. A coordinated admission
extension for real successful drivers remains a separate integration decision.
Both VX modes report the same exact upstream routine; FACT is bound in the
plan key, not encoded in the report's routine name. A report alone therefore
does not distinguish a newly generated N factor from caller-supplied F data.
INFO=n+1 is an accuracy warning with documented partial outputs, never success.

All matrix layouts are independent and row-major packing is explicit in the
caller plan. Original Hermitian A in FACT=N is selected-component packed even
when column-major: internal LACPY would otherwise read ignored imaginary
diagonals. RFS/FACT=F original column-major Hermitian A may stay direct after
the HEMV/norm/diagnostic source audit. Actual factor coefficients, including
meaningful complex block data and diagonals, are not normalized as original
Hermitian input. No broad symmetry/finiteness scan reads the unused triangle.

Queries inspect metadata only and bind all modes, shapes, layouts, original
and effective strides, scalar and provider ABI. Real-backed 1-by-1 views with
INT64_MAX original leading dimension use the source-valid effective dimension;
pure integer tests, never forged backing, exercise large arithmetic limits.
Every nonempty referenced operand and workspace region must be accessible
through the provider context. Serial execution rejects pinned storage;
unused zero-byte pinned workspace retains existing compatibility.

## Source-derived workspace, integer and failure audit

Actual pinned Fortran implementations and their call chains, rather than
prototypes alone, determine this contract. The installed pinned header has
all 24 declarations; the separate executed ABI probe checks their actual use.

Fixed CON needs 2*n scalar workspace. Fixed RFS needs 3*n real or 2*n complex.
Real estimators need n extra provider-width IWORK entries in addition to n
converted pivots: these are distinct simultaneously live subregions of one
2*n provider-integer region. Complex paths use n provider integers for pivots;
RFS/VX also use n real entries. No ASC index_t/LP64 reinterpretation is used.

The exact pinned ILAENV branches supply NB=64. SYSV performs a TRF query and
integer conversion; S/C apply SROUNDUP again. HESV computes its own n*NB
query. VX minimum is 3*n real or 2*n complex, and preferred storage includes
64*n only in FACT=N. Source LACN2's 3*n arithmetic is guarded even for complex
paths with only 2*n scalar work. NRHS terminal loop increments, native pivot
bounds and BLAS vector cursor expressions use checked provider-width counts.
VX's initial TRS uses actual X leading dimension; refinement's compact
single-RHS internal solve does not invent a full-NRHS B-stride constraint.

S/C query planning guards both REAL conversion and optional upward epsilon
multiplication below the provider's power-of-two upper-exclusive integer
bound. D/Z preserve the maximum of raw integer counts and checked rounded
query values, avoiding understated capacity after downward rounding. Pure
tests use independent LP64/ILP64 constants at these exact boundaries.

For n>0 and NRHS=0, SV still factors and VX still factors/estimates. Effective
foreign LDB/LDX is max(1,n), with valid local dummy RHS objects, rather than
the generic empty-view dimension one. RFS empty-order/zero-NRHS and CON
empty-order/zero-norm paths do not call the provider.

Signed paired pivots are validated before absolute-value or index operations.
Raw F/refinement solves reject exact zero evaluated 1-by-1/2-by-2 divisors
before output mutation; CON retains its source-specific singular early
RCOND=0 result. Positive FACT=N factorization INFO publishes only documented
partial AF/pivots and RCOND=0, preserving X/FERR/BERR. Invalid returned INFO,
pivots, work-query values or diagnostics cannot certify factors. Selected
NaN pivot failures retain raw INFO and are not mislabeled as exact singularity.

Complex SY is transpose-symmetric, whereas HE is Hermitian. Refinement
diagnostics use abs(real)+abs(imag); original Hermitian diagonal contributions
use their real component. The condition estimator's norm and mathematical
residual oracles remain distinct. In particular, FERR is a source estimate,
not a universal verified error bound, and no universal finite-output promise
is inferred.

## Frozen standalone verification

All artifacts in this section are under `p05-indefinite-expert-02`.
The six configurations each ran 32 CTest cases, with no failures or skips:
LP64 and ILP64 Debug; LP64 and ILP64 ASan+UBSan; LP64 and ILP64 Release with
dynamic libc interposition. Thus 192 actual CTest cases passed. Matching
`ctest-<abi>-<debug|asan|libc>.log` and JUnit XML preserve raw output.

Each mathematical group runs separately for S, D, complex-symmetric C/Z and
Hermitian C/Z; its counts below are per scalar/symmetry group and per build.

| Group | Executed cases and independent checks |
| --- | --- |
| CON | 32 principal diagonal/block cases, both triangles/layouts, empty/scalar and singular blocks, extreme scales, analytic RCOND; additional workspace/pivot/quick-return/wide-stride checks. |
| SV | 424 principal cases and four selected-NaN failures; both triangles and independent A/B layouts, n=0/1/2/7/67, NRHS=0/1/3, work=1/n/64n, extreme scales, singular-last INFO, independent reverse Schur factor reconstruction and residuals. |
| RFS | 448 principal cases, all 16 independent A/AF/B/X layouts, perturbed-solution improvement, independent wide residual and source-safe-term BERR, FERR plausibility, immutable input/red-zone checks; separate alias/workspace/pivot/divisor/placement failures. |
| VX | 1792 principal cases, 512 exact failure/warning cases and four selected-NaN failures; N/F and minimum/preferred work, all 16 layouts, independent long-double inverse-norm estimator checks, residual/error diagnostics and precise output publication. |
| Faults | 72 injected source-return faults and 20 structural cases, with typed wrappers for all 24 outer routes and actual call counters; malformed INFO/pivots/work/diagnostics, stale or short workspace and active-operand/report aliases. |
| ABI | One dedicated test performs 96 real direct foreign calls covering all 24 routines, both triangles, N/F and query/execute, CHARACTER lengths, INFO, native pivots and IWORK/scalar/real sentinels. |
| Counts | One pure checked-arithmetic test covers exact LP64/ILP64 cursor and query-rounding boundaries without invalid backing spans. |

Debug observes global C++ new and ELF static allocation calls. Release libc
runs additionally interpose malloc/calloc/realloc/aligned_alloc/posix_memalign
through shared-runtime calls; positive controls observe both direct libc and
shared libstdc++ allocations before the audited operations. First numerical
operations are included rather than deliberately warmed away. The direct
ABI-only probe is not an allocation test. ASan/UBSan instrument new and frozen
adapter C++ plus tests, not the previously built provider/Core/Dense archives.

The source-conditioned allocation closure covers all 24 roots and 149 archive
members per ABI. External leaves are cabs, cabsf, memcmp, memcpy and memset;
there are no unknown imports. XERBLA's unreachable validated error branch and
the actual ILAENV ISPEC branches are recorded explicitly, not silently erased.
The graph and dynamic observation together apply to these exact artifacts
and exercised modes, not arbitrary platforms or future provider builds.
Closure JSON hashes are LP64
`9e3ab976ba9ca16f0f9e332f1b6e44b5dd6aa7147eb19888b216c265bcbcb389`
and ILP64
`d191884573a1b83b3ded03cf55c5d81a66e543de88a5bddb333739c6e83ae82e`.

The 19 owned code files pass clang-format 18.1.8. All three public headers
compile independently as C++20, no exceptions, without vendor includes.
Strict Doxygen 1.9.8 emits zero warning bytes and documents all 60 unique
public overloads in namespace XML. The actual repository Markdown checker
passes. All 12 owned translation units pass LP64 clang-tidy, including a
second four-production-TU pass covering private headers; ILP64 direct-ABI
and typed-fault translation units also pass.

### Preserved inherited strict failure and separate overlay

Original ILP64 production strict checks fail on one unchanged copied
dependency: the GNU Fortran-emitted primitive assertion
`std::is_same_v<lapack_int, long>` triggers google-runtime-int.
No owned production finding remains. Both original failed logs are preserved.

The integration owner approved a separate external comment-only overlay,
preserving the primitive assertion and explaining a line-level exemption.
Its input is root's frozen V7-03 header, SHA-256
`d9827334031238405eda48fcf399e8e5f8202f18ea55ed950156471859781395`,
from tree `a7aa1b35798b6e7062be22990895360232fb4ef4`, archive SHA-256
`07d4991e7fae66b51696e769f64cc7676c5dbed4e05ed49f945b9869d80ee052`.
The separate `strict-overlay.tar` SHA-256 is
`406448c1cdb461c268ba9047835d3b415752a022b91c43bde106c31c18021552`.
Only those two comment lines differ from the tested source. All four ILP64
production TUs then pass full private-header strict checks in
`tidy-production-ilp64-overlay.log`. This is strict-only overlay evidence:
it does not rewrite the original archive, dependency manifest or failed check,
and the agent-owned candidate does not contain the exemption.

## Retained findings and integration boundary

Earlier working-tree diagnostics remain under `p05-indefinite-expert-01`.
Initial compiler/style defects were corrected rather than suppressed.
A failed RFS oracle had conflated ordinary normalized residual with the
source's guarded BERR: an exactly solved zero row can have BERR=1 because
SAFE1 appears in both numerator and denominator. Independent residual and
source-safe-term diagnostic oracles now check their different definitions.
No numerical adapter change concealed that finding. A fault test initially
aliased CON scratch with an unused sample A; it now aliases the actual AF
operand and proves the intended structural noncall. The original failure
logs remain available. Test-only linker identifiers have narrowly explained
reserved-name exemptions; new production code adds no policy waiver.

Next task is the integration owner's coordinated import of the 19 frozen code
files and this final review, followed by optional-target/test registration,
ABI/header inventories, normalized per-row coverage/evidence contracts,
strict checks with the separately reviewed dependency correction, and full
provider-free/provider-enabled/installed component regression on the exact
integrated identity. The external harness records complete compile/link and
24 fault-wrap symbol requirements. No commit, push or PR was made here.
Owner-review and license gates remain exactly as recorded by the program;
nothing in this slice claims those approvals or shrinks remaining P05–P11.
