# P05 general-band expert owner review — bounded handoff

Final bounded status: all20 exact S/D/C/Z GBSV/GBCON/GBRFS/GBSVX/GBEQU adapters are implemented. The corrected final source executes74 CTests in each of eight actual-ABI configurations (GNU11 Debug/Release, Clang19 ASan+UBSan, GNU11 libc allocation probe; each LP64/global ILP64):58 pass,16 required mathematical failures, zero skips, exit8. Aggregate592 executions:464 pass,128 fail. All30 strict TU/ABI checks,24 standalone header compiles,27-file formatting and Doxygen67 public declarations pass. The16 upstream mathematical gates remain open, so full family acceptance is not claimed. Root integration/registrations/installed consumers remain open and root-owned.

Scope: S/D/C/Z GBSV, GBCON, GBRFS, GBSVX and GBEQU (20 required exact routines); GBEQUB remains separate and required. This is an uncommitted isolated owner branch, not integrated or installed evidence. Root owns registrations and public installed integration.

Worktree `/home/yicai/AI4SciComp/asc-cpp-p05-general-band-expert`, branch `feature/lapack-p05-general-band-expert`, base `455c235f8a0c20e4994a035a6b64b81e9950a453`. Creation followed recorded negative worktree/source/durable searches and absent target branch/path checks. All 13 GB8 frozen dependency files and the root normal-return guard were copied into absent paths, hash checked, and remain unchanged. Evidence directory: `/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p05-general-band-expert-01-bws3io6q`.

## Executed source/ABI admission

Pinned source `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` remains unmodified. All 20 roots close over 146 admitted members in each exact actual ABI; no writable static symbols. Actual external leaves: cabs, cabsf, memcmp, memcpy, memset. Forty actual compiler-emitted prototypes (20 LP64, 20 global ILP64) completed. Exact header-vs-emitted full-signature compile checks are still open. Source closures, source/object/archive hashes, raw nm logs, prototype commands and outputs are in the evidence directory.

## Implemented, not yet fully accepted

New compact column-major `ReferenceGeneralBandView` preserves every nonnegative bandwidth and rectangular/empty shape, distinguishes compact diagonal KU from expanded factor diagonal KL+KU, and checks complete LD*N backing. New nominal raw signed band pivot descriptor does not reinterpret GETRF tags or certify successful factors. Caller provenance and borrowed lifetime obligations are explicit.

GBEQU4 uses no numerical workspace, metadata-only exact plans, complete context/alias/source-arithmetic admission and full-width INFO sentinel. Actual pinned routines execute even for empty shapes. Source partial zero-row/column results and raw clamped scale diagnostics remain unchanged.

## Executed first diagnostic evidence

GNU11 Debug actual LP64 and global ILP64 production builds succeeded. Initial Ninja configure failed because Ninja is unavailable; fresh Makefiles builds preserve those configure logs. First test compile failed due to an unused inherited helper constant; only new helper code changed, and subsequent compilation succeeded. Frozen `initial-03-source/` plus manifest bind the executed new files.

Each ABI ran all eight CTests: four regular passes and four required extreme failures, zero skips, exit 8. Each regular scalar covers 27 independent rectangular/empty/excess-bandwidth/scale profiles plus exact-zero-row and exact-zero-column partial output cases, full source immutability, caller-output and scratch guards, and C++/libc allocation probes around query/execute.

**Open required mathematical failure:** for singleton A=min_normal/8, all four GBEQU routines return INFO=0, R=1/min_normal, C=8, ROWCND=8, COLCND=1. The independently required ratio min(R)/max(R) is 1. Direct pinned-ABI controls (normal-return guard, no ASC call) reproduce all four failures for each ABI, each process exits 4. See `direct-gbequ-commands.json` and `direct-gbequ-{lp64,ilp64}-run.log`. The failing mathematical gate is retained; no waiver, skip, expected failure, source patch or numerical substitution is applied.

## Exact next work

Implement source-derived GBCON4 on the preserved expanded descriptor and nominal raw pivots, followed by GBRFS4, GBSV4 and all N/E/F GBSVX4 modes. Revalidate the complete family against both actual ABIs, native sentinels, structural/report/alias boundaries, cold allocation, strict style, public headers and Doxygen. GBEQU boundary/fault/strict/sanitizer checks are also incomplete. Required mathematical failures remain open while unaffected work continues. No full family, integration, installed, acceptance or release success is claimed.

## Slice 04 executed addendum

