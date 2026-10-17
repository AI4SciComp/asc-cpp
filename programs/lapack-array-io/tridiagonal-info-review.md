# GT24 output sentinel and metadata contract review

This bounded external correction applies to the exact twenty owned C++ files
from `p05-tridiagonal-04/source`, bound by verification-ledger SHA256
`dba522f06850ab904a6d074dc62231e31a9c9a341c136be7802b803141169371`.
The isolated owner worktree remains at
`b1b78d789fb8f5366f13f86c411832df842b2755` on
`feature/lapack-p05-tridiagonal`. Both the worktree and frozen04 remain
unchanged. The completed GB handoff remains untouched. No shared registry,
consumer, contract generator, source lock or upstream source is edited.

External evidence root:
`/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/p05-tridiagonal-sentinel-01-vskus1c2`.
`source-old` contains exact04 production plus the additive test/normal-return
overlays; `source-fixed` additionally contains the proposed correction. Both
complete source manifests bind every byte. The thirteen-file patch applies
cleanly to exact04 and yields the tested corrected files; the application-check
record and before/after hashes are preserved separately.

## Findings and correction

1. At original `internal_tridiagonal.h:332`, placement array-new creates the
   native integer objects without populating the output pivots. Original
   `reference_tridiagonal.cc:205` and
   `reference_tridiagonal_expert_driver.cc:266` use those bytes as GTTRF and
   GTSVX FACT=N output, then validate and publish them. Actual LP64 and ILP64
   old-source controls demonstrate that prior-valid but wholly unwritten pivot
   bytes can be accepted as successful fresh output. The isolated one-pivot
   ILP64 controls also accept a write of only the low four bytes of pivot 1
   when prior bytes were either zero or valid. No second pivot masks that
   width defect. Initially-zero/no-write controls reject as expected; those
   control results are retained. These observations establish false acceptance
   under injected defects; they do not establish an undefined-behavior or
   uninitialized-memory diagnostic.

   New private `OutputPivotObjects` at corrected helper line341 starts the
   existing native objects and fills all n pivots with native signed MIN.
   Its only two callers execute after complete structural preflight and the
   empty check. FACT=F and other input-pivot consumers still fully populate
   their own native input conversion; the separate estimator array is not
   repurposed. The fixed controls reject all no-write/partial-width cases,
   retain the native INFO=0 defect return and leave public pivot output and
   packed solution destinations unpublished.

2. Each of the twenty-four native wrappers previously initialized INFO=0.
   For all six routine families and all four scalars, a real successful native
   call was made with its INFO destination redirected to a private local in
   the injection wrapper. The original ASC INFO destination remained untouched.
   All twenty-eight scalar/routine/mode controls per ABI (including separate
   GTSVX FACT=N/F) falsely returned success with reported INFO=0. Every pinned
   routine itself assigns INFO=0, so complete native output is required;
   `native-output-source-audit.json` binds all twenty-four exact source hashes
   and relevant numbered statements.

   All twenty-four ASC native INFO locals now start with native signed MIN,
   with direct `<limits>` includes in the five affected translation units.
   Existing `ProviderDefect` already preserves MIN and avoids negating it into
   an overflowing argument index. The corrected controls report kProvider,
   kUnusable, the exact native-width MIN, called_provider=true, and no invented
   native argument. No source algorithm, native diagnostic translation, empty
   behavior, layout choice or successful numerical result is substituted.

3. Original public header lines37-39 described report preservation only for
   aliases involving the report itself. Actual `CheckMetadata` at helper103
   rejects every detected metadata-to-metadata, metadata-to-operand or
   metadata-to-workspace alias before resetting the report. The public wording
   now describes that complete rule. Added exact-capacity, aligned, real
   backing tests exercise workspace-to-plan aliases for every operation and
   FACT=N/F, plus a live pivot operand aliasing an actual plan integer field.
   They verify zero native calls, byte-identical report preservation, unchanged
   numerical buffers and unchanged scratch. Their valid size/alignment/plan
   identities prevent another structural rejection from masking this rule.

## Executed evidence

The old-source control builds pass and the four separately registered boundary
processes fail on each actual ABI, with no skips or expected-failure properties.
Their logs demonstrate 32 false pivot successes and 56 false INFO successes
across both ABIs. This is fault-injection evidence, separate from mathematical
verification and from the native calls used for ordinary numerical controls.

