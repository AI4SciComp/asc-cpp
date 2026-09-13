Current native status: 20 verified rows through the existing acceptance validator,
112 modes and 1,120 required class slots. Normalized evidence is in
`docs/contracts/lapack-evidence.json` and `lapack-native-executed.json`; current
implementation/test/platform bridges and fresh replays are bound by
[the subset checkpoint](stabilization-review.md). The original 40 alias slots
are closed. This grants zero Reference-provider credit.

Everything below is historical at its stated revision; its earlier open slots
and next integration steps are preserved rather than reapplied.

Native20 acceptance is implemented as additive external tests and reviewed partial records. No native production file, original native test, frozen contract, preserved owner worktree or shared root ledger was edited. The source snapshot is `a0a1e156041015636534627098e1e025e90e2ac3`; all 58 recorded native/Core/Dense transitive source inputs also match the frozen V12 source. Historical V9-02 full-suite commands retain their historical identity.

The final import candidate contains four new test files:

| Destination | Frozen source |
| --- | --- |
| `tests/dense/native_mode_acceptance_test.cc` | `consumer02/native_mode_acceptance.cc` |
| `tests/dense/native_alias_acceptance_test.cc` | `aliases04/native_alias_acceptance.cc` |
| `tests/dense/native_alias_support.h` | `aliases04/native_alias_support.h` |
| `tests/dense/native_concurrency_test.cc` | `concurrency02/native_concurrency.cc` |

`native-test-handoff.tar` contains exactly those four files under their proposed paths. `native-test-source.sha256` binds every byte. `registration-snippet.cmake` uses the existing `_asc_dense_configure_test` and `asc_register_test` APIs; the concurrency target links `Threads::Threads` and does not link the process allocation replacement. No product declaration or public-header manifest entry is added. The separate `interop02/interop.cc` is an external pinned-provider numerical oracle diagnostic and is not proposed for unconditional provider-free registration.

The mode consumer covers 48 scalar LU solve modes, 32 scalar Cholesky solve modes and eight QR scalar/layout modes. It adds independent factor/RHS layouts, known two-RHS scalar answers, empty order and empty RHS, structural/provenance rejection, exact-zero diagonal rollback, complex diagonal normalization, workspace shortage, nonfinite QR input and computed overflow. The alias consumer covers every one of the 112 native routine/mode combinations, including complex abs1 pivot selection and first ties, all-layout pivot/placement validation, all-mode factor/RHS and pivot/RHS aliases, malformed options and defensive pivot/diagonal checks, report overlaps, and QR lengths/increments/placements/operand aliases including unused workspace tails.

The mixed-type conservative alias fixtures keep separate live scalar blocks and a live pivot/report object in the intervening padding. They are used only for metadata rejection before numerical access. Ordinary numerical factorization and solves use actual scalar arrays. No report is treated as a live floating-point value, and no successful factor provenance is fabricated. The original LU, Cholesky and QR test files remain byte-identical and their assertions, tolerances, fixture order and loops are unchanged.

Executed evidence for the exact final sources:

- The mode consumer passes relocated installed static Debug, static Release and shared Release, each 1/1. These CXX-only CMake consumers import only native Dense/Core dependencies and explicitly reject unrelated targets.
- The alias consumer passes the same three installed lanes plus Clang ASan/UBSan and glibc allocation observation, each 1/1. Every final log contains all 112 unique mode profiles, with no failed or skipped tests.
- Fresh ASan/UBSan and libc replays each pass all four tests: original LU, Cholesky, QR and the mode consumer. Full logs retain 240 emitted LU profiles, 48 Cholesky reconstructions/192 solve residuals, 248 QR numerical profiles and the new 48/32/8 mode counts. Exact allocation observations come from regular GNU and libc lanes; sanitizer-owned allocation operators retain the repository's accepted observation exception.
- Four independent scalar workers pass 20,000 native calls in both GCC and ASan/UBSan runs. No race-detector claim is inferred.
- Native raw QR factors pass the pinned ORGQR/UNGQR oracle in 32 workflows with actual four-byte LAPACK integers and 32 with actual eight-byte integers. The common normal-return guard is active. This adds zero Reference coverage credit and no native ORG/UNG/ORM/UNM rows.
- All six current native production/original-test strict TUs pass. The final mode and concurrency TUs pass strict checks; the final alias TU and its private header pass with an explicit header filter. Final new-source formatting passes. Actual tools are GCC 11.4.0, Clang 19.0.0, clang-tidy 18.1.8 and clang-format 19 under the repository configuration.
- Historical V9-02 provider-free Debug 277/277, Release 277/277 and shared 279/279 records are hash-audited. Their reuse is limited by explicit transitive source equivalence, not relabelled as an a0a1e15 or V12 full run.