GBEQU4, GBCON4, GBRFS4 and GBSV4 (16 exact routines) are implemented and compiled against both actual ABIs in GNU11 Debug. Each coherent current24-test run passes16 and fails8 required extreme mathematical gates, zero skips, exit8; logs/JUnit/source snapshot and identities use `slice-04-*`. This supersedes the initial next-task paragraph: GBSVX4 N/E/F is next incomplete implementation.

GBCON regular tests use 21 monomial/permutation/excess-bandwidth/scale profiles plus singular factors, both norms and ANORM=0, exact independent inverse norms, full-factor immutability, caller output/scratch guards and no allocation. For singleton A=min_normal/8, both norms produce RCOND=0 instead of independently exact1 for all4scalars/bothABIs. Direct pinned GBCON calls reproduce this without ASC entry (each process exits8); source and logs retained. This adds four required failing CTests per ABI and does not replace the four GBEQU failures.

GBRFS runs 648 profiles per scalar (all N/T/C modes, independently both B/X layouts, n=0/1/3/7, empty RHS, excess/asymmetric bands and power-of-two scales). Known perturbed solutions refine to independently checked residuals/forward solutions; FERR/BERR, noalloc, input preservation and guards pass. GBSV runs126 profiles per scalar plus6 last-pivot singular cases, including blocked70x70 KL32/KU65, independent B layouts and nrhs0/1/3; reverse elimination/swap reconstruction, solve residuals, preserved singular B, noalloc and guards pass. Native INFO and producer pivots use full-width invalid sentinels after preflight.

Still open: GBSVX4 implementation, matching-plan fault/metadata/factory/arithmetic boundaries, exact full prototype comparisons, all strict/header/Doxygen checks and broader configurations for the full current family. No full acceptance, installed or integration success is claimed.

## Slice 06 executed addendum

All20 exact routine adapters are now implemented, including GBSVX4 N/E/F with full-width INFO/output-pivot sentinels, source-defined scaling/provenance, separate compact AB/expanded AFB, independently packed B/X and actual empty calls with the mandatory growth slot. Both actual ABI GNU11 Debug coherent28-test runs pass20 and retain8 required mathematical failures, zero skips, exit8. Raw commands/logs/JUnit and all21 new C++ source/header identities are frozen under `slice-06-*`.

Per scalar, the GBSVX numerical test covers648 shape/scale/transpose/layout/RHS profiles, each through N, E and four F equilibration modes; F repeats with the same factors and plan. This is6480 actual driver calls plus48 forced E equilibration branch calls per scalar, with N/R/C/B outcomes checked explicitly. Factor reconstruction against the scaled matrix, original-system solve residuals/solutions, exact A/B scaling relations, raw-factor/pivot/scale immutability in F, all padding/output/scratch guards and noalloc pass. Zero N and zero RHS calls are executed, not replaced locally.

The exact next work is failure/boundary acceptance: matching-plan INFO/pivot partial-width/no-write faults for all20, metadata aliases and no-mutation stale plans, pure source-cursor boundaries, GBSVX singular/warning/extreme diagnostics, standalone compact-descriptor tests, full emitted-prototype comparisons, strict/public-header/Doxygen checks and all actual-ABI runtime configurations. Initial20 success is not full verified acceptance; the8 mathematical gates remain open. Root integration and installed consumers remain root-owned.

## Slices 07–08 executed addendum

All20 exact function signatures now compile against the40 compiler-emitted prototypes and actual installed headers in both ABIs. Pure source-cursor boundaries and descriptor metadata checks pass, as do91 matching-plan native failures per scalar (all seven routes, full-width INFO/output-pivot sentinels, no-write and isolated partial-width writes with initially-zero/prior-valid caller bytes), plus seven stale-plan and exact-capacity non-report metadata-alias pairs per scalar. Slice07 runs34 tests per ABI:26 pass,8 retained mathematical failures, no skips, exit8.

Slice08 adds GBSVX singular and accuracy-warning profiles for all N/T/C and independent B/X layouts. Both actual ABI Debug runs42 tests:30 pass,12 fail, no skips, exit8. All27 new C++ source/header identities are frozen under slice-08-source/ with slice-08-source.json. The four new extreme GBSVX gates fail on singleton A=min_normal/8: N/F return INFO=2, RCOND=0 instead of independently exact1, with FERR=inf, BERR approximately8/9 and correct X=1. Direct pinned controls without ASC reproduce all4scalars/bothABIs, whereas E returns RCOND1/INFO0 (direct-gbsvx-commands.json and logs). Raw source remains unchanged. The extreme test also incorrectly expects no RHS diagnostic despite infinite FERR; that assertion will be corrected to the documented RHS0 warning, preserving the independent mathematical failure. All12 required gates remain open.

