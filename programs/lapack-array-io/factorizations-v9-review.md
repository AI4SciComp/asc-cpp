# V9 indefinite expert and SVD least-squares integration

Status: 32 additional partial reference routes are registered; combined
verification is pending. No full P05/P06 or reference capability is claimed.
The denominator stays 2113 required rows, with 198 in progress and 1915 not
started. Native coverage stays separate at 20 implemented-unverified rows.

## Recovery and exact imports

Integration is `feature/lapack-array-io` at local commit
`455c235f8a0c20e4994a035a6b64b81e9950a453`, in the preserved sibling worktree.
The dirty original release checkout is untouched. Runbook section 2.5
explicitly assigns bounded parallel implementation/review when available.
Root owns registration, manifests, integration and installed verification.
No remote writes, source pin changes or provenance approvals were made.

All external locators are relative to
`../asc-cpp-evidence/lapack-array-io`. The exclusive import audit is
`resume-20260908-01/import-v9-files.json`: every target was absent and every
source hash matched the preserved handoff before import. No original owner
source or frozen archive was replaced.

The indefinite owner handoff is `p05-indefinite-expert-02/handoff-files.json`,
SHA256 `2bf4c0f3665ceff3638d0a28989ec1c6d923c03bb20409f439ccc20300b99045`.
Independent review read all 19 code/test files, pinned sources and the
executed 192-test evidence. It found two acceptance-test gaps and no concrete
production defect. Additive patch
`p05-indefinite-expert-review-tests-inioge3l/test-acceptance.patch`, SHA256
`f526a9d188f27202674e2e85be7354e7c77826d28e254b7090a87691dc587f2d`,
adds SV RHS padding/workspace guards and FACT=F injected-return/structural
checks. Both ABIs passed 12 focused tests each; original assertions remain.
The independent public consumer source has SHA256
`54d18c24bf9492cb95f7f6a746ee12a126e481ec43d4aa622a2f058a54479e0a`.
It passed 192 workflows, 960 numerical calls and 192 rejections per ABI in
direct diagnostics, not installed packages. Root read that complete consumer.
The common factor factory and approved private prototype overlay are unchanged.

The ten SVD least-squares files are byte-exact to
`p06-svd-ls-02/verification-ledger.json`; its tested source archive has SHA256
`2a300b834c5d1681384ab45f5c9c3c39fdfd703f95f5a5382648cad60aa25d3c`.
Independent review audited all ten files, eight drivers, eight 21-test lanes,
closure/dependency identities and raw records. The public consumer source
SHA256 is `40cec6a3a6e864718bfb29249250a106113f90715bc33131e50e794ad77d7e51`.
It passed 192 profiles per ABI using known tall/wide/rank-deficient fixtures,
independent residual optimality and nullspace/minimum-norm checks. Root read
the complete consumer. Neither those checks nor source fidelity closes
GELSD zero-RHS/tree/nonfinite gates or GELSS's wide right-vector output failure.

Historical owner reviews retain their original standalone scope. V9 imports
also include their historical reviews, without rewriting frozen evidence.

## Atomic integration and current acceptance

The optional profile becomes `incremental-lapack-v9` in producer, consumer
validation and installed tests. Five new production translation units and
four public headers are registered. ABI ownership/hashes, independent header
oracles and capability/coverage manifests are updated together. Public headers
now total 92; the installed provider suite has 13 consumers.

The central suite adds 32 indefinite expert and 21 SVD least-squares tests,
plus eight normal/no-exceptions public-header checks. Existing allocator,
fault and mathematical assertions retain their original meaning. No existing
BLAS, Random, base-component or CUDA implementation is changed.

Fresh full LP64/true-ILP64, provider-free Debug/Release/shared, installed,
affected all-ASC-C++ sanitizer, strict style and documentation evidence is
required before this integration checkpoint can pass. The previous full V8
results predate its diagonal-overflow correction; V9 must include that fix.
Full normalized routine/mode, platform, provider-shared, XBLAS and required
owner/provenance gates remain open independently of these command results.

## Executed candidate01 checkpoint

Frozen tree `ca060e26980eea5c845185affd8cfc49ab576f01`, archive SHA256
`c3e1bb03955a178a6a846e71cf9e70971cf7988f8a7ad79669798b9706030235`,
mapping SHA256 `37a014c625c372b40cf159915d7ef6cd6d9e56bbc45c0e463f8ff8654353a3b8`
is external `p05-p06-integration-v9-01`. The source archive stays immutable.

