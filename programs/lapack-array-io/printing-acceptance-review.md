# P02 printing acceptance checkpoint

Three additive test files and two registrations are integrated. Current
provider-free Debug, Release, shared Release and Clang19 ASan/UBSan each pass
all four original/additive Dense and Sparse printing tests, with zero skips.
All 23 Core/Dense/Sparse CPU production translation units were rebuilt in
each lane. Six relocated installed consumers each pass 1/1: Dense-only and
Sparse-only consumers in Debug/static, Release/static and Release/shared.
Their compiler dependencies contain installed public headers, and target,
symbol and runtime dependency checks preserve component and provider isolation.
Fresh strict checks pass for both new translation units and the shared private
helper; three-file formatting, coverage validation and public-surface checks
pass. No production source, original test, public declaration or frozen
contract changed. P02 remains in progress.

The new tests cover independent rank-four layout/slice strings, combined
row/column/slice/global previews, zero preview limits, complete-token progress
at every failing byte boundary, balanced byte-budget sweeps, all twelve
Sparse wire scalar types in COO/CSR/CSC, eleven scratch aliases backed by
live typed objects, stored traversal and distant compressed coordinates.
The original tests and their assertions remain unchanged.

The root reviewed all three source files, the frozen display contract and
the preserved test-oracle correction. The owner candidate01 compile failure
and candidate02 Dense runtime failure remain frozen. Candidate02 incorrectly
required an entirely reset report after a sink callback failed at runtime;
the contract retains progress after validation and permits the unprinted
values to mark the report truncated. Candidate03 changes only that new
runtime expectation to zero accepted bytes and zero complete tokens. All
actual preflight alias cases still require all three reset fields. This is
an explicit oracle correction, without a production change or failed-test
waiver.

The owner handoff is `p02-display-acceptance-v12-01/handoff-01.json`, SHA-256
`85dcb6074dd74fe88984bf11ad4713b60da45610be09f307d9a1689bc7f90d44`.
The root audit `p02-display-root-review-01/audit.json`, SHA-256
`7467cbe30d58720b524808f2cd8b4078069c6fc6e3c587ad07adc1c34ab317ee`,
validates 29 executed and eight historical records, 186 dependency hashes,
71 unchanged current source inputs, six installed dependency closures and
the exact three-file import. Historical full runs and installed example
output retain their historical identities. Exact-source owner libc tests
retain their direct-malloc and shared-allocation positive controls; they are
not called fresh root libc or whole-project results.

The current root snapshot is tree
`1c9d0bbb88ac6525ee487e25848cdb251299df04`, archive SHA-256
`44cf3fc6cad8f66b73b0455dfb59c00675d1f4deea68ad6480ff8f248363a573`.
The mapping remains SHA-256
`74dd426bf6b2cf14bff70e151ef84bcbfa73403659714b1458cb967530d2ca63`.
`p02-display-integration-01/completion-audit-01/audit.json`, SHA-256
`39e54750c1db3fae0bd91d4240840bffa0016d43ff5f04ee05acea1918badc61`,
checks all 38 current command records, 932 frozen files against git blobs,
compiled objects and flags in four lanes, complete raw logs and all six
isolated installed consumers. Logs and relocated prefixes are separate.

A separate external diagnostic observes unchanged printer loads with
[Clang SanitizerCoverage load callbacks](https://clang.llvm.org/docs/SanitizerCoverage.html#tracing-data-flow).
The observer is a separately uninstrumented translation unit. Root read all
three diagnostic files and checked the compiler callback contract. Actual
Clang19/O1 execution observes every selected backing byte exactly once and
no omitted or padding bytes for twelve Dense/COO scalar types, plus no Dense
value reads for zero max-elements. The tested CSR and CSC double fixtures
each read 24 value bytes exactly once and 392 offset bytes from 524,296 live
backing bytes, under the independently fixed 480-byte bound. This is scoped
load-count evidence, not a performance claim or proof of every mode.

The repeated-live-load positive control observes two reads. The same source
without instrumentation fails as required; neither negative-control failure
is counted as a passing test. Earlier diagnostic strict failures and their
separate style-only correction remain frozen. The sealed 18-record diagnostic
audit is `p02-display-read-root-review-01/audit.json`, SHA-256
`c3197e3adf273e387efa2d261788ee9dfe0f27a8c1e3ac57a5374cff1c24c5b0`.
Nine fresh root replay records repeat strict/format/build/link, the passing
instrumented CTest and the failing uninstrumented control. The completion
audit above binds those records and the 19 compiler-read production inputs
to the current snapshot. No diagnostic source is installed or registered as
an unconditional portable test.

The owner partial overlays are retained unchanged as
`printing-partial-acceptance.json` and
`printing-load-partial-acceptance.json`; their historical root-replay-open
annotations are superseded only by the explicit current checkpoint here.
Remaining precision/locale combinations, empty and owner-scratch modes,
rank-zero budget/cross-product cases, full read-count modes, real Windows
MSVC and macOS AppleClang remain required. A separate bounded additive P02
candidate is assigned for the remaining Linux cases. No native or Reference
routine coverage credit is added: all 2,113 Reference requirements remain.
