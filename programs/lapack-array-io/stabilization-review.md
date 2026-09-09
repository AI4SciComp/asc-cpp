# Stabilization and subset acceptance checkpoint

Status: **STABILIZATION_PASSED_SUBSET_ACCEPTANCE_PENDING** (2026-09-09).
Fresh PR and push CI each pass all 19 jobs; both CodeQL runs pass. The product
is synchronized at `52439959f261351da6e51e3131391ef1197da2e8`, product commit
`5f3c6d921374bab53a036fd6b5b4e4f7ce9591ed`, tested PR merge
`bf99463e6d7edbaff70f4e81be87065dd7668ae7`. Head and merge have the exact same
tree `8c4a17111e0a4f9d09c22e8df4d77af802baf571`; base `develop` remains
`46412183b2ae86101b2361c52376a8db8efff264`. PR #47 remains OPEN DRAFT.
The final acceptance records are a local documentation-only follow-up, with
its actual commit and patch recorded in external `final-handoff-01.json`.
No production, test, contract, workflow or provider change is unsynchronized.

The authoritative external index is
`stabilization-20260909-01/acceptance-checkpoint-final-01/manifest.json`.
`hosted-final-checkpoint-01` binds all 19 PR job logs, both CodeQL logs, both
push Windows logs, commands, toolchains, checkout identities and test totals.
Every total agrees with individual passing-test lines; there are zero unexpected
CTest skips. Successful jobs' skipped failure-artifact uploads are not tests.
The optional third-run failure ZIP was only partially downloaded; both complete
failed Windows job logs are retained, and that ZIP grants no evidence credit.

| Current dimension | Evidence-backed status |
| --- | --- |
| Integrated product | Recovered V27 plus four stabilization repair slices; candidate15 matches all 1,131 non-program files |
| Hosted CI | PR 34341838137 and push 34341831100: 19/19 jobs each; CodeQL 34341838133 and 34341831174 pass; current failed jobs: none |
| Windows MSVC 19.44 | Static and shared, Debug and Release: 227/227 each; the 40 Sparse resource-observation assertions now pass with exact resource-attempt and rollback checks retained |
| macOS AppleClang 17 | Debug/Release, static/shared: 289/289 each; portable decimal and explicit thread lifetime fixes execute |
| Linux full runtime | GNU 11 Debug/Release: 290 each; GCC 14 static 290/shared 292; Clang 18 Debug/Release static 289/shared 291 |
| Other hosted gates | ASan+UBSan 244, LSan 244, TSan 4, package/contracts/examples 72; format/tidy and strict Doxygen pass within their actual configured scopes |
| Local executed evidence | C14 full GNU 11 Debug shared 291, GCC 14 Release static 290/shared 292; C15 Sparse I/O four affected 1/1 replays and strict analysis pass; exact unchanged-source bridges retain the other results |
| Preserved work | Original dirty checkout and all 15 dirty siblings still match recovered heads, diffs and untracked hashes; no sibling import; PPSVX candidate18 retained externally |

All historical failed jobs are diagnosed in the sequence below. The final
Windows failure was a test-harness visibility error: Core DLL resource allocations
are not observed by the executable's replacement `operator new`. Candidate15
uses the existing resource-visibility helper at two expectations and adds exact
resource-attempt assertions. Release exactness where observable, long diagnostics,
rollback and leak checks remain. Earlier repaired causes include the named pivot
view lifetime, macOS integer/parser/thread portability, Windows export/section/
newline issues, and installed example language discovery. No warning, failing
mathematical assertion, test or global check was removed to obtain these passes.

| Capability | Implemented/callable and tested subset | Fully verified / still open |
| --- | --- | --- |
| P02 Dense/Sparse printing | Public bounded printers; real/complex, rank0/empty/higher ranks, layouts/subviews, visible truncation and sink failures. All mapped print tests pass Linux/macOS/Windows | Whole precision/rank/layout/metadata/read-count crossproducts remain open; previews are not reloadable |
| P03 text/binary | Public Dense and COO/CSR/CSC persistence, independent wire fixtures, all 12 scalar codes, structural/value checks, progress, size/overflow and transaction tests execute across hosted platforms | Complete scalar/storage/layout/failure crossproducts remain open; no implicit conversion, padding serialization, device copy or densification |
| P03 file cleanup | Installed Linux GCC14 static:60 scalar/storage configurations, 2,760 assertions, 60/60 CTest; checked close/flush, first-error/secondary-cleanup and unpublished-owner release | Nonempty rank2 Dense left/right and COO/CSR/CSC only; other ranks/empty/strided modes, linkage/platforms and reusable repository integration remain open |
| P10 Matrix Market | Existing representation/order/symmetry/pattern/duplicate/malformed tests pass across hosted platforms; independent interop and installed consumers pass | Complete required crossproducts remain open. New checked-close evidence is f64 general Dense array and COO coordinate only, 24 assertions/1 CTest each on Linux static |
| Native LU/Cholesky/Householder QR | 20 callable routines: 8 GETRF/GETRS, 8 POTRF/POTRS, 4 GEQRF. Reconstruction, residuals, multiple RHS, supported N/T/C, numerical failures, zero sizes, ownership/workspace/layout and concurrency subsets execute | 0 fully verified; all 20 implemented_unverified. Forty partial slots remain: 32 valid same-call POTRS factor/report aliases and 8 QR report/tau/workspace aliases |
| Optional Reference provider | 350 executable partial registrations; actual pinned LP64 and true ILP64 foundation 10/10 each, bound to unchanged production and real archive/ABI identities | Strict complete-implementation callable count 0 and verified 0; this does not mean zero executable routes |
| Full Reference scope | 2,113 required routines retained; 1,763 not started and 350 in progress | All 2,113 full contracts incomplete; 130 required XBLAS definitions absent; 78 historical required mathematical failures retained separately |
| Expanded quality/review | Hosted BUILD_TESTING=OFF tidy passes; owner packet assembled | 18 manually audited legacy test/benchmark lint findings reproduced on the isolated base remain failed; concrete redistribution approval pending |