Both actual ABIs pass 70/70 Clang19 ASan/UBSan tests, zero skips: all 67
affected registered tests plus the Sylvester, indefinite expert and SVD
least-squares public consumers. Every ASC Core/Dense/reference C++ source is
rebuilt with instrumentation; foreign archives/runtimes are not instrumented.
The harness and generated ABI configurations are hashed in sanitizer metadata.
Strict Doxygen covers 92/92 headers, 1827 public members and zero warnings;
Markdown and all 31 imported C++ file format checks pass. Full combined results are recorded below; each finished raw record is
indexed. No running process is credited as passing.

Candidate01 provider-free Debug/Release each execute 277 tests with 273 passes
and four failures; shared Release executes 279 with 275 passes and the same
four failures, zero skips. Root omitted the five new production files from the
explicit compiled-source oracle and sorted header hashes by Path components
instead of canonical complete-path strings. Both registration errors are
corrected in the live worktree; the original failing checks remain unchanged.
The existing hardening script independently confirms the corrected 92-header
surface. Fresh candidate evidence is still required.

Strict analysis of the imported SVD least-squares mathematical test found
missing direct includes, an unannotated using-directive and a 187-line helper.
Those are test-style defects; production source is not changed to accommodate
them, and every numerical assertion/fixture order must survive extraction.
Candidate01 actual sanitizer profile completion was audited independently:
`completion-audit-01/audit.json` binds full preserved LastTest logs, 474
GELSS/GELSD profiles per scalar (1896 total) and all six indefinite completion
markers in each ABI. This guards against confusing Fortran STOP exit zero
with a completed mathematical run; the separate standard-SVD owner discovered
that issue in its own earlier candidate, which is not part of V9.

## Completed candidate01 findings and correction

Both full provider suites execute 492 tests: 488 pass and the same four
registration checks fail, zero skips. Strict analysis completes all 22 TUs
per ABI: 18 pass and four SVD least-squares test mains fail style checks.
No failed record is removed or credited as a passing checkpoint.

Root adds a private test-only automatic guard at entry of every foreign-calling
provider test main, configuration ABI probe and all 13 installed consumers.
Normal return destroys the guard; `std::exit`/Fortran STOP skips its destructor,
and its registered exit callback diagnoses failure with exit93. Pure arithmetic
count and public-header compilation tests do not enter Fortran. No production
error handler, algorithm, public API, allocator or numerical assertion changes.

External `normal-return-guard-01` tests the real pinned DGETRF/XERBLA STOP
path, ordinary return and `std::exit(0)` on both actual ABIs. Guarded regression
passes 1/1 per ABI: return0 and both premature-exit modes93. The deliberately
unguarded regression fails 1/1 per ABI; separate raw unguarded DGETRF calls
print the native illegal-argument diagnostic and exit0 before the probe's
unreachable return3. These zero exit codes are controls demonstrating a false
pass risk, not routine acceptance. The new repository regression retains all
three controls. Candidate02 must rerun the affected full suites with the guard.

## Corrected V9 identities and bounded follow-up checks

Candidate02 is tree `ebea799755d0fc8d639daa1a470fc18ba4c5014c`, archive
`fb671581fd3a73f32f0918087c02b88198298ebe0bdc63d1a286a5c8ce84d7bd`,
mapping `3689167dbbbf8719f070fe69022044590d5301213f91e873067d7d432761dd40`.
It corrects the four registration failures, preserves all SVD-LS assertions
and adds common normal-return protection. Both actual ABI ASan/UBSan suites
pass71/71, zero skips; full and remaining strict runs are still in progress.

Independent SVD-LS test-fix ledger is
`p06-svd-ls-test-style-v9-01/candidate02/verification-ledger.json`, SHA256
`21e1036531d9dd3f6b822b8d25196c776cd59f89d56f7ee123e2195cb58d1ed8`.
All35 final raw commands pass: eight21-test lanes, ten strict ABI/TU checks
and eight-file format. All132 assertions,90 loop headers and1896 ordered
profiles survive; complete21-test output matches the preserved owner baseline
in every lane. These remain direct-link diagnostics, separate from root full
and installed tests. Root read the full seven-file diff and final review.