Next: complete preflight/workspace/pivot/scale and post-native diagnostic fault acceptance, strict checks of all new code and public headers/Doxygen, then coherent all-test runs across both actual ABIs and build/sanitizer/allocation configurations. No integration, installed, complete family acceptance or release success is claimed.

## Slice09 executed addendum

Both actual ABI Debug builds and coherent42-test runs completed:30 pass,12 unchanged required mathematical failures, zero skips, exit8. The GBSVX extreme RHS0 diagnostic assertion now follows the public nonfinite-FERR contract; the independent condition=1 check remains failing. Additive matching-plan checks pass for every required scratch region (one-byte short, exact-capacity misalignment, device placement, operand alias), all consumer pivot values including a legal-global but out-of-band pivot, last zero U diagonal, every selected-scale zero/negative/Inf/NaN, and21 post-native diagnostic faults per scalar including negative-RHS precedence over earlier NaN. Prior91 native sentinel faults and all stale/metadata-alias assertions remain. Frozen27-file snapshot: slice-09-source/ and manifest.

All24 standalone public-header compiles pass (six headers, GNU11/Clang19, both actual ABI configurations; compact templates instantiated for all4 mutable/const scalars). Second strict diagnostics pass every production TU and five regular numerical tests plus full ABI test; remaining test-only include/nested-conditional hygiene corrections are identified. Final strict identities/configurations and Doxygen remain open.

## Slice10 executed addendum

All74 CTests execute in each actual ABI GNU11 Debug build:58 pass,16 required mathematical failures, no skips, exit8. The28 fresh-process route/scalar tests invoke no earlier foreign call: CON/RFS/F use exact source-derived identity factors/raw pivots, and each first query/native execution passes C++/static-libc allocation probes. Factory tests retain invalid signed raw pivot values without scanning and reject nonunit stride/device placement. All prior structural/native/quality assertions remain. Frozen27-file snapshot: slice-10-source/ and manifest.

The4 new GBRFS extreme gates fail finite FERR and independent backward-error agreement for the exact singleton min_normal/8 problem. Direct pinned calls (no ASC) reproduce every N/T/C mode/all4scalars/bothABIs: INFO0, X1, FERRinf, BERR approximately8/9, independent backward error0. Each direct process exits12; commands/source/logs use direct-gbrfs-*. The ASC nonfinite warning and RHS0 diagnostic pass. All16 mathematical gates (GBEQU4/GBCON4/GBRFS4/GBSVX4) remain required and failing; no numerical substitution or suppression is applied.

Doxygen1.9.8 succeeds without warnings and reports all67 public function declarations documented; six standalone headers pass GNU11/Clang19 for both actual ABI configurations (24 checks). Next: complete strict checks and serial coherent Release/Clang ASan+UBSan/libc-probe configurations, then freeze the bounded handoff.

## Source-derived admission and integration requirements

`internal_lu_band_expert_counts.h` composes the unchanged audited GB8 arithmetic with each new source path. GBEQU checks compact KL+KU+1 before any empty return and active M+1/N+1, J+KL, KU+1+I before subtraction, and M+N INFO arithmetic. GBCON adds LACN2 3*N and upper LATBS K+I-before-subtraction bounds, K=KL+KU. GBRFS adds KL+KU+2 before MIN, N+KL, source/foreign leading dimensions, NRHS terminal cursor even for N=0, and every nested one-RHS GBTRS boundary. GBSVX composes factor/solve/condition/refinement, adds E's equilibration and all N+KU+1, K+2 and N+KL norm/copy/scaling cursors. Original ASC B/X strides stay wide; only actual packed leading dimensions narrow. Real and complex WORK/IWORK/RWORK differ exactly as emitted prototypes require; the GBSVX growth slot remains live even at N=0. No WORK query is invented for these fixed-workspace routines.

The root-source and object admission records bind all20 exact roots to146 members in each actual ABI. Every member's source and object hash, raw symbol/import list and writable-static scan is recorded. Only source-conditioned ILAENV ISPEC1 GB/TRF (N4=KU: NB1 when KU<=64, otherwise32) and preflight-excluded XERBLA diagnostic edges are conditioned; required routines remain required. The five external leaves cabs/cabsf/memcmp/memcpy/memset remain explicit. Slice10 revalidation compared322 dependency/header/archive/source identities with no mismatch and a clean pinned source tree. INFO is initialized to full native MIN for every actual call, and N/E/SV pivots receive full native MIN only after preflight. Input pivots retain signed band semantics and are copied by value into native storage. IWORK is a separate live native tail, never ASC index_t reinterpreted storage.