`subset-acceptance-04/checkpoint.json` maps 63 local API/test/configuration rows.
The final hosted subset map binds the same 21 tests to each full Linux/macOS/
Windows job, ASan/UBSan and LSan; Windows executes them in both configurations.
TSan executes its selected native concurrency test. These are test-subset passes,
not completed routine/mode contracts. Random/BLAS compatibility regressions are
included in the full provider-free runs. Provider-free CI does not exercise the
optional Reference facet. Historical V27 LP64/ILP64 full 861/939 results retain
their exact frozen inputs and 78 failures; they are not current passing evidence.

Executed installed-consumer command:

```sh
python3 -B /home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/installed-consumers-04.py
```

This completed harness has exclusive output paths; use a fresh suffix for a
replay. It relocates the actual C10 GNU11 Debug shared installation and compiles
copied public examples linked only through exported ASC targets. Production is
byte-identical through C14/C15. Dense prints A/B, factors once, solves two RHS
twice, and independently obtains scaled residuals `0.00000e+00` for both solves.
Save/read compares exactly 124 binary bytes. A malformed read consumes 95 source
bytes while leaving X unchanged. Sparse independently passes exact COO/CSR/CSC
text/binary reloads, stored negative zero and corruption rollback. Compiler
dependencies exclude source/private headers; Sparse links no Dense, and neither
native consumer links LAPACK/Fortran. Dense and Sparse CTest each pass 1/1.

File helpers require explicit overwrite intent, truncate, flush successful
writes, and check close. They preserve a primary error and report secondary
cleanup failure; a close-only load failure discards staged ownership. Destination
rollback does not undo source consumption. No atomic replacement, fsync durability
or race-free path replacement is claimed; a failed save can leave a partial file.

The top-level state coverage SHA was stale V27 metadata. The current hash is
`21116d3357fdc030224d9390d4c5da87a275d228fc2f5c7dae245f0ba79e4290`, already
shared by candidate08, candidate15 and owner packet01. The semantic comparison
finds only 16 repeated artifact hashes for two amended tests; no routine, mode,
status, denominator or provider changed. Fresh candidate15 incremental validation
exits 0; strict full validation exits 1 at `lapack.cbbcsd: full profile incomplete`.
The original stale hash and both executed outcomes are retained.

Owner decision remains: approve or amend the exact
[redistribution packet](redistribution-review-packet.md), covering the pinned
3.12.1 source-derived metadata/declarations and proposed notice under the external
provider model. Both full license texts and all material hashes remain matched;
no notice, upstream patch, binary bundling or XBLAS import is approved here.

One next bounded implementation task: repair the 18 baseline-reproduced legacy
BLAS test/benchmark lint findings, preserving negative cases and numerical
assertions. The exact configuration command and diagnostic inventory are in
“Next bounded quality task after hosted stabilization” below and `state.json`.
No new numerical family or sibling import precedes that quality slice.

## Historical slice records

Everything below records observations at its stated slice. Earlier “pending,”
“next,” “current” and local/remote identities are historical; the checkpoint
above supersedes them without erasing their failures or evidence boundaries.

Status: candidate15/product5f3c6d9 repairs the last C14 hosted failure with
four passing affected tests and passing format/strict checks. Fresh hosted
verification is next. C14 has18passing/1failing CI jobs and passingCodeQL;
its sole Windows shared Release failure is diagnosed below. The three C14
full local runs and unchanged production/provider evidence retain their
precise scopes. Full2,113Reference and P00–P11 acceptance remain incomplete.
No merge, release, tag, upstream patch or redistribution approval is implied.

## Recovered identities and preserved work

Fresh read-only GitHub queries on 2026-09-09 confirm draft #47 → `develop`,
head `b1b78d789fb8f5366f13f86c411832df842b2755`, base
`46412183b2ae86101b2361c52376a8db8efff264`, tested PR merge
`ff074864eaf5aea998fc501c533fb4dd77234efd`. Main and develop share that base.
The integration worktree is `../asc-cpp-lapack-array-io`, branch
`feature/lapack-array-io`, recovered clean HEAD
`9bc62dbb5064260207166e61f36ea33aeb4c2f87`, 39 local commits beyond the remote.
The latest product commit is `87cd6460206cfa3e2ea384adc8fbcdfcefb8e3e3`;
9bc62db records external PPSVX candidate18 evidence, not an integrated PPSVX route.

| Dimension | Recovered observation |
| --- | --- |
| Remote integrated | V7: 162 partial reference routes; 20 native operations; printing, native persistence and Matrix Market implementations |
| Newer integration | V27: 350 partial reference routes; 20 native operations; later printing, binary-progress and native acceptance additions; clean before this stabilization change |
| Other worktrees | All 15 sibling worktrees accessible and dirty; original release checkout separately retains 320 status entries; no reset, stash, import or deletion |
| Existing executed evidence | V27 Release LP64/true ILP64 each 861/939, 78 required mathematical failures; selected Debug each 111/127; ASC-only sanitizer each 48/64, 16 mathematical failures; zero skips. These are historical frozen-source results, not passes for the new candidate |
| Unimplemented | 1,763 required reference rows not started; 130 required upstream XBLAS-dependent definitions absent from prepared providers; full P00–P11 closure open |
| Inadequately verified | All 350 partial reference rows and native20 lack full routine/mode/platform closure; 2,113 reference contracts incomplete |
| External work | PPSVX candidate18: four unregistered routes, scoped behavioral/memory evidence and retained mathematical failures; deferred intact |
| Blockers | Concrete redistribution review pending; Windows/macOS execution and hosted fixed-candidate checks pending; required numerical and optional-upstream gates remain visible |

Raw recovery and all failed-job logs are outside source under
`../asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/`.
`worktrees.json` binds each head, branch, status and dirty diff hash;
`sibling-content-inventory.json` hashes each changed/untracked sibling file and
compares it with integration. Dirty files are not automatically pending imports:
61/74 band-expert, 20/34 indefinite-expert, 6/11 SVD-least-squares and 3/13
Sylvester changed paths already match integration exactly. Remaining differing
paths require their retained handoff/source review, not blind cherry-picking.
P07 SVD’s nine changed paths remain outside integration. Candidate18’s external
freeze and earlier failed attempts remain untouched.