Candidate02 strict review found the new STOP control needed a direct
`lapacke_config.h` include. Its intentional `std::exit(0)` also triggers the
thread-safety checker despite running in a single-threaded subprocess whose
purpose is to bypass automatic destruction. A documented single-line
`concurrency-mt-unsafe` exception applies only to that deliberate control.
Candidate03 changes only this test TU: tree
`cd8ad6f4f7956ce6c205e60acc23f90958263bb4`, archive
`e13ff774b871f39875af16bc971abd014d03cbe5509b63934f0d98e50effef4e`.
Both ABI strict checks pass; Debug, Release and ASan each pass1/1 actual
return/exit/pinned-STOP regression per ABI. All81 changed C++ files pass
formatting. These scoped checks do not relabel the full02 suite as full03.

Review of the next GT slice exposed an analogous existing indefinite-header
wording error: implementation rejects all call-metadata aliases before report
reset, while the header singled out aliases to the report itself. Candidate04
clarifies that contract and adds96 exact-capacity plan/workspace-alias cases
across six scalar/family modes; byte snapshots check report, metadata, operands
and scratch preservation. No old test/assertion or numerical production code
changes. Public-header hashes and coverage artifacts update together.
Candidate04 is tree `6f1bbe1f5f481495fc36833ad949e39e5e3bfb5f`, archive
`db4e3745c958bb1af803ae9f282fc0b95bf23ba8fcdab6962d8db40ba3050bed`,
mapping `68607f71dc107e2d013b85a27d1a2b3ec3e89f0bcc6469374c9c542c996be400`.
Scoped current preflight/header, strict and documentation checks are running.
Its first preparation attempted header paths relative to the wrong root and
ran no tests; that failure and ensuing exit127 script launches are recorded
in its preparation-failure.txt. The corrected harness uses actual include
paths and separate normal/no-exception header compiler settings.


## Completed V9 composed checkpoint

Full candidate02 completes both actual ABI suites493/493, provider-free
Debug277/277, Release277/277 and shared279/279, zero skips. Each relocated
provider package runs all13 consumers successfully. All-ASC-C++ ASan/UBSan
passes71/71 per ABI; foreign archives and runtimes remain unsanitized.
Strict02 completes23 TUs per ABI with22 passes and the probe failure already
corrected and rechecked in03. These are executed command gates, not full
routine/mode acceptance.

Candidate04 passes six8-test preflight/header lanes and strict documentation:
92 public headers,1827 documented members,zero warnings. Strict analysis
rejects its three typed memcmp comparisons because metadata objects contain
padding. Candidate05 copies the same live objects before and after rejection
into byte arrays and compares those arrays, preserving the full byte-level
rollback assertion without a suppression. No numerical code changes.
Candidate05 tree `57ccbe0e85aa368e16f5105da1464ae9da60fbd1`, archive
`e28293aacb2ecfc192810d4a4fd0b15e94a68e12fe0c05c5099ef7552423cef6`,
retains mapping `68607f71dc107e2d013b85a27d1a2b3ec3e89f0bcc6469374c9c542c996be400`.
Six fresh scoped lanes pass8/8 each, strict changed TU passes both ABIs,
all82 changed C++ files pass formatting, and current coverage/public-surface
validators pass. The format record's configuration label says81, but its
executed argument list contains82 files, including the newly changed preflight
TU;82 is the actual checked count.

Full02 is not relabeled full05. Source delta proofs bind the test-only03
control change, public documentation/additive04 test and manifest updates,
and test-only05 snapshot-comparison correction. Raw full/ASan completion logs
and all13 installed consumers are hashed in candidate05
`completion-audit-02/audit.json`, SHA256
`cc83db7b1678cabcde6c40a4b05e577c41b0798c0dd8253e9c475d446305459f`.
All four full/ASan logs contain474 SVD-LS profiles per scalar,1896 each lane,
and all six family completion markers. The first audit's JSON-only selector
found no plain-text SVD-LS records and made no profile-count pass assertion;
it is retained, and audit02 uses the actual prefix and asserts every count.

Known mathematical, source-closure, normalized mode, platform, shared-provider,
XBLAS and owner/provenance requirements stay open. The required denominator
remains2113, with198 reference rows in progress,1915 not started and no fully
verified reference rows. Native20 contracts remain implemented_unverified.
