The original GETRF2/GETF2/GETRI/GESV adapter now initializes its twelve native
INFO destinations to the full selected `lapack_int` minimum. An omitted INFO
write therefore cannot manufacture successful execution or a valid GETRI plan.
The exercised short four-byte zero write also remains invalid on the audited
little-endian true-ILP64 ABI. This is a bounded INFO correction, not detection of
all possible provider corruption.

Recovery used actual root HEAD
`c98f5284c05e07011f8e6606effe8ad1b428fe67`, branch
`feature/lapack-array-io`, preserving the uncommitted V12 Band20 work. The exact
status, full source file hashes and dirty diff are in `recovered-source.json`
and `root-diff.patch`. Only new external snapshots/artifacts were written. The
root, shared registrations/manifests, previous GETRF/GETRS02 and SVD handoffs,
and other owners' work remain untouched.

The only production file is `src/dense/lapack/reference_lu_expert.cc`:

- Recovered SHA-256:
  `230d5091ec78ceecdc78c69a06a9019b2980ea5d30d7e6845b9013d6172af100`.
- Corrected SHA-256:
  `dfb9e1f3607d1d34c06c6aa335a34f6023aa09316973c32bdb65d01c73c7c3a5`.

None of its twelve initialization sites was already protected. Four sites are
shared by GETRF2/GETF2, four by GETRI query/execution, and four serve GESV.
The production change contains those twelve seeds and one explanatory comment.
Its existing MIN-safe signed-INFO interpretation and publication logic remain
unchanged. Full-width MIN is preserved in the report, with no invalid negation
or fabricated argument position. The call returns `kProvider`, records
`kProviderArgument`, and retains routine/provider identity and
`called_provider=true`. Diagnostic index and native argument are absent.

GETRI query retains its existing `kUnchanged` output validity, including an
INFO defect. Query failure returns no execution plan. Empty GETRI queries still
make an actual LWORK=-1 native call; empty execution makes no native call and
leaves INFO absent. The tests enforce zero query calls during GETRI execution.
GETRI successful execution uses both minimum and preferred caller WORK, and
its real positive singular outcome preserves the entire input factor unchanged.

GETRF2/GETF2/GESV malformed INFO does not publish converted pivots or unpack a
row-major numerical output. Direct column-major native mutations remain marked
unusable. Normal positive singular INFO still publishes completed raw LU and
validated pivots. GESV then leaves B unchanged. GESV with n>0 and nrhs=0 still
factors A. Empty factorizations, drivers and inverse execution complete without
native calls or invented INFO. Public contracts and all preexisting tests and
assertions are byte-identical to recovery (`unchanged-contracts-tests.json`).

The test-only wrappers call the unmodified pinned native function with every
original argument except its INFO destination. They receive the real native
INFO in a full-width local, then either publish it, omit only its destination
write, or perform the explicit true-ILP64 four-byte zero write. A test-only
nesting guard leaves recursive LU and GESV's nested native calls unmodified;
only the outer ASC call receives the injected INFO defect. Every `__real_` and
`__wrap_` declaration has a compile-time exact-type check against the pinned
`lapack.h` declaration: 32 checks for sixteen S/D/C/Z native routines. No
foreign source, numerical operand, pivot destination, error handler or provider
registration is patched by these controls.

The new tests contain per-scalar finite mode matrices:

- GETRF2 and GETF2: 1x1, 3x3, 4x3, 3x4, 0x3 and 3x0; both padded layouts;
  nonsingular and exact-singular representatives.
- GESV: orders 0, 1 and 3; zero, one and two RHS; A and B layouts independently;
  nonsingular and exact-singular representatives.
- GETRI query: orders 0, 1 and 3, both layouts, nonsingular/singular raw factors.
- GETRI execution: the same orders/layouts/factors, minimum and preferred WORK.

Each mode has a normal control and an INFO-withheld control. Actual ILP64 adds
a short-write control. The full new-test count is 312 profiles per scalar in
LP64 and 468 in ILP64, including explicit empty operations. These contain 216
and 324 observed native calls per scalar respectively, excluding setup calls.
Every normal-returning call is checked for exact native success or the expected
singular index. Normal controls independently reconstruct P*A=L*U, verify known
solutions, and verify both left and right inverse products in wider arithmetic.
All modes check report reset, original/published storage, padding, raw pivot
preservation or publication, workspace guards, and C/C++ allocation observation.
The injected INFO modes are defensive evidence, not additional successful
capability rows.

The new foreign-calling main uses the exact common NormalReturnGuard, SHA-256
`6d0d088037e666f0e9c92d912dbddb2a7c48501284183c52cb79e3b0c9bdc81d`.
The existing guard test remains selected. An early provider exit cannot count
as normal test completion.

The final evidence table and artifact hashes are recorded in `handoff.json`.
All runs use actual separate LP64 and true ILP64 pinned static providers. The
old-code regressions fail all four scalar tests on each ABI. They fail only the
new INFO/report/publication assertions and complete every declared mode. The
corrected selection retains the original reference-LU, LU-layout and LU-expert
S/D/C/Z tests, LU count test and normal-return test, adding the four new INFO
tests: eighteen tests per ABI/configuration. Runtime, strict, formatting and
exact-mode audit records are separate; no failed check is relabeled as passing.