D005 maps the runbook’s suggested `docs/development/lapack-array-io/` to
`programs/lapack-array-io/`. Both original and durable runbooks hash to
`51c1cdf7e864bb38b849d7c5280fe34e92ff278da2653e6ee3c97c1cfb2923ae`.
Fresh live Google C++/Python guide captures, timestamps and hashes are in
`sources.json`; D003’s C++20/API compatibility exceptions remain in force.
The provider lock and pinned source identity remain unchanged.

## Original V7 hosted failures and repairs

CI run 34145084831 has nine failed and ten successful jobs. CodeQL run
34145084827 also fails compilation. All ten failed-job raw logs were read;
these are failures of the remote V7 merge, not evidence that local V27 passed.

| Jobs | First cause and repair in progress |
| --- | --- |
| GCC14 static/shared, package step, CodeQL | `lapack_lu_test.cc:437` dangling-reference warning. `Pivots::raw()` returns a descriptor by value, `values()` returns a span by value into the live Pivots owner. Retain the descriptor locally; add a range-outlives-descriptor regression. No actual expired element storage was found |
| All four macOS jobs | Three Dense I/O `MultiplySize` calls deduce conflicting `uint64_t` and `size_t` types. Select the existing checked uint64 wire-count domain explicitly |
| Windows static | Unsigned negation in Matrix Market; unreachable instantiated branches in ScalarName/complex Clone; COFF section limit in Sparse shape tests. Separate constexpr branches, return checked unsigned zero directly, apply /bigobj only to the diagnosed test target |
| Windows shared | C4251 on internal I/O classes containing inline Status/Result storage. Export out-of-line method boundaries instead of entire stateful classes; retain C++20 object lifetimes and existing accessor semantics |
| Windows test harness | Git CRLF conversion changes exact wire fixtures and content-addressed provider/source records; evidence child print emits platform newlines. Pin text checkout to LF and make the byte-exact test child write explicit bytes |
| Windows workflow | Compilation failure did not stop subsequent CTest/install attempts, causing missing-executable cascades. Check each native exit immediately. Bash/pwsh configure/build/test output is now logged and uploaded even before CTest exists |

The package job’s independent strict Doxygen step succeeded; its package step
stopped on the same GCC compilation diagnostic before package tests. No package
linkage or ABI defect is inferred from that log. Local package/ABI verification
must still execute after the fixes. Original failure logs are retained.

## Capability semantics and acceptance still required

`callable_reference=0` is the strict ledger count of routes with complete
implementation state, not a count of executable overloads. The 350 registered
partial routes are executable but remain `in_progress`; relabeling them would
hide incomplete contracts. Native20 are callable and implemented-unverified.
No row is fully verified. These dimensions are not additive percentages.

Existing printing, Dense/Sparse text/binary and Matrix Market tests and installed
examples will be reused. Fresh candidate testing must cover changed Core codec
headers, complex owner lifecycle, native LU/Cholesky/QR and Random compatibility,
then full provider-free/package configurations. Independent fixture, transaction,
resource/progress, scalar/storage and platform gaps remain open until linked to
executed evidence. Native POTRS alias and QR report/tau/workspace acceptance gaps
recorded by earlier scoped reviews are not closed by an ordinary test pass.

Next bounded task: finish R1 portability repairs and execute the affected
candidate tests, then the full relevant provider-free/package matrix. PPSVX
candidate19 remains deferred until this stabilization checkpoint is settled.

## Executed repair slices (current work remains in progress)

The external `checkpoint-slice-01.json` indexes actual command records, hashes,
source identities and test totals. C01’s full GNU11 shared Release run executes
290 tests: 288 pass and two public-header inventory checks fail. C02 fixes the
reviewed eight-header inventory; its strict Doxygen checks pass 126/126 headers,
2,204 public members, zero warnings. C03’s full GNU11 shared Debug run executes
290 tests: 289 pass and the old ELF baseline fails. Neither failed run is called
a complete pass. No required test is skipped.

GCC14.3 xPack was prepared outside source (archive SHA256
`93bc0add266a50626861d571cce92667b8d42e2770f3f321b3c8df096cee5bf1`).
The exact original LU test fails GCC14 compilation; the named-view amendment
passes. C02 then exposes rank-zero Sparse metadata copies; a minimal reproducer
also fails at the unchanged PR base in an isolated detached worktree.
C03 fixes zero-rank copies but exposes GCC14’s rank-one iterator-range bound
diagnostic. C04 uses the fixed rank as an explicit copy count and retains a
named extent span. Existing empty/rank-one/data assertions remain unchanged.
Its affected build succeeds. Initial CTest attempts fail loading the host’s
older libstdc++; a fresh attempt with the matching xPack runtime passes 11/11.
The runtime path and binary hashes are explicit in `gcc14-runtime-identity.json`;
no global runtime or system configuration was changed.

C03 Clang19 ASan/UBSan passes the selected eleven native/I/O tests. C04’s
count-explicit Sparse change still needs fresh sanitizer evidence; earlier
Sparse sanitizer results are not automatically promoted. Both C04 optional
provider builds rehash their original prepared prefixes and execute ten LU/
layout/normal-return/real-ABI tests each, all passing. This is scoped ASC wrapper
and ABI evidence, not an upstream-suite or full-provider completion claim.

Fresh public-only installed consumers pass 1/1 each using a relocated GNU11
shared producer. C02 differs from that C01 producer only in the reviewed header
hash file and the two copied example sources; the bridge is checked explicitly.
Dense reads and prints A/B, factors once, solves two RHS twice, and independently
reports residuals `0.00000e+00` for both solves. It saves and reloads exactly 124
bytes, compares value bits, rejects malformed input after 95 consumed bytes while
preserving X, and separately checks singular-factor rejection. Sparse preserves
exact COO/CSR/CSC text/binary structure and value bits, including stored negative
zero, and rejects a corrupt CRC without destination mutation. Compiler dependency
files exclude source/private headers; runtime inspection finds no Dense in the
Sparse consumer and no LAPACK/Fortran dependency in either native consumer.
These C02 consumer records remain distinct from the final Sparse copy amendment.

## Reviewed ABI delta

