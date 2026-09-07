# P05 general-band LU bounded review

This is the isolated, uncommitted S/D/C/Z GBTRF/GBTRS owner slice on
`feature/lapack-p05-general-band`, based on
`455c235f8a0c20e4994a035a6b64b81e9950a453`. The branch/path were confirmed
absent before the authorized worktree was created. Existing general-band
implementations/evidence were searched and none found. No shared contract,
registration, inventory, ledger or existing worktree was edited. This report
is a bounded handoff record; it does not award full-profile capability credit.

## Interface and source contract

The new `lapack_lu_band.h` optional-provider API implements exactly `sgbtrf`,
`dgbtrf`, `cgbtrf`, `zgbtrf`, `sgbtrs`, `dgbtrs`, `cgbtrs`, `zgbtrs`. Other
GB drivers, condition/refinement/equilibration and tridiagonal families remain
required and open. The existing column-major-only `LapackLuBandView` provides
`2*kl+ku+1` factor rows and the original diagonal at zero-based `kl+ku`.
Only explicit row-major RHS conversion occurs; band inputs are never densified.
GBTRS supports N/T/C and either independent RHS layout. Every query is a
metadata-only checked formula and neither source routine has a WORK query.
Caller integer storage uses the selected native signed width/alignment;
row-major nonempty RHS additionally needs `n*nrhs` live scalar objects.

`ReferenceLuBandFactorView<T>` is a distinct nominal family. Reports deliberately
leave `factor_family` absent; the common GETRF tag/factory is not reinterpreted.
The factory requires complete same-scalar/same-provider GBTRF INFO=0 provenance,
actual provider execution and no contradictory diagnostic fields. It checks all
`min(m,n)` raw swaps, including the last: `j+1 <= p[j] <= min(m,j+kl+1)`.
These signed one-based values are sequential band swaps, not a permutation.
Rectangular factors can be inspected; GBTRS requires square factors. The caller
must ensure the buffers and report share one origin and preserve their lifetimes
and contents; metadata cannot authenticate that obligation.

GBTRF positive INFO retains completed raw factors and all validated pivots as
`kSingular/kDocumentedPartial`, with first exact zero at INFO-1. Such reports
cannot certify a reusable successful factor. GBTRS checks every raw swap and
exact zero U divisor before mutation and never invents native INFO. Negative,
impossible, unwritten or corrupted native outputs produce `kProvider/kUnusable`.
Native INFO and pivots start with full-width nonzero sentinels; invalid output
pivots and failed packed RHS are not published. Directly supplied column-major
numerical storage cannot be rolled back after a foreign defect. Success retains
upstream nonfinite/conditioning limitations; no finiteness scan or tolerance is
added.

The exact upstream is Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. GBTRF calls ILAENV with N4=KU:
NB=1 for KU<=64, otherwise 32; GBTRF also falls back to GBTF2 if NB>KL.
Blocked coverage therefore requires KU>64 and KL>=32. The provider, not ASC,
selects that branch. Source-selected asymmetric blocked fixtures include
(129,129,40,67), (131,117,32,66), (117,131,40,67) and degenerate small matrices
with large declared bandwidths. Fixed WORK13/WORK31 arrays are each 65-by-64
native scalars; inspected archive closure contains no writable static symbols.

`internal_lu_band_limits.h` is a pure arithmetic checker, permitting exact
LP64/ILP64 boundary tests without fabricated huge backing spans. It checks
`2*KL+KU+1`, KU+2, the actual GBTRF loop terminal, J+KV, the blocked expressions
which add pivot row plus KV before subtracting indices, and the reference
SWAP/GER final cursor `1+length*(LDAB-1)`. GBTRS checks the upper TBSV and RHS
loop terminals and `1+NRHS*LDB` for lower-factor SWAP/GER/GEMV/LACGV routes.
A single RHS column uses its equivalent N stride, retaining the original ASC
stride in plan identity. Empty GBTRS still supplies LDB=max(1,N), because that
argument is checked before the NRHS=0 source quick return.

## Executed bounded acceptance

External evidence directory:
`/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p05-general-band-01-7tvlpl1r`.