Root integration must import only the27 new C++ files plus this owner review; the14 copied dependency/guard paths are not owned changes and must not replace the root's guarded GB8 tests. Add all five new production translation units and six public headers through existing root-owned provider/component contracts and registries. Numerical tests include all74 CTests in the final external harness, with every16 mathematical gate still an ordinary failing test. ABI test compilation must supply ASC_GB_EXPERT_EMITTED_HEADER using the recorded actual-ABI emitted header aggregation; the diagnostic wrappers require all20 --wrap symbols exactly as the harness records. Guard the foreign-calling tests with the unchanged installed_lu/normal_return_guard.h. Root owns public installed consumers and real package/module/isolation registration. This branch performs neither those shared edits nor installed verification. GBEQUB4 remains a separate required next family; no verified-family or P05-complete label is justified by provider fidelity.

## Final snapshot01 fixture correction

Independent review found that the cold GBCON identity-factor fixture inherited ANORM=8 from the generic fault fixture. Although its native call and allocation check executed, that norm did not match the identity matrix. The cold fixture now supplies norm1 and independently expects RCOND1. No production code changed. Snapshot01 and completed strict logs remain preserved; its active ILP64 GBSV strict job was explicitly terminated and earns no pass, and its unstarted runtime matrix earns no evidence. `final-01-interruption.json` records the stopped stale chain. New immutable final-source-02/ with manifest supersedes snapshot01 for final verification. Completed exact-identity strict checks can be reused with all included headers/configurations unchanged; the modified fault/cold TU is rechecked for both ABIs. All74 tests will execute afresh in each final configuration.

## Final02 executed handoff addendum

Every final configuration has successful configure/build exits0 and coherent CTest exit8 with exactly the same16 mathematical failures; no skips or additional failures. Both sanitizer logs contain no AddressSanitizer/UndefinedBehaviorSanitizer findings. ASC and tests are instrumented; the admitted pinned Fortran archives are not sanitizer-instrumented, so source bounds and all caller redzones remain necessary evidence. Both libc-probe configurations, including all28 corrected fresh-process first-native-call cases, pass their allocation assertions. This audits the linked static objects and C++ allocation paths, not arbitrary shared-runtime internals.

All30 strict TU/ABI records are complete and passing. Nineteen unchanged completed snapshot01 records retain their exact source/header/configuration identities;11 checks were executed on snapshot02, including the corrected cold/fault test in both ABIs. No terminated or unstarted job is credited. Public headers remain byte-identical to the executed24 header and67-function warning-free Doxygen checks. The final27 C++ files match final-source-02.json; all14 copied GB8/guard dependency paths remain unchanged.

Exact next task is root review/import of the new28-file handoff (27 C++ files plus this review), followed by root-owned registrations and independent installed/public/package verification on the integrated source identity. Verify handoff/source-manifest.json before copying; do not copy the14 dependency paths from the diagnostic source context. The external harness CMakeLists documents all74 CTests, exact emitted-prototype macro and20 fault-wrapper link options. After integration, GBEQUB4 remains required separate work. No commit, push, merge, publication, shared-registry edit, root worktree edit or source patch was performed. No background verification remains running for this handoff.

## Root integration V13, current verification pending

The root read all six public headers, three private helpers, five production
translation units and the complete owner review. An independent reviewer read
all thirteen test/support files. Root audit
`p05-general-band-expert-root-review-01/audit.json`, SHA256
`f32b1cfeff8739d32d1c8edce29f507edead667078b21b8ae3524f988db7e822`,
reconciles 417 evidence artifacts, the eight original 74-test lanes and all
146 pinned archive members per ABI. Those lanes rebuild eight ASC translation
units and reuse older Core/Dense archives; they are not full current ASC
sanitizer evidence.

