The eight original Reference-provider GETRF/GETRS call sites now initialize
native INFO to the full selected `lapack_int` minimum. This prevents an untouched
INFO destination from manufacturing success. On the audited little-endian true
ILP64 ABI, it also exposes the exercised short 32-bit zero write. It does not
claim detection of every possible provider memory corruption.

The recovery boundary is root branch `feature/lapack-array-io`, commit
`a0a1e156041015636534627098e1e025e90e2ac3`, with preserved uncommitted V10
integration work. `recovered-source.json` and `root-diff.patch` record that actual
state. The root and all shared manifests remained read-only. No worktrees,
branches, upstream files, or other owners' implementations were changed.

The production patch contains only eight initializations and their explanatory
comment in `reference_lu.cc`. Its old SHA-256 is
`411000fb94916ff9b50d8756e1373ec32c822babfa58f64b391fd6077ea920ab`, exactly the audit
candidate. Its new SHA-256 is
`69a6b7a2e9f4f6d6483164e60d8be630f3db79acf07328effad6881697484272`.
The public headers, all preexisting assertions, and shared registrations are
unchanged. The existing `InterpretInfo` already excludes native MIN before
signed negation; that path now receives an unwritten sentinel safely. It retains
MIN in `native_info`, leaves `native_argument` and `diagnostic_index` absent,
returns `kProvider`, and marks `kProviderArgument/kUnusable`. The report retains
the attempted provider/routine and `called_provider=true`. This preserves the
previous invalid-MIN report behavior; it does not invent `INFO=0`.

The publication rules remain unchanged. GETRF does not publish converted pivots
on this failure. Row-major output is not unpacked. A column-major operand may
already contain the native result, so the report correctly marks it unusable.
GETRS preserves the factor and raw pivots; row-major B stays original while a
column-major B may contain the native result. The tests compare all storage,
padding, pivot canaries, and workspace guards against the positive control and
original destinations as appropriate. No transactional guarantee is added for
an already executed foreign call.

The pinned source remains pristine Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. `pinned-lu-source.json` hashes all eight
routines and records exact source lines. All four GETRF sources initialize INFO
at line 145 and document only zero, bounded positive singular indices, or their
small negative argument positions. All four GETRS sources initialize INFO at
line 158, with negative positions -1/-2/-3/-5/-8 for invalid input. Native MIN is
therefore outside the documented result set. The S/D transpose paths and C/Z
TRANS paths remain distinct and are exercised with N/T/C.

The test-only GNU ELF wrappers execute the real pinned routine with the exact
original numerical arguments and a separate full-width INFO destination. Every
actual native INFO must be zero. Positive controls copy it to the original
INFO destination; withheld controls leave that destination untouched. A third
true-ILP64 control copies only four zero bytes, modeling a short output after a
successful native computation. There is no upstream patch, no XERBLA override,
and no interception of numerical operands. Withheld/short outputs are defensive
failure evidence, not an additional claim of successful numerical coverage.

The finite tested matrix contains GETRF shapes 1x1, 3x3, 4x3, 3x4 with both padded
layouts. GETRS covers orders 1 and 3, both factor and RHS layouts independently,
N/T/C, and one or two RHS. Each S/D/C/Z instance has 56 positive controls and 56
withheld controls per ABI. True ILP64 additionally has 56 short-write controls.
Positive controls independently reconstruct P*A=L*U in wider arithmetic and
check the known solution. All operations retain C/C++ allocation observation.
The new foreign-calling main uses the exact current common NormalReturnGuard;
its hash is `6d0d088037e666f0e9c92d912dbddb2a7c48501284183c52cb79e3b0c9bdc81d`.

| Final source selection | LP64 | True ILP64 |
| --- | --- | --- |
| Old Release + new INFO regression | 4/4 fail as required | 4/4 fail as required |
| Fixed Release | 18/18 pass | 18/18 pass |
| Fixed scoped Debug | 18/18 pass | 18/18 pass |
| Fixed Clang19 ASan/UBSan | 18/18 pass | 18/18 pass |
| Strict Clang18, three changed translation units | 3/3 pass | 3/3 pass |

Every CTest selection has zero skips. Each fixed run retains the original
reference_lu, lu_layout, lu_expert S/D/C/Z tests, LU counts, normal-return guard,
and the four additive INFO tests. Whole-file formatting passes for all four
changed/new C++ files. `completed-profile-audit.json` checks actual unique mode
records, positive native INFO, exact old false-success outcomes, expected
failure assertions only, and normal completion. Each run contains exactly 448
LP64 or 672 ILP64 observed native calls in the new test.