The final frozen source is `source.tar` (SHA256
`bc5b40d9c31a0d08fa76c47f9e1435f9d28b38fbbd0b59da83697e6b0984876b`),
with the original eleven code hashes in `owned-code.json` and all 841 files
rechecked against `source-manifest.json` (SHA256
`714a0a06776b9155e9b888bc330051495ead0d807377cad79fd45e18ec9685d4`).
The independently added boundary test is frozen separately in
`source-addendum-v2/tests/dense_lapack/lu_band_boundary_test.cc` (SHA256
`c4bed65a75a739d444e5afe82db320daf89e4077f44b86d053a2600523a47f1e`).
Its new source does not alter the earlier source archive or original eleven
files. This final report supersedes the preliminary report inside that archive.

All six final configurations passed 11/11 original tests each: actual LP64 and
ILP64, each with GNU 11.4 Debug, Clang 19 ASan/UBSan, and libc allocation
interception. Each configuration executed 84 checked nonsingular factor cases,
936 solves and sixteen genuine singular fixtures. The separate additive
boundary executable passed all four scalar tests in all six configurations.
Thus the bounded evidence comprises 90 executed CTest passes and no skips.
`executed-tests.json` and `executed-addendum-v2-tests.json` record the actual
JUnit and LastTest hashes; `commands-*-final.json` and
`commands-*-addendum3.json` retain configure/build/test commands and exit codes.

The diagnostic harness initially used reusable P04 archives. Its older
`lapack_foundations.cc` and `reference_lu.cc` were identified during evidence
reconciliation. Every final build instead compiles the two unchanged current
source files alongside this slice. Exact relink maps and executed public
consumers confirm that the reused Core members are only `contracts.cc.o`,
`execution.cc.o`, and `status.cc.o`, whose sources match the current frozen
source. No object from the older Dense archive is pulled. The maps also expose
uncalled GETRF dependencies pulled by the provider-construction translation
unit; those are not counted as executed GB coverage. Earlier archive-based
runs remain diagnostic only. `dependency-identity-final.json` records these
checks and the actual archive, generated configuration, compiler and runtime
hashes. No installed-package evidence is claimed.

ASan/UBSan instruments the current ASC GB/foundations/provider-construction
translation units and tests; the foreign archives and reused Core members are
not instrumented. No whole-provider sanitizer claim is made. The GNU Debug
checks intercept global C++ allocation and static archive libc calls. The libc
variant additionally uses the preserved direct-libc/shared-libstdc++ probe,
with positive controls observed in all eight original numerical/fault scalar
processes and four additive scalar processes per ABI. Scoped operations
reported no allocations. Actual provider flags are `-O2 -frecursive`, with
`-fdefault-integer-8` only for ILP64; copied provider target flags, rather than
unused CMake Release defaults, identify those builds.

Independent tests reconstruct the full original rectangular matrix by reversing
each band elimination and its interleaved swap, then check normalized infinity
norm reconstruction error. N/T/C systems use separately formed known solutions,
normalized residuals and forward error, both RHS layouts and 0/1/3 RHS columns.
Fixtures cover empty/scalar, diagonal/one-sided/asymmetric bands, noncommuting
swaps, far pivots entering WORK31, tall/wide blocked factors, and exponents
+/-100 (single) and +/-800 (double). Source-defined fill slots start as NaNs;
ld padding, factor/pivot/RHS storage and exact workspace red zones are checked.
Four real singular fixtures per scalar include first/middle/last pivots and
failed-factor rejection. These mathematical checks are separate from ABI
fidelity and return-fault injections.

Actual pinned-header calls probe all eight routines with full-width guarded
native parameters, INFO and pivots, complex components and hidden CHARACTER
length. Sixteen untouched compiler prototype emissions (all eight, each ABI)
are also compared after removing only C input-const annotations absent from
Fortran declarations. Fault tests verify negative/minimum/impossible/unwritten
INFO, zero/negative/out-of-band/last/unwritten-width pivots, column mutation vs
packed nonpublication, every unsupported factory report class, stale plans,
wrong shape, placement, alignment, aliasing, insufficient workspace, exact-zero
solve preflight and no native calls on structural rejection.