`pinned-source-review.json` records pristine Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, hashes every selected source, and
locates INFO assignments, query/return paths and nested native calls. The source
routine INFO sets exclude full-width MIN. The source's real query, empty and
singular behavior is exercised directly by the controls above.

Dependency scope is deliberately bounded. Release and scoped Debug rebuild the
affected adapter and tests against hashed V11 Release ASC dependencies. This is
not an all-ASC-Debug closure. ASan/UBSan rebuilds affected code and links the
fully instrumented V11 ASC archive. Its complete Core/Dense source/header
closure matches the recovered corresponding files (`borrowed-source-closure.json`),
including private headers; new unlinked V12 Band20 files are outside this scope.
The rebuilt adapter supplies its symbols before archive extraction. Pinned
LAPACK/BLAS and Fortran runtime archives remain uninstrumented. Actual binary,
header, provider configuration, compile flags/database and runtime hashes are
bound in matching before/after dependency manifests. No shared-provider,
installed-consumer, other-platform, full BLAS/Random or full-program gate is
claimed by this isolated selection.

Production strict analysis completed on the exact corrected TU in snapshot02.
Snapshot03 changes only test code: direct includes, constant naming, explicit
printf boolean casts and explicit deleted move members for the nesting guard.
`reused-production-strict.json` compares every production header and the TU
byte-for-byte, binds unchanged dependencies, and names the two actual strict
records. The changed test TUs and support headers receive fresh strict analysis
in snapshot03. No warning suppression was added to fix a diagnostic.

Both earlier attempts remain preserved. Snapshot01 failed both builds because
the new test tried default-constructing a plan whose identity factory is
mandatory; no tests ran. Snapshot02 reproduced the old defects before any
production change, then passed six eighteen-test corrected runs and the full
mode audit, but strict rejected the new test-only issues described above.
Snapshot03 preserves every assertion and mode while fixing those issues, then
reexecutes both old controls and all six corrected lanes. Exact commands and
exit codes remain in their original records.

The five-file `correction.patch` applies under `git apply --check` to the root.
`integration-registration.cmake` is an unapplied, reviewable registration using
existing repository helpers. The root integration owner must review/apply the
patch, add that registration, freeze the composed implementation and execute
its required integration evidence. Public headers/manifests were not edited in
this bounded task.

The separate output-pivot candidate is intentionally unexecuted. Original
GETRF Factor and expert GETRF2/GETF2 Factor and GESV Driver zero-initialize
provider output pivot arrays. A short positive low-half write on little-endian
ILP64 may therefore be hidden by zero high bits. This is a source observation,
not demonstrated failure or authorization to change pivot semantics here.
GETRI ConvertPivots explicitly writes every full-width input pivot and is not
an output-pivot case. `separate-pivot-candidates.json` records the next independent
actual-native pivot-destination omission/short-write controls to determine
whether a MIN seed is needed. Existing SVD mathematical/source gates, other
provider INFO candidates and full pinned CPU capability remain open.


## Root import

Root read all four new test/support files, the twelve-seed production patch and report/publication/query paths, the complete review/registration and mode parser. Root audit `p04-lu-expert-info-root-review-01/audit.json` SHA256 `77936278aa2198c3175799c6aa8ed72bbf49f7e0c0eb5b44e1cedf7716b245f9` independently checks32 command records,23 artifacts,48 external dependency hashes and183 borrowed source inputs. The root rerun of the exact actual-mode audit passes and matches its preserved output; no historical command is relabeled current. Exact five files imported, four new tests registered, all16 affected mapping hashes refreshed. Current combined22-test integration remains pending; output pivots remain a separate active correction.


## Current composed integration completed

Current originalLU8+expert16 INFO integration: six22/22 Debug/Release/ASC-only ASan lanes pass,0skips. All53CPUCore/Dense/provider TUs rebuilt eachlane; exact312/468expert modes/scalar and preserved112/168originalINFO profiles pass. Format5/coverage/publicsurface pass. Historical6strict records apply through byte-identical actual compiler-read changedTU/headers/generatedABIconfig; no freshstrict or fullproject claim. Foreign archives/runtimes unsanitized; output-pivot correction and full mathematical/mode/platform gates remain separate.

Frozen tree `e8d719945740a6640fa963a37c68b73c64b9f0ae`, archive SHA256 `7f75537dafded1f9ae2b8e002e50f41285abed84c6ae73dac126d99acda10211`, mapping `d566272179926c1ae9c67dabccc0cfd37acca14b021cd31b4c471c868d64c4da`. Completion audit `p04-lu-expert-info-integration-01/completion-audit-01/audit.json` SHA256 `e6da4a3267fad30efe4ab36e19bdaabf64eff43da37e2f67fb2b4f4527e50dee` verifies21 executed records,921 exact source files, every compiled CPU source and actual returned/native mode profiles. Root product files match the frozen source.