A fresh GNU11 Debug shared build of base `46412183` reproduces the committed
old ELF baseline exactly. The candidate adds 149 dynamic symbols and removes
zero: Core 39 (codec/printing/Matrix Market helpers and five charconv instantiations),
Dense 72 (native foundation/algorithms, array/Matrix Market readers and standard
library support), Sparse 38 (reader/format support and charconv instantiations).
Random and Utilities symbol sets remain identical. All old symbols, SONAMEs and
direct dependencies remain present and unchanged. Dense/Sparse OS-ABI markers
change from System V to GNU alongside newly emitted GNU-unique standard-library
objects; the full readelf/nm observations are retained. Internal template support
symbols do not become supported public API by appearing in this inventory.

`InspectElfAbi.cmake` generated both actual reports. `abi-semantic-delta.json`
retains every added/removed report row. After reviewing this additive feature
surface, the GNU11 Debug baseline records candidate digest
`1afd6c7ceb94b82b9d68e7ad6a4773c7c89d89b571c393206dc0f57228b7870b` and
retains the prior digest as history. The exact inspector replay passes; C05’s
fresh full Debug shared matrix is the closing candidate gate. Other toolchain
baselines are not silently rewritten or claimed verified.


## Closing local evidence and acceptance boundary

C05 GNU11 Debug shared passes **290/290**, zero failures/skips, including
source/installed public surfaces, package/relocation/exported-component checks,
Random/BLAS regressions and the reviewed ELF baseline. C05 Clang19 ASan+UBSan
passes 22 selected tests. The first sanitizer selector omitted the separately
named Dense Matrix Market interop tests; no omitted test receives credit.
C06 explicitly runs Dense interop write/read and Sparse Matrix Market fuzz:
3/3 pass, including 12,294 mutated Sparse transactions. The22 plus3 invocations
cover 24 distinct tests, with Sparse fuzz repeated after its harness amendment.

C04's GCC14 full build additionally diagnosed the Sparse Matrix Market fuzz
insertion range. C06 checks that each nonempty seed leaves insertion capacity
before copying; all 4,096 mutations per COO/CSR/CSC instance and all assertions
remain. The direct GCC14 compile now passes. C06's full GCC14 static/shared
lanes are in progress; their affected suites each require 11 actual tests.
Fresh strict clang-tidy18 passes 11 inspected codec/native/Sparse TUs, and
clang-format18 passes all modified C++ files. C06 strict Doxygen again passes
126/126 headers,2,204 public members, zero warnings; link checks pass.

`checkpoint-slice-02/records.json` binds 78 completed command records at this
slice, including failures. Its copies of LastTest.log preserve full native
mode output beyond CTest's default JUnit success-output truncation. Final
full-lane records must be added separately; this index is immutable.

| Capability | Implementation and executed subset | Remaining acceptance |
| --- | --- | --- |
| P02 Dense/Sparse display | `PrintArray`, real/complex literals, rank0/empty/vector/matrix/higher-rank layouts, subviews, visible budgets and sink failures; Dense 48/Sparse 144 precision/notation/locale fixtures, Sparse coordinate/value output; GNU11 Debug shared and ASC sanitizer | Full precision/rank/layout/metadata/read-count crossproducts and required Windows/macOS evidence; preview is not serialization |
| P03 ASC text/binary | `Read/Write/Save/LoadDenseArray*` and Sparse equivalents; independent literal text/i16-binary/COO fixtures plus independent codec interop, all supported scalar codes, COO/CSR/CSC shapes/stored zeros, corruption/truncation, size/overflow, staging/resource failures and host admission | Complete scalar/storage/layout/failure-boundary crossproduct; deterministic close/cleanup failures and required platforms remain open |
| P03 destination transaction | Existing `Read*Into` cases compare complete destination/padding on rejection. Binary i16 regression executes 75 reads / 145 writes, including short/zero/error progress and EOF-before-commit | Source consumption is retained; no source or sink rollback claim. New-owner failures release acquired storage; resource lifetime still belongs to caller |
| P10 Matrix Market | Native Dense array ordering, Sparse coordinate one-based translation, valid field/symmetry classes, lower-triangle/Hermitian/skew rules, explicit pattern-unit policy, stable checked duplicate sums, malformed/overflow paths and independent interop | Full required mode/platform evidence still open. Optional cross-representation conversion is not silently performed |
| Native20 | GETRF/GETRS 8, POTRF/POTRS 8, GEQRF 4; reconstruction, multiple RHS, N/T/C where supported, singular/non-PD/rank-deficient and zero-size cases, layout/workspace/ownership checks. Existing additive 48/32/8 scalar modes, 112 alias modes and 20,000 concurrent calls execute | 40 partial acceptance slots: 32 POTRS valid same-call factor/report aliases, 8 GEQRF report/tau/workspace aliases; Windows/macOS and race-detector coverage open. All 20 stay implemented_unverified |
| Optional provider foundation | Actual pinned LP64 and true ILP64 providers each 10/10 LU/layout/normal-return/ABI tests; prefix/archive identities rehashed | Scoped evidence only; 350 executable partial routes, zero complete-reference callable/verified rows; historical 78 required math failures and 130 absent XBLAS routines remain open |

The exact test/API/contract inputs are preserved in each snapshot and command
record. Existing partial overlays remain in force; no evidence ID, mode status,
required routine or tolerance is promoted by this table.

### File convenience guarantees

The existing Core File path helpers request explicit `kTruncate` and open `wb`.
They preserve the first write/flush error, always attempt close, and record a
secondary close failure in `cleanup_error`; a close error becomes the result
when earlier operations succeeded. Load helpers validate complete content
before returning an owner, attempt close and discard an otherwise successful
owner if close fails. Existing destination-view reads use explicit ByteSource
staging, not a path-level atomic replacement.

Executed path tests cover successful text/binary files, missing paths, invalid
overwrite intent, trailing input, and a real Linux `/dev/full` flush failure.
A deterministic independent close-only failure remains missing evidence.
Successful save does not promise fsync durability, atomic replacement or
cross-process exclusion. A failing save may leave a truncated/partial file.
Binary preserves supported IEEE quiet-NaN/signed-zero component bits; text and
Matrix Market retain their distinct frozen nonfinite/conversion policies.
No implicit device copy, padding serialization, Sparse densification or native
wire scalar conversion was introduced.

