# Stabilization and subset acceptance checkpoint

Status: first pushed checkpoint has 13 passing CI jobs plus passing CodeQL,
and six diagnosed platform failures. Their fixes pass affected local tests;
the final candidate08 matrix and fresh hosted verification remain pending. The owner’s
2026-09-09 stabilization assignment supersedes the queued PPSVX candidate19
expansion. Full P00–P11 scope and the 2,113 required routines remain unchanged.
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