The additional boundary test makes alias rejection independent of other
preflight failures. Exact-capacity aligned integer workspaces overlap real
factor or output-pivot elements. Aliased factor/RHS solves use the exact same
metadata as a valid independently queried plan for every N/T/C mode. All
rejections preserve numerical buffers and avoid the native call. A one-pivot
ILP64 return fault writes only the low four bytes of otherwise valid pivot 1;
no second unwritten pivot can mask failure to reject that partial-width write.
LP64 separately exercises an entirely unwritten pivot. All cases preserve
caller pivot output and workspace red zones.

Exact source/archival admitted-path closure records contain 59 members per ABI, source hashes
matched to the pinned commit, and only memcmp/memcpy/memset external imports
on the admitted path. ILAENV's unrelated ISPEC branches and XERBLA's unreachable
ordinary-validation diagnostic branch are explicitly recorded as conditioned
edges; no required public routine is removed from the inventory. No writable
static symbols are present in the inspected closure.

Live Google C++ and Python guides were retrieved on 2026-09-07 UTC with hashes
in `style-source-ledger.json`. Repository C++20 concepts/API conventions,
borrowed typed views, explicit template instantiations and narrowly scoped
invalid-enum diagnostic tests retain existing repository exceptions. Full
repository Clang18 tidy rules are enforced; placement-new's required header
uses the existing IWYU keep convention. Python audit helpers and generated
logs remain outside the source tree.

The original seven owned translation units pass the complete repository
Clang18 tidy configuration for both ABIs. Header checks pass all sixteen
combinations (four public/private/test headers, GNU11/Clang19, ordinary and
no-exceptions). Strict Doxygen 1.9.8 passes with the full existing repository
configuration, and its XML contains all 21 public function declarations with
complete parameter documentation. Original-file formatting also passes.
The additional boundary translation unit is checked separately with the same
explicit repository configuration because its external overlay has no parent
`.clang-tidy` or `.clang-format` file. Exact final commands and logs are in the
corresponding `tidy-*`, `header-commands.json`, `doxygen-final*`, and
`format-*strict*` records.

Failed diagnostics remain preserved. Actual tests found and fixed an empty
GBTRS call supplying LDB=1 when N>1/NRHS=0; the current implementation supplies
max(1,N). A claimed noncommuting fixture was corrected after its independently
computed second pivot disproved the intended fixture property; the strict
noncommuting assertion was retained. Formatting/tidy corrections preserved
all prior assertions. The preliminary Doxygen harness lacked the full strict
repository preprocessing/prototype configuration; the successful final run
uses that complete configuration. The additive harness initially used PUBLIC
properties on an imported target and was corrected to INTERFACE. The first
additive alias fixture pointed at the leading complex-double guard, which did
not overlap LP64's shorter integer scratch range; its preserved failing test
led to using the actual factor-storage address. The successful v2 additive
source and `addendum3` build names distinguish those corrected results. An
external-overlay format invocation without the repository configuration is
also retained and superseded by the explicit strict configuration check.
No failing assertion or required source mode was disabled.

## Root integration requirements

Register the one new optional public header and `reference_lu_band.cc`, plus the
new standalone numerical/fault/boundary tests, pure limits test, exact-ABI
probe and public-only installed consumer
candidate. The consumer uses only public ASC APIs and the existing unchanged
`installed_lu/factorization_support.h`; direct-link execution is not installed
package evidence. Add exact eight-row coverage records and explicit six solve
modes per scalar, source identities, dependencies and final test evidence only
after root integration and installed runs. Revalidate current source hashes,
provider identities and all required profile checks. No source/contract changes
are requested outside those normal root-owned registrations.

The owner handoff contains twelve code/test files plus this review, all new
and uncommitted; `handoff-files.json` binds those exact paths and hashes.
Root must review and import them, add source/header/test/install registrations,
and rerun integration and installed-consumer gates on the resulting source
identity before assigning verified coverage. The next dependency-satisfied
owner action is that bounded root integration, not a new architecture phase.
Missing XBLAS/full-profile dependencies and all other required P05 rows remain
visible in the shared program state, which this owner did not edit.


## Root V10 integration checkpoint

Root read all12 C++ files and this final owner review, verified the13-file
handoff and every artifact in acceptance-artifact-manifest.json, and checked
all42 compiler-recorded shared inputs. Only the existing indefinite public
header differs from the owner build: its V9 change clarifies metadata-alias
report preservation and changes no declaration or production code. Exact
root audit is `p05-general-band-root-review-01/audit.json`, SHA256
`1459a08dd7fa8a3bc30892a6bd1419ac845afb74e62aa881298b94d83533de26`.
Six owner11-test final lanes plus six4-test addendum lanes execute90 passing
tests, with original limitations retained above. That evidence does not claim
full ASC sanitizer coverage or the normal-return protection added by root.