### Final installed-only workflow

`python3 -B ../asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/installed-consumers-02.py`
executed successfully in a new external directory. This archival harness uses
exclusive output paths: inspect its records or choose a new suffix for a replay.
It installs the C05 Debug shared producer, relocates the prefix, and builds
copied C06 examples using only exported installed targets. The checked C05→C06
bridge contains one test-only fuzz capacity guard; library/header/example
bytes are identical. Both CTest consumers pass 1/1, zero skips.

Dense prints A/B, factors once, solves two RHS twice, independently reports
scaled residuals 0.00000e+00 and 0.00000e+00, and verifies an exact 124-byte binary
save/reload. Malformed staged input consumes 95 bytes while preserving X.
Sparse verifies exact COO/CSR/CSC text/binary archives including stored negative
zero and corruption rollback. Compiler dependencies exclude both checkouts
and private headers; ldd excludes Dense from Sparse and excludes foreign
LAPACK/Fortran from both. These checks prove native installed workflows;
provider-enabled consumer coverage is a separate dimension.

### Preserved remote authorization

The retained 2026-09-07T05:06:02.752Z session explicitly permits pushing this
program's feature branches and opening/updating draft PRs when tools allow.
`remote-write-authorization.json` records the exact paragraph and message hash.
The current resume preserves it. Local gates must finish before the candidate
is pushed; draft status and pending redistribution approval remain unchanged.
No release, merge, tag, provider import approval or permission change follows.


## Local checkpoint before hosted verification

Product commit `6c72b638b46f159e41db2f26346d6caa8ecc9788` passes the available
local gates: GNU11 Debug shared 290/290, GCC14 Release static 289/289 and GCC14
Release shared 291/291, zero skips. C06 differs from this commit only in the
CI documentation-build log name and install-log capture; all 1,127 production,
header, test, build, contract and example inputs are checked by the bridge.
The final 18 workflow shell scripts parse successfully. No Windows/macOS
execution credit follows from shell parsing or Linux compilation.

The first GCC14 shared full run records 288/291: three nested Core/package
consumers selected the host GCC11 compiler and could not link GCC14's
`GLIBCXX_3.4.31` string helper. `gcc14-shared-replay-01` archives the complete
original test workspaces and Testing logs, then uses explicit CC/CXX plus the
matching runtime, as the hosted GCC14 job already does. Affected tests 5/5 and
the full 291/291 pass; closing CMake caches confirm the actual GCC14 compiler.
No product code, producer binary or package test was changed for this replay.

`local-checkpoint-01/manifest.json` binds the product commit/tree, snapshot
bridge, all completed command records and raw artifact hashes, strict checks,
workflow parsing, and preserved worktrees. The retained prior session's final
handoff at 2026-09-09T01:19:35.214Z agrees with recovered 9bc62db/candidate18;
its PPSVX next task is explicitly deferred by the current assignment.

The strict full-profile validator was executed and still exits 1 at required
`lapack.cbbcsd: full profile incomplete`. Incremental identity/honesty validation
passes: 2113 required, 350 in-progress reference routes, 1763 not started,
20 native callable/implemented-unverified, zero verified native/reference.
These separate results are both retained; the full-profile failure is not a
passing test or an exemption. Existing required mathematical failures retain
their original source identities and interpretations; this codec/CI repair
introduces no provider patch, rescaling, oracle/tolerance or denominator change.

All 16 original/sibling worktrees retain their initial heads, statuses, tracked
diffs and untracked hashes. The one detached base-reproduction worktree also
remains available. No sibling slice was imported during stabilization.


## Fresh hosted checkpoint and decimal portability slice

Authorized push head `254db534ce520c2e1a24af4eba1996736b59eb60` is tested through
PR merge `79cc79f52d20d33578a3e2ed6422dc4b79dcd9b5`, whose tree equals the head
(tree `f731c83e278b55a37e8af9dea01183d5c493f272`). Checkout lines in all successful
logs independently bind this merge. PR CI34322877900 completes with 13 passing
jobs and six failures; CodeQL34322877813 passes. Linux totals are GCC11 static
Debug/Release 289 each, GCC14 static289/shared291, Clang18 static288/shared290
for each configuration, ASan+UBSan243, LSan243 and TSan4; package selected72.
All report zero failures and zero unexpected test skips. The quality job passes.
Failure-artifact upload skipped after a successful job is not a skipped test.
These results belong to 254db534, before the decimal fallback.