Every corrected Debug, Release, ASan/UBSan and libc-observation lane on both
actual ABIs selects and executes all original GT29 tests plus four additional
boundary processes. Each lane has 29 passes, the same four required extreme
mathematical failures, zero skips and CTest exit8. Across eight lanes there are
264 executed tests, 232 passes and 32 retained failures. `executed-final.json`
binds each real JUnit and LastTest log. Every added pivot/INFO/metadata test
passes. The GNU libc lanes each observe 28 positive controls and no scoped
allocation attempts. Thirty-two separate cold FACT=N/F processes were also
rerun from current binaries in Release/libc on both ABIs; all return zero,
with all sixteen libc positive controls observed.

The original mathematical, expert and extreme-test bodies are byte-identical
apart from the normal-return guard include and first main local. Existing
fault-test functions and all their assertions are byte-identical before the
new appended functions. The pure-count test is unchanged. The fault main
retains its original scalar-only mode and adds the distinct `boundary` mode.
`prior-assertion-preservation.json` records these exact checks. No tolerance,
selection, assertion, required row or failure property was weakened.

The unchanged root test-only normal-return guard is copied into the external
source at its existing relative installed-support path. SHA256 is
`6d0d088037e666f0e9c92d912dbddb2a7c48501284183c52cb79e3b0c9bdc81d`.
Every foreign-calling GT main creates it as the first local, before provider
setup or allocation observation; the pure arithmetic count main is exempt.
The guard is an explicit existing root dependency, excluded from the thirteen-
file patch. All twenty-four complete native wrapper type comparisons remain
compiled on both actual ABIs, including hidden CHARACTER lengths.

Full formatting of the twenty owned C++ files and guard passes. All four
public headers pass self-contained C++20/no-exception probes with GNU11 and
Clang19 (eight checks). Doxygen warning-as-error generation passes, and its XML identifies all63
public function members as documented. All eleven production/test translation
units pass complete repository Clang18 rules with warnings as errors for both
actual ABIs (22 translation-unit/ABI checks). The earlier successful LP64
fault-test check binds the same unchanged corrected source; its explicit
command is reused rather than rerun. `strict-commands.json` identifies every
executed check and that exact-source reuse. The owned private/public headers
and the normal-return guard are selected by the strict header filter.

Exact04 external dependency hashes were checked unchanged before these runs.
The same frozen V8 base C++ archives/configurations and pinned Reference-LAPACK
3.12.1 commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` are used. ILP64
uses the actual globally 64-bit provider and matching BLAS, not a suffixed
compatibility shim. All387 compiler-recorded dependencies are hashed; all29 consumed nonowned
ASC source/header inputs match frozen V8. The existing full provider/runtime
closure audit remains
identified by04 records; no provider or source identity changed. ASan/UBSan
instruments the corrected GT source/tests and links the recorded instrumented
V8 C++ base; the pinned Fortran archives remain uninstrumented. Boundary guards
and allocation observations are separate evidence. No installed, relocated,
full-profile or independently instrumented Fortran claim is made.

## Open work and root integration

The four required `tridiagonal_extreme_math_{s,d,c,z}` tests remain failing for
pinned GTCON/GTRFS/GTSVX behavior on the representable subnormal singleton.
They are not waived, expected-failed, omitted or recategorized. The frozen
math assertions and upstream source remain unchanged. Full-profile optional
dependencies, source-owner/license gates and all other program requirements
remain open under the central runbook state.

Root should review/apply
`tridiagonal-sentinel.patch` to the unchanged GT20 import, ensure the existing
normal-return guard dependency remains identical, and register four additive
CTest cases invoking `tridiagonal_fault_test <s|d|c|z> boundary`. The external
CMake harness records these exact registrations without editing shared files.
Revalidate integration and the independent public consumer on the resulting
source identity; this task did not edit or claim the root consumer's evidence.

The live [Google C++](https://google.github.io/styleguide/cppguide.html) and
[Python](https://google.github.io/styleguide/pyguide.html) guides were retrieved
again during this review; URLs, UTC times and exact hashes are in
`live-style-guides.json`. Existing C++20 explicit-provider/type conventions,
private GNU linker wrapper names and existing narrowly scoped diagnostics
remain the recorded repository exceptions. No new suppression is added.

Exact next root action: inspect `patch-file-identities.json`, then apply
`tridiagonal-sentinel.patch` to the unchanged twenty-file GT04 import. The
external `patch-application-check.json` proves the patch applies to those exact
before hashes and yields every tested after hash. The four additive CTest
registrations appear at the end of the external `CMakeLists.txt`. Root owns
integration and installed/public-consumer revalidation. This owner task has
no remaining running process or pending check and performs no commit/import.