Root imports the exact13 payload files into previously absent destinations,
adds the existing normal-return guard to five foreign-calling test/consumer
mains, and atomically registers the public header, source allowlists, two
header probes,14 numerical/ABI/limit/fault tests, installed consumer and all
eight required reference rows. Profile incremental-lapack-v10 has206 partial
reference routes,1907 not started,20 separately implemented_unverified native
operations and zero fully verified reference/native operations. The denominator
remains2113. Fourteen installed consumers are now registered.

V10 candidate01 tree `f53158b07706fadf34eb1b28a1e48be1a0688a38`, archive
`e60540f0f00b77bb93fd3f6ad99473d014c028ada95554b9eb780c449c79a8dd`,
mapping `87a733e7df7789d15d7a890770d5ea85cadf1fed8132fa0a2051c6655f1d8e82`.
Fresh full actual LP64/ILP64 and provider-free Debug/Release/shared builds are
running. Both new sanitizer lanes rebuild every CPU Core/Dense/provider C++
source and select14 GB tests, one public consumer and the actual pinned STOP
control; foreign static archives/runtimes remain unsanitized. Eight owned TUs
per ABI undergo current strict analysis. Documentation, coverage and formatting
have separate current-source records. No completed V10 runtime claim yet.

V9 implementation was committed locally as
`a0a1e156041015636534627098e1e025e90e2ac3`; original release checkout,
preserved owner worktrees and remote refs remain untouched. An initial commit
preparation guessed100 changed paths and failed its count assertion before
staging; the corrected preparation verified actual102 paths against the exact
frozen05 product and durable program files, then committed successfully.


## Completed V10 composed checkpoint

Candidate01 full LP64 and true ILP64 each execute509 tests:505 pass and four
header-manifest checks fail because their independent expected count remained92
when the new public header made the actual count93. Provider-free Debug and
Release each pass273/277, shared275/279, with the exact same four failures.
All five suites record zero skips. No original failed record is reclassified.

Candidate02 changes only that oracle count and its two diagnostics. Its tree is
`21eaea428cf9819e807f01882db542533995e244`, source archive SHA256
`6dc344700ae8c940326ef79f0faa5e9731bb31520632a8190d011c454a7935e2`;
mapping remains `87a733e7df7789d15d7a890770d5ea85cadf1fed8132fa0a2051c6655f1d8e82`.
All five fresh four-test header lanes pass using the original build/copied/
installed/relocated package identities and new external check workspaces.
This is composed evidence; the full suite was not rerun against02.

Both ABI installed packages pass all14 consumers. All CPU ASC C++ sanitizer
lanes pass16/16 each; foreign archives/runtimes remain unsanitized. All eight
owned TUs pass strict analysis per ABI. Formatting12 files, coverage and public
surface checks pass. Doxygen documents93 headers and1848 members, zero warnings.

Harness preparation used CTest show-only, which truncated the top-level full
LastTest.log files. Original full command/JUnit records remain, and complete
nested installed-consumer and sanitizer logs were preserved and hashed.
A fresh verbose four-test numerical replay on each unchanged01 build passes,
retaining full output:21 factor and234 solve records per scalar, per ABI.
No profile count is inferred from truncated original JUnit output.

Exact55 command records and their artifacts, original failed names, corrected
checks, installed results and all835 current product files are reconciled in
`p05-general-band-integration-v10-02/completion-audit-01/audit.json`, SHA256
`04f30966f894ed1e045a0906786e92e4d2a8649780c1f625c6d61397c7e5e5a2`.
The live product equals frozen02 outside durable program records. No normalized
routine/mode, platform, full-provider or owner/provenance gate is awarded by
these command passes. Required counts remain2113,206 reference in_progress,
1907 not_started and20 native implemented_unverified, zero fully verified.

Next: complete the already executed Sylvester INFO-output correction audit,
import its four reviewed files and refresh affected coverage hashes; then
integrate the preserved corrected GT24 source and independent public consumer.