The scope of dependency evidence is explicit. Release and scoped Debug rebuild
the affected adapter and test code, using hashed Release V9-02 Core/Dense/other
provider archives. Debug is therefore not an all-ASC-Debug closure. ASan/UBSan
rebuilds the affected adapter/tests and links the previously built fully
instrumented V10 ASC archive. The exact V10 source closure equals this recovered
ASC closure; the V9 source differs only in an unrelated indefinite-header
comment, recorded in `borrowed-asc-source-closures.json`. The new adapter supplies
all its symbols before archive extraction. Pinned LAPACK/BLAS and their Fortran
runtimes are uninstrumented. `dependencies-before.json` and
`dependencies-after.json` match and bind the actual archives, headers,
configuration, compiler flags/commands and runtime files. No full-program,
shared-provider, other-platform, installed-consumer, foreign-instrumentation or
final full-profile claim is made here.

All prior evidence is preserved in `../p04-lu-info-output-01`: old regressions
failed, fixed six lanes passed, and strict review failed on three test style
issues. The final snapshot fixes a copied status parameter, a nested conditional,
and an oversized test function; no assertion was deleted. It adds actual mode
log records. The first setup tried a nonexistent V9 compile database; it ran no
build/tests and was corrected to bind existing flags/link files. An inline audit
syntax typo ran no audit. The final audit's first relative manifest path was
resolved against the source directory and refused before execution; the recorded
absolute-path invocation passed. None of these failed attempts is relabeled.

`correction.patch` applies cleanly under `git apply --check` to the integration
root. The integration owner must review/apply the four-file patch, add the
registration shown in `integration-registration.cmake`, freeze the composed
source, and run its required integration checks. The helper names in that
snippet already exist in the root test registration; it is an unapplied review
artifact, not a shared-file edit.

The remaining original-LU INFO candidates are still open: the twelve sites in
`reference_lu_expert.cc` serve GETRF2/GETF2, GETRI execution/query, and GESV across
S/D/C/Z. GESVX is in `reference_lu_driver.cc` and also remains open, as do other
families in the original audit. The SVD finite-qd, driver reduction/cursor,
LASQ restoration and required numerical-source findings remain preserved in the
prior SVD handoff. This bounded correction closes none of those mathematical or
full-capability gates.


## Root import and independent replay

Root read all four corrected files, the complete patch, actual registrations and preserved review. Audit `p04-lu-info-root-review-01/audit.json` SHA256 `36e974f216c2d455836edd60fc5704aa01dcb351704961a104352ecb335d1cbe` verifies33 command records,20 handoff artifacts and46 dependency hashes; separate root compiler checks match all8 wrapper prototypes to pinned declarations on each ABI.

Fresh root replay uses a new CTest harness so historical logs remain intact. Both old Release lanes fail4/4; corrected Release and ASC-only ASan each pass4/4 on both ABIs. Every actual native INFO is zero; unique mode sets contain112 profiles/scalar LP64 and168 true ILP64, including deliberate short32 INFO writes only in ILP64. Root replay audit `p04-lu-info-root-runtime-01/audit.json` SHA256 `56e50f86ac84acf6483965460375afceb0c949e9f38cbf4b8090c2b2f4fbe0d9`. These rebuilt-owner binaries use explicitly audited older unchanged dependencies; current root integration remains separate.

The exact four files are imported, four ordinary tests registered and all8 affected artifact hashes refreshed. No public declaration, contract mode, original test or provider source changes. The separate native-pivot short-write candidate is unexecuted and remains open. Full V12 remains frozen with the old INFO adapter; its results cannot verify this correction.


## Current root integration completed

Current frozen tree5dd66163 six18/18 lanes pass (all-ASC Debug,Release,ASan bothABI),0skips; all53CPUCore/Dense/provider TUs rebuilt. Exact112/168 unique profiles per scalar, all actual native INFO0. Format4/coverage/publicsurface pass. Six historical strict records remain valid through byte-identical actual compiler-read TU/headers/generated ABI config. Foreign archives unsanitized. No full-suite or pivot short-write completion claim.

Frozen tree `5dd66163f10e34fdd72e19da35c79370575775cb`, archive SHA256 `46d0f26a92f04dddfae0d383d966854b9b1df2199ab8d15ed56a341ce7e1b018`, mapping `ae514d2343cd688ab8f3880e1dd7e8b0c2e72decd73555e44f835c42d7cd6451`. Audit `p04-lu-info-integration-01/completion-audit-01/audit.json` SHA256 `b5dcd01f46262406469cf9581db80ddfdcb638fb6281fa3e6e4d6304712ff39f` binds21 actual commands, all six compiled source closures and complete emitted mode sets. Existing V12 full runs are older INFO bytes and are never relabeled as these tests.