The separately reviewed acceptance02 delta uses live scalar arrays and an
actual plan object-representation workspace overlap. All 63 original fault
assertions remain unchanged. Its additive test checks 288 exact-diagonal
GBSVX statistics and 48 rational GBRFS forward-error profiles per lane.
All eight 75-test lanes retain exactly 16 required mathematical failures,
zero skips. Deliberate wrong-statistic and tiny-positive-FERR controls fail
with 576 and 144 assertions per ABI. Scalar/plan aliases, general
nondiagonal statistics, nonunit pivot growth and further output-fault modes
remain required. Root audit
`p05-general-band-acceptance-consumer-root-review-01/audit.json`, SHA256
`9f1a45293b9ce013b167539469087f4f759d564806112c9ea337566680673066`,
checks 34 delta records, 840 build artifacts, 450 compiler dependencies and
eight independent consumer records.

Root public consumer06 passes both actual ABIs and strict checking. It uses
the independently invertible band matrix [[0,2,0],[1,3,4],[0,5,6]], distinct
row/column scaling and complex phases, with 96 workflows, 864 calls and
96 stale-plan rejections. It reconstructs factors, preserves padding and
const operands, and exercises N/T/C, independent B/X layouts and N/E/F reuse.
This preliminary test links the sealed owner archive; installed and current
root verification remain separate.

Consumer02's unscaled forward-error bound was inappropriate for its scaled
matrix, whose exact infinity condition is 6784 (7100 when transposed).
Direct pinned SGBSVX in both ABIs reproduces the exact rejected value
1.00016272, INFO=0 and FERR=0.000695281662. Candidate05 uses the explicitly
known inverse [[2,12,-8],[6,0,0],[-5,0,2]]/12, transformed by the independent
scales/phases, to derive componentwise forward sensitivity. The original
128-epsilon backward-error check and unscaled check remain unchanged.
Independent inverse-identity and deliberately wrong-solution controls pass.
Candidate06 only adds two required direct includes and an explicit conversion;
its numerical and strict checks pass. All earlier failed sources/logs remain
preserved; no frozen mathematical gate changed.

The actual compiler-emission comparison remains frozen externally with all
twenty complete declarations in both ABIs. Root adds a separately named
portable CXX pinned-signature probe; it does not replace the required actual
emission evidence or introduce Fortran into installed CXX consumers. Raw
generated absolute-path headers are not repository or installed API files.
All 73 original numerical/fault/count selections plus the additive diagnostic
are registered, including all sixteen mathematical failures; the portable
probe is separate. This integration raises the development mapping to
270 partial Reference rows and 108 public headers. No row is fully verified.
The 1,843 unimplemented required rows, native20 acceptance, missing XBLAS,
platforms, shared provider and full contracts/evidence remain open.


## V13 composed integration checkpoint

GB20 V13 full02 LP64/ILP64 each720/752, exactly30 required numerical failures plus two omitted-source-inventory failures, zero skips. Provider-free Debug280/282, Release280/282, shared282/284 retain the same two inventory failures. Final04 adds only five explicit source entries; the common source-only2/2 replay passes and its original commands match all five full lanes. Final03 formats only the new consumer and updates its mapping hashes: four fresh current-library and two fresh relocated-installed1/1 consumer tests pass. Full02 package passes17 installed families perABI with unchanged300s timeout. All58 primary CPU Core/Dense/provider TUs rebuilt in each full and ASC-only sanitizer lane; ASan each75/91 retains exactly16 GB math failures, no sanitizer diagnostics or skips. All34 changed-TU strict checks plus two final-consumer strict checks, format29, coverage, public surface, Markdown and Doxygen108headers/2036members/zero warnings pass. Exact consumer/object/provider closures audited. Original01 ABI-registration,02 format/source-policy and audit-helper failures remain preserved. No full04 suite, foreign sanitizer, normalized full-mode, platform, XBLAS or mathematical gate closure claim;270 Reference rows remain in_progress/1843 not_started of2113 required; native20 separately implemented_unverified; zero fullyverified.

Final product tree `eeb249415cbe7c8d0b0127f8af290f386bca1356`, archive SHA256
`15ff5e4717103d77eaf9ce6967f2338f410fd682242f2a7abf11bf1754f0bbfd`, mapping
`442e2f8c421b9ec5fd6149eaa23981b4abb3f01ba5e03fb7c6c747cadb3e4474`. Completion audit `p05-general-band-expert-integration-v13-04/completion-audit-01/audit.json`
SHA256 `99914ff87c875d8fdac08359ab600ce0787fdad273fa2c678df7ab972d67f32e` binds the executed records, primary production objects,
final public consumers and unchanged actual provider prefixes.
The audit first included a separately rebuilt subproject consumer in
its primary-library uniqueness check; that failed attempt remains
recorded. The final audit checks the exact primary library target
object directories, still requiring all58 production sources.