`verification-ledger-01.json` audits 80 executed command records, three historical full-suite records, 155 dependency hashes, 75 native/Core/Dense/original-test compiler dependency files and 19 sanitizer production dependency files. `native20-evidence-index-02.json` additionally binds raw LastTest logs, exact selected test records and the 20 pinned upstream source hashes used by the preserved contracts. `supplemental-validation-03.json` records the final schema checks, installed alias dependency isolation and V12 source bridge.

All earlier failures remain frozen. The 80-record ledger includes the first alias compile failure, the first consumer/concurrency strict failures and the alias03 private-header strict failure. Each was corrected in a new candidate. An initial ILP64 diagnostic harness requested unsuffixed archive names; it was rejected before compiler execution, then recovered from the preserved actual `*64.a` installation and replayed successfully. One supplemental Python schema import created a new bytecode file inside the external frozen source directory; the source-change guard correctly rejected its evidence. Only that newly generated bytecode was moved to external quarantine, original bytes were unchanged, and a fresh bytecode-disabled validation passed. These failures receive no passing evidence credit.

The reviewed overlay retains exactly 20 native rows, 112 modes and 1,120 required class slots. It records 1,080 slots with described executed Linux evidence and 40 partially covered slots. All 20 routes remain `implemented_unverified` with empty official `evidence_ids`. Required Windows MSVC and macOS AppleClang matrices remain open. The 32 POTRS padding/alias slots retain the valid same-call factor/report overlap case: a successful factor over separate C++ array objects or a manufactured report is not accepted as evidence. The eight GEQRF padding/alias slots retain separate report/tau and report/workspace overlap fixtures; the executed report/A case does not establish those cases. All Reference 2,113 requirements remain unchanged and incomplete.

The next integration step is to review the four source files and registration snippet, import them locally, and run the composed exact-root native checks. A concrete command after registration is:

```sh
cmake --build <frozen-build-dir> --target asc_dense_native_mode_acceptance_test asc_dense_native_alias_acceptance_test asc_dense_native_concurrency_test --parallel 1
ctest --test-dir <frozen-build-dir> -R '^asc_cpp\.dense\.(lapack_(lu|cholesky|qr)_test|native_(mode_acceptance|alias_acceptance|concurrency)_test)$' --output-on-failure --no-tests=error --test-output-size-passed 1000000
```

The relocated installed harnesses are preserved at `consumer02/CMakeLists.txt` and `aliases04/CMakeLists.txt`. They can be configured in new build directories against the integrator's exact relocated prefix; do not reuse a previous record directory or overwrite its raw logs. Root integration/replay and the explicit remaining native mode/platform gates are required before any verified promotion.

Root independently read the four final test/support files, registration, frozen
contracts and all distinct class-scope descriptions. The read-only root audit
`native-mode-root-review-01/audit.json` (SHA-256
`65095fa2f9f39fe2fc11c7fb8d6974f7208e15d3875de557dad42558f37b0c2e`)
checks 86 executed/historical/supplemental records, preserving four earlier
compile/style failures and the rejected bytecode-mutating validator attempt.
It validates every final 112-mode emitted set independently, 155 dependencies,
all source bridges and the current unchanged 58 native source inputs.
The exact four files are imported and three tests registered; current composed
root verification is pending. The partial overlay is retained unchanged,
with zero native or Reference verified promotion.

Current root validation completed on tree
`6c51559b3e82e3c843deff91356272ea019c8f91`, source archive
`db6a458c428cfe8fc684597a2107e69662a437cd415631ca75bff45ae0ebf48b`.
Provider-free Debug, Release, shared Release and Clang ASan/UBSan each
pass all six original/additive native tests, zero skips. All 19 Core/Dense CPU
production translation units were rebuilt per lane; no foreign provider is
linked. The 112 alias modes, 48/32/8 additive solve/factor modes, 20,000
concurrent native calls, original 240 LU profiles, 248 QR profiles and
48 Cholesky reconstructions/192 solve residuals remain explicit in raw logs.
Fresh strict checks for all three added translation units (including the
private alias helper), four-file formatting, coverage and public-surface pass.

Clean relocated installed consumers each pass 3/3 in Debug/static,
Release/static and Release/shared. Each consumer compiles against only the
installed public Dense/Core headers; target, symbol and runtime isolation is
checked. The initial install harness reused its log directory as its prefix,
so relocation also moved the logs. That complete attempt is preserved. The
new `install-replay-01` uses separate log directories and clean prefixes;
only installation and consumers were repeated, using the same built libraries.
No original log or tested source was overwritten.

The completed audit `native-mode-integration-01/completion-audit-01/audit.json`
(SHA-256 `d20afec7355917c3a4df5961eaf54467ef8523388e64a57bf466071ec8d51005`) binds all42 executed
records,929 frozen source files, compiled objects/flags, full logs and installed
consumer dependencies. The historical libc and raw-QR interoperability controls
retain their original source identities with the independently audited unchanged
source closure. No fresh root libc, full-project, race-detector, other-platform
or complete native-mode verification is inferred. Native20 remain
implemented_unverified;40partial class slots and required platforms remain open.