All four macOS failures are the same source/library portability defect:
AppleClang17 with Xcode16.4 libc++ has no float/double `std::from_chars`, and
Core Matrix Market instantiation diagnoses the unavailable overload. This is
separate from the earlier fixed checked-size deduction error. The
[libc++ C++17 status](https://libcxx.llvm.org/Status/Cxx17.html) lists floating
`from_chars` only from LLVM20. Both Windows failures are the native alias test
fixture's explicit alignment padding (C4324 promoted to error), after the
previous compilation defects were fixed. All twelve push/PR failure logs were
read; one transient log-download EOF and its successful retry are preserved.
`hosted-failures-new-01/diagnostic-summary.json` binds each PR job and repair.

The new Core fallback is original code, with fixed stack integer storage and
exact decimal-ratio rounding. It is selected only when the standard library
lacks the needed overload. It does not import another library, change the
wire schemas, allocate a locale object, use ambient floating rounding, silently
narrow a supported scalar or alter the pinned provider. Independently generated
integer/Fraction fixtures cover both signs at subnormal, normal and overflow
boundaries under four floating rounding modes; long discarded tails distinguish
exact ties and values above/below them. Existing stream/resource/rollback tests
remain intact. The native fixture replaces forced alignment with explicit gap
bytes and retains separate live objects and every alias assertion.

C07 executes 26/26 affected tests in GNU11 Debug shared and 26/26 with Clang19
ASan+UBSan, zero skips. An external checksum-verified libc++14 package runs the
Core parser/Matrix Market helpers 2/2, exercising the actual unavailable-overload
branch. Its broad build first failed an external flag setup, then stopped at
preexisting Dense C++20 ranges unavailable in libc++14; neither failure is
hidden or used to weaken ASC's C++20 requirement. This is Core fallback evidence,
not macOS or full libc++ acceptance.

`candidate-07/abi-semantic-delta.json` reviews exactly two added internal Core
codec exports, no removals and no SONAME or direct dependency changes. The
actual InspectElfAbi report hashes to
`831e8d9ddbcfb214732fdc9297a1fd28ec3f26788210bbb4038b405400342a51`.
Candidate08 freezes the fixes, formatting, updated Core header digest and this
reviewed GNU11 Debug ABI observation. `decimal-fixture-replay-02/record.json`
proves the private generated fixture reproduces byte-for-byte. Full candidate08
GNU11 Debug shared and GCC14 Release static/shared gates are running; fresh
installed and provider-foundation checks and hosted macOS/MSVC execution remain
required. No native/reference row or whole I/O package is promoted by this slice.


Candidate08 strict checks pass: three clang-tidy TUs, changed C++ clang-format,
Doxygen 126/126 headers and 2,204 public members with zero warnings, plus links.
Its incremental ledger validator passes; strict full validation still exits 1
at `lapack.cbbcsd: full profile incomplete`, preserving the full denominator.
`candidate07-08-bridge.json` proves all C++ source/header/test bytes are identical;
only the two reviewed inventories and Fraction module-import spelling differ.
Thus the C07 26-test sanitizer and libc++14 Core results retain their precise
scope for this candidate. Actual helper-object references to both fallback
symbols are captured in `candidate-07/libcxx14-routing-03`.

The fresh installed command is
`python3 -B ../asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/installed-consumers-03.py`.
It executes against the same C08 GNU11 Debug shared producer and copied examples,
with no source bridge required. Dense and Sparse each pass 1/1, zero skips.
Both scaled Dense residuals are 0.00000e+00; binary saved/read bytes are 124/124;
malformed staged input consumes 95 bytes and leaves X unchanged. Sparse exact
COO/CSR/CSC text/binary archives pass. Compile dependencies exclude source/private
headers and runtime dependencies retain Sparse-only and provider-free isolation.
The complete command, exit, source and installed-file hashes are retained there.


All six actual hosted failure-artifact ZIPs from PR CI34322877900 were downloaded
and inspected. Each contains configure/build output and the original diagnostic
before CTest could execute; Windows uses `ci-build-Debug.log`, macOS uses
`ci-build.log`. `hosted-failures-new-01/artifact-downloads/record-02.json` binds
archive and log hashes. The first artifact-inspection script expected only the
macOS filename and failed; its observation is preserved, and the corrected
inspection checks the actual per-platform names and compiler diagnostics.
This closes failure-log capture for compilation failures without treating
unexecuted CTest as a successful or skipped required test.


## Final local inventory correction before the second push

Product `fbe21fe514910e86c05bad2c1aca6c954b838acf` is byte-identical to C08 across
all 1,131 non-program files (`candidate08-commit-bridge.json`). The full C08
results are GNU11 Debug shared289/291, GCC14 Release static288/290 and
shared290/292: exactly two failed audits in each, zero skipped tests. Every
other test passes, including the new decimal test, native/Random, I/O and
installed-package workflows. Both failures report that the approved compiled
source inventory omitted `src/core/array_parse.cc`. They are not numerical,
ABI, package-linkage or runtime failures.

C09 adds precisely that source to the existing explicit inventory in
`tests/architecture/check_public_file_policy.cmake`. The exact-set assertion,
header policy and all module/dependency checks remain unchanged. A fresh C09
configuration executes both actual CTest audits 2/2, zero skips. No compiled
source/header/executable-test or provider input changes between C08 and C09;
`candidate08-09-bridge.json` binds the one-line contract correction. Full C08
failures and untruncated LastTest logs are retained under
`candidate-08/retained-full-logs`. The source-independent inventories are fixed,
not skipped. This is combined local closing evidence, not a claim that a single
C09 full CTest invocation passed. Fresh hosted full C09 verification is next.


## Second hosted diagnostic slice

Second pushed head `0d623ba85017bc4b3fc377c46299fc59779e6145` addresses both
original platform defects and the source inventory omission. Fresh PR
CI34328080477 has twelve passing Linux/package/sanitizer jobs, five failed jobs
(four macOS plus quality), and two Windows jobs still running at the recorded
observation. All ten push/PR failed-job logs are retained/read in
`hosted-second-failures-01`; no Windows cause is inferred from these logs.

The quality error is the explicit `bit_width` result cast under GCC14 headers:
[LWG3656](https://cplusplus.github.io/LWG/issue3656) changes its result to int,
while GCC11 headers return the unsigned argument type. C10 uses the equivalent
`32 - countl_zero(word)` for the nonzero 32-bit word, whose result is consistently
int. No warning is disabled. macOS now compiles through the decimal reader and
stops in `native_concurrency_test.cc` because its libc++ lacks `jthread`.
C10 uses explicit `std::thread` joins, preserving four scalar workers and all
4,000 fixture groups/20,000 numerical calls. A fifth barrier participant keeps
startup coordinated; if thread creation fails, missing participants drop and
all started workers are released/joined before the test fails. This changes a
test harness, not ASC's public exception or numerical contracts.

C10 GNU11 Debug shared, GCC14 Debug shared and Clang19 ASan+UBSan each execute
3/3 affected parser/Matrix Market helper/concurrency tests, zero skips. Real
libc++14 Core executes2/2. Three strict checks (both GCC standard-library header
sets plus the thread test) pass. Fresh InspectElfAbi matches the existing
`831e8d9d...` baseline exactly. The relocated installed command is now
`python3 -B ../asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/installed-consumers-04.py`;
it passes Dense1/1 and Sparse1/1 with the same residual/124-byte/95-byte rollback
observations and installed-only dependency checks on the actual C10 producer.
Expanded native/I/O and refreshed real-provider gates remain in progress here;
no C10 hosted or full-profile acceptance is claimed.

`subset-acceptance-02/checkpoint-02.json` binds 63 executed C08 test/configuration
records, 240 LU numerical profiles, 248 QR numerical profiles and 112 alias modes
per compiler configuration to actual public declarations and source hashes.
The initial record used abbreviated QR labels and is explicitly superseded by
the corrected FormHouseholderQ/ApplyHouseholderQ labels. This is scoped C08/C09
evidence, not a fully verified native overlay or a pass for every C10 input.
All 40 native partial slots and the full Reference denominator remain required.

## Windows runtime and installed-example repair slice

C09 PR CI34328080477 completed with12passing/7failing jobs; CodeQL34328080791
passed. Head0d623ba was tested as merge9d5153ff into base46412183. All14
failed push/PR logs are preserved under hosted-second-failures-01. Windows
static Debug executes221/227 and shared Debug226/227, with zero skips; neither
job reaches Release. This supersedes the earlier observation that Windows was
still running. macOS4 and quality fail for the C10-fixed causes above.

Windows standalone examples compare ENABLED_LANGUAGES to literal CXX, although
Windows toolchain selection already enabled CXX;RC before package lookup.
Three examples now capture the language set before find_package and require
it to remain identical afterwards. Unrelated imported targets remain forbidden.
All three copied standalone examples configure/build/run1/1 using the relocated
C10 installed prefix in standalone-consumers-11; Windows still requires replay.

Static Debug’s four legacy BLAS tests and Dense benchmark require a zero global
allocation count despite MSVC Debug STL proxy allocations in public Status/Result
scaffolding. Their sources, production BLAS/Status and the existing observation
helper are byte-identical to base46412183 (msvc-debug-source-comparison.json).
The helper already documents this instrumentation allowance and preserves exact
counts in Release/non-MSVC builds. C12 applies that same unchanged policy to
these five consumers; mathematical assertions and reported benchmark counts are
retained. This is not a claim of allocation-free MSVC Debug behavior. Source
comparison is not an executed Windows baseline; available Windows tooling has
no installed MSVC C++ component, so hosted replay is the required platform gate.

C12 affected runtime tests pass5/5. C13’s added direct benchmark include failed
all three local builds because that target lacks the tests-root search path;
C14 uses the existing relative include convention. Original logs remain intact.
C14 also fixes directly-used/missing and unused includes in the touched legacy
files. Production/native/I/O/provider inputs are byte-identical to C10 through
candidate10-14-bridge.json, retaining scoped C10 native/I/O26/26, actual provider
10/10 perABI, sanitizer3/3, libc++14Core2/2, ABI and installed-consumer evidence.

An expanded manual clang-tidy invocation on the five legacy test/benchmark TUs
fails on pre-existing long functions, invalid-enum negative cases, deliberate
transpose index ordering and direct include debt. This is broader than the
hosted quality job, which configures BUILD_TESTING=OFF. It is recorded as failed,
not suppressed or called a pass. An isolated-base reproduction and current
replay distinguish unchanged findings from the direct-include corrections.
Full subset/code-quality acceptance remains open.

## Deterministic checked-close acceptance

The external command
`python3 -B ../asc-cpp-evidence/lapack-array-io/stabilization-20260909-01/run-file-close-faults-01.py`
executes a new fault harness against the actual C14 GCC14 Release static
installation, relocated before discovery. Dense-only and Sparse-only projects
link through their installed exported targets; compiler dependencies exclude
source/private headers, and Sparse imports/links no Dense target/library.
Both CTest invocations pass 1/1 with zero skips, each exercising 46 assertions.
`file-close-faults-run-01/identity-and-results.json` binds the retained harness,
installed files, commands and raw logs to C14/product6f5cd1c.

GNU link wrapping calls the real fclose/fflush, then deterministically returns
EIO/ENOSPC. Successful payloads followed by close failure reject owner publication
and release staged resource allocations before returning. Malformed input and
resource-allocation failures retain their original error with a secondary
cleanup error. Saves flush and close exactly once; flush error remains primary
when close also fails. Disarming faults restores successful saves. This tests
ASC's reported cleanup contract, not a real filesystem outage or durable write.

Scope is Linux, GCC14, static linkage, f64 rank-two Dense and Sparse COO, text
and binary. It closes that specific deterministic close-only gap. Other scalar,
rank, layout, CSR/CSC and platform cross-products remain open; integrating this
external harness into reusable repository tests also remains review work.
Existing helpers still require explicit truncate intent and promise neither
atomic replacement nor fsync-style durability. Original owners and the source
checkout are unmodified by the fault harness.

The expanded manual legacy lint comparison is now executed on the isolated
base, not inferred: `legacy-tidy-baseline-comparison-01.json` shows all 18
remaining candidate diagnostics also occur on base46412183. Missing/direct
includes are corrected; no warning suppression, threshold change or negative
assertion deletion was introduced. This audit remains failed and is an open
P11 quality obligation outside the hosted BUILD_TESTING=OFF tidy scope.

## Candidate14 full local checkpoint

Product6f5cd1c0a84be54b66823b3f0dff7ada1fa39a6a matches all 1,131 non-program
files in candidate14. Full GNU11 Debug shared passes291/291; GCC14 Release
static passes290/290 and shared passes292/292. Every run has zero failures and
zero skips. Each also passes18/18 affected tests before the full run. The full
runs cover module isolation, headers, inventories, package/relocation, installed
examples, native LU/Cholesky/QR, array I/O, BLAS and Random compatibility.

`subset-acceptance-03/checkpoint.json` binds63 current executed API/test/configuration
mappings, exact source hashes, emitted numerical/alias records and immutable full
LastTest logs. C10 unchanged-source provider10/10 per real ABI and sanitizer/ABI
results retain their scope. No full routine/mode/platform status is promoted:
native20 remain implemented_unverified, 40 native class slots remain partial,
350 executable partial reference routes remain in_progress, and all2,113
reference contracts remain incomplete.

Cleanup harness02 repeats both installed46-assertion tests successfully and
passes strict lint for both configurations. Original harness01 runtime passes
and strict failures are retained: unused include, missing nodiscard and GNU
reserved linker identifiers. Harness02 fixes those with direct includes,
nodiscard and assembly symbol labels; no diagnostic is disabled. The separate
60-configuration scalar/storage cleanup matrix is in progress and receives no
pass credit yet. Fresh hosted candidate verification is the next R1 action.

## Third pushed checkpoint and wider cleanup evidence

Authorized non-force push succeeded at b223ada16bd499f2ee8b6dc04e1bfab436da0e90
(product6f5cd1c); draft #47 still targets develop46412183. New merge candidate is
b1e1bf9479940f1cabd5b8db5f076c4c649b884e. PR CI34333946235 and CodeQL34333946309
are queued/running; push runs34333940704/34333940805 are separate. Fresh platform
results remain required. The first evidence-launch attempt resolved a relative
manifest against the wrong working directory and failed before git push;
absolute external paths fixed the launcher. No source-tree artifact was created.

The cleanup matrix now completes all60 configurations and2,760 assertions:
all12 exact wire scalar encodings, nonempty rank-two Dense left/right and Sparse
COO/CSR/CSC, each exercising text and binary save/load failure boundaries.
All60 CTest invocations pass1/1, zero failures/skips. Every consumer uses only
installed public targets/headers; all Sparse links exclude Dense. The same
real fclose/fflush wrappers exercise close-only failure, primary parse/allocation/
flush errors, secondary cleanup reporting, one-close lifetime and staged-owner
release. `file-close-matrix-run-03/acceptance.json` binds every executed record,
fixture matrix, generator, source identity and link evidence.

These are independently constructed text seed fixtures; binary seed archives
in this cleanup matrix are writer-produced. Independent binary-schema acceptance
remains separately supported by the repository fixture tests. This scoped Linux
GCC14 static result does not grant other ranks, empty/strided layouts, OS/linkage
or whole P02/P03/P10 acceptance. Reusable repository integration remains open.

The same installed cleanup experiment now covers f64 general Matrix Market
Dense array and Sparse COO coordinate helpers: each passes 24 assertions,
CTest 1/1, zero skips, and strict lint. See `file-close-mm-run-05/acceptance.json`.
The original MM harness04 runtime pass and include-cleaner failures remain
preserved; harness05 removes unused includes without changing assertions.
This does not claim symmetry/Hermitian/skew/pattern or complete scalar/storage
cleanup acceptance for Matrix Market.

`owner-packet-recheck-02.json` rehashes every proposed ASC material and both
upstream license texts. All still match the exact pending packet. The notice
is unapplied and no owner approval is recorded. The provider/source locks,
full coverage denominator and proposed distribution model remain unchanged.

The GitHub CLI refused completed-job logs while the overall workflow was still
running. Those retrieval errors are retained in hosted-third-logs-01. The
completed-job API endpoint succeeds; hosted-third-logs-02 retains actual PR
logs for Linux and macOS, with one transient EOF retried separately. CTest's
macOS summary omits the explicit zero-failure phrase; normalized-summary-03
parses both forms and cross-checks the actual passed-test lines. Earlier
normalizations are preserved and superseded, not mistaken for missing tests.
The repository evidence runner already reads JUnit and needs no change.

## Next bounded quality task after hosted stabilization

Repair the 18 independently reproduced legacy test/benchmark clang-tidy
findings in the five already-touched BLAS consumers. Six are deliberate
fixed-underlying-enum negative cases, four are intentional transposed-index
oracle calls, and eight are oversized fixture functions. Preserve every
numerical/negative assertion and tolerance. Use the repository's existing
narrow, explained treatment of false positives where applicable; split the
long fixtures into named checks without changing their operation sequence.
Do not alter the global check list, thresholds, warnings-as-errors or the
allocation-observation policy. The exact baseline/current diagnostics are in
`legacy-tidy-baseline-comparison-01.json`. No new numerical family or sibling
import is part of this task.

Configure its working-tree test context outside source with:

```sh
cmake -S /home/yicai/AI4SciComp/asc-cpp-lapack-array-io \
  -B /home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/stabilization-next-legacy-tidy-01 \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-11 -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_LAPACK=OFF -DASC_CPP_ENABLE_CUDA=OFF \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE=/home/yicai/AI4SciComp/asc-cpp-evidence/lapack-array-io/dependencies/asccmake-8a7dcbad/source
```

Apply the bounded fixture edits before rerunning the five affected lint/runtime
checks. Existing frozen candidate14 evidence must remain unchanged. The 40
partial native slots, remaining array-I/O cross-products and complete Reference
program continue to be required after this quality slice.

## Fourth repair: Sparse I/O allocation observation across a DLL

Third PR/push CI both finish18pass/1fail; both CodeQL runs pass. Windows static
passes227/227 in both Debug and Release. Shared Debug passes227/227; shared
Release passes226/227. Its only failed test is sparse.io_test:22 assertions at
line384 and18 at1141 expect all successful resource requests to appear in the
executable-global allocation probe. All other assertions pass. Both failed job
logs and both static success logs are in hosted-third-windows-04.

IoResource delegates successful requests to HostMemoryResource::Allocate in
Core. Under Windows DLL linkage, the executable's replacement operator new
cannot interpose that allocation. The existing observation helper already maps
resource counts accordingly, and Sparse Matrix Market tests already use it.
This is a program-introduced test-harness error; the base has no sparse/io_test.cc.
C15 uses ProcessVisibleResourceAllocationCount(failure-1) at exactly those two
sites and adds exact resource.allocation_attempts()==failure assertions. Long
message/provider/native-code preservation, rollback, no leaked resource and
all numerical/format checks remain. The helper and its platform policy are
unchanged. Shared Release still requires an exact zero executable-probe count.

Only this test differs from C14 (`candidate14-15-bridge.json`); all1,130 other
non-program files match, including every production/native/provider/header and
package input. GNU11/GCC14 static/shared and sanitizer affected replays are in
progress. Fresh hosted verification remains required before stabilization passes.

Candidate15 affected verification is complete: GNU11 Debug shared, GCC14
Release static/shared, and Clang19 ASan+UBSan each pass the full Sparse I/O
executable1/1, zero skips. Formatting and the complete Sparse I/O TU clang-tidy
check pass. Product5f3c6d921374bab53a036fd6b5b4e4f7ce9591ed matches all1,131
frozen candidate15 files. No other production or test input changed; the next
action is the authorized feature push and fresh hosted matrix.
