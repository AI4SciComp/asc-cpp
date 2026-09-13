# GNU/Linux shared provider admission

The reviewed shared-support patch is integrated in the feature worktree. Its
scope is Linux x86_64 GNU11.4 with separately attested matching shared providers;
other compiler/platform guards remain. No Reference row is promoted. Native20/array-I/O remains
`SUBSET_REVIEW_READY_FULL_PROGRAM_INCOMPLETE`, and the four first-party robust
PPSVX algorithms retain their separate experimental status. The fixed Reference
counts remain 2,113 required, 378 registered (36 callable-unverified and 342
partial), 1,735 not started and zero verified.

All paths below are relative to the existing external
`asc-cpp-evidence/lapack-array-io/master-continuation-20260910-01` root.
No provider archive, binary or runtime is installed by ASC or published here.

## Source and provider identity

The staged source is `shared-asc-stage-01/source`, from commit
`2814b1a21edda2f006b08fc1d80578513ba795b3`, tree
`95b5f798ef9ab9e0c18145e154004c0a6e24bbd1`. Reviewed amendment04 is
`68951896c1c1508750fc0483354573a72f4a7e232a2bde6c600412da7b89a758`;
its manifest binds eight first-party CMake files. Earlier amendments and their
failed runs remain separate. No C++ numerical source or public declaration
changes in this patch.

Pinned source remains Reference-LAPACK3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`. GNU11.4 builds both actual ABIs;
ILP64 uses BUILD_INDEX64 and real default-integer-8 Fortran. Each shared provider
passes all111 configured upstream tests, without skips. Attestations are:

- LP64: `1f8ca85330618d8bf90806262ecfff64bf532a6e9c10dd1bcd04aff7de431747`.
- ILP64: `2f80bdcaf91fd17bea2db45a6047cd583bed990a6c5bd34572666e9173d71141`.

`shared-provider-finish-{lp64,ilp64}-03` binds the complete provider records.
The initial relocated-toolchain link failure and listing-footer parser failure
remain recorded. A separate search directory references the existing attested
runtime files through symlinks; no provider source, toolchain file or runtime
was modified. The maintained preparation tool and115 normal/optimized tests
are pushed in `2bce86758f779313c0ecd64dcc52e27718fb0303`.

## Concrete findings and corrections

Executable `--wrap` does not rewrite calls already linked inside the ASC DSO.
The retained initial calibration failed both selected tests. The correction
links test-only observation DSOs from the exact production PIC objects and
binds the existing wrappers at that link boundary. Calibration passes10/10 in
both ABIs, including forbidden-read, legitimate-input and output-write controls.
All61 production objects were byte-identical before/after the observation
change. Provider/runtime DSOs remain uninstrumented; no foreign-code sanitizer
or function-wide no-read claim is made.

The first relocated PT example could not find the indirect Dense dependency.
The facet now uses the same origin-relative installed ASC lookup as Dense.
A subsequent clean-loader probe exposed a second real defect: LP64 silently
used the system LAPACK/BLAS, and ILP64 failed to load its provider. The exported
link-only closure now retains exact attested provider/runtime dependencies with
scoped GNU linker state. Corrected installed prototypes run in both ABIs without
LD_LIBRARY_PATH. The runtime checker rejects both retained defective consumers
and accepts the corrected consumers. It checks exact resolved files/hashes and
rejects alternate providers or foreign dependencies in base ASC libraries.

A concurrent-build package attempt timed out at the unchanged300-second limit.
Its entire workspace is retained. Serialized amendment04 runs pass in246.389
and240.001 seconds; no timeout, case, assertion or tolerance was relaxed.
Unchanged provider identity headers now keep their timestamps when CMake-only
link settings change. Both amendment04 reconfigurations relinked without any
C++ object recompilation.

## Completed and outstanding execution

`shared-release-audit-06/audit.json` binds actual IDs, outcomes and runtime
records for the unchanged123-test maintained family selection. Both ABIs pass
118/123, zero skips. Only the four existing PT scalar mathematical tests and
single-real GEDMDQ mathematical test fail. All installed examples and28 package
isolation controls pass with a clean loader environment. Base Core, Dense,
Sparse, Random and Utilities have no LAPACK/BLAS/Fortran runtime dependencies.
Amendment04 Debug and ASC-only ASan/UBSan each pass117/122 per ABI. Their
five failed outputs are byte-identical to the corresponding shared Release
outputs. TSan passes17/17 real-provider concurrency tests per ABI, using
independent caller state. ASC/test objects are instrumented; provider/runtime
objects are not. There are no sanitizer diagnostics or skipped tests.

The full maintained shared Reference selection in `shared-all-{abi}-02`
executes751 tests per ABI:668 pass and83 mathematical processes fail, zero
skips. Package/configuration gates have separate executed records. All78 older
failure outputs match the retained v27 static records after normalizing only
source-directory prefixes. The five added PT/GEDMDQ outputs match actual static
hosted run34555581852 at `2bce86758f779313c0ecd64dcc52e27718fb0303` by the
same comparison. No numeric field, assertion, source line or tolerance is
removed. These are83 failed processes, not83 independent root causes.
`shared-math-signature-comparison-01`, `shared-added-math-comparison-01` and
`shared-profile-math-comparison-01` bind those comparisons.

`shared-admission-shared-{abi}-01` passes target inventory, five configured
architecture/public-surface/configuration tests (including seven configuration
rejection/control cases), and maintained ELF inspection in both ABIs.
`shared-admission-static-{abi}-01` freshly compiles all product libraries,
passes the same five gates and passes the complete relocated package in268.296
and268.875 seconds. Static provider metadata and generated identity headers are
byte-identical to the preceding admitted static builds; see
`shared-static-metadata-comparison-01`. No ABI baseline or validator is weakened.

`shared-root-import-01` records a clean application of exactly the eight
amendment04 files and verifies every resulting hash. Root support documentation
and the hosted family matrix now distinguish static/shared providers, each in
both actual ABIs. Strict Doxygen, links, documentation consistency, scoped
coverage and backlog validators pass in `shared-integration-doc-checks-01` and
`shared-integration-record-checks-01`. Counts and native evidence are unchanged.
The integrated source tree `d5a1a3bd92bfc521fce4fc1bf95fd5a708201641` is frozen
in `shared-integrated-frozen-01`. All1,121 build/API/test/example/ABI files are
byte-identical to the tested staged inputs. Both fresh archived-source builds
in `shared-integrated-{abi}-01` pass122/127 selected tests with the same five
mathematical failures and no skips. Their complete relocated packages pass in
250.955/250.683 seconds, including clean exact runtime closure and all public
consumers. `shared-integrated-audit-01` binds actual configured IDs, commands,
JUnit results and unchanged five-failure signatures. Fresh normalized ELF
reports are byte-identical to the staged reports in both ABIs.

The subsequent review/state edits change narrative only; they do not make a new
product candidate or attribute hosted verification to an unexecuted tree.
The integrated revision's new hosted static/shared matrix remains pending.

The Windows tooling defects found by hosted CI were corrected separately in
`4e4e2b1aca29e51c5b659a6e59a01ff60902d6d5`: migration outputs preserve exact
UTF-8/LF hash bytes, and exclusive-directory diagnostics no longer depend on
POSIX error wording. All115 normal/optimized local tests pass. Real migration
replay reproduces the three maintained manifests byte for byte; no evidence
hash, status or validator acceptance rule changes. Hosted Windows verification
is tracked separately from provider admission.

GNU/Linux evidence gives no Windows, macOS or full Clang-provider credit.
The local missing/unadmitted resources are recorded in
`platform-resource-review-02` and `platform-container-resource-01`; provider-free
hosted jobs do not certify foreign ABI/provider contexts.

Root native inputs and numerical requirements are unchanged. The Reference
PPSVX five-cause decision, PT reciprocal cause and SGEDMDQ missing-vector cause
remain in the [owner packet](owner-decisions.md). Shared behavioural fidelity
cannot close their mathematical gates. Notice approval remains separate.

After this platform slice, `P09.required.lascl` is dependency-ready. The four
pinned scalar sources, seven G/L/U/H/B/Q/Z modes and actual symbols in both
static providers are bound in `lascl-prerequisite-01/review.json`. Initial original LASCL source and checked declarations are now external in
`lascl-source-01`, with all seven modes, explicit packing and direct-column
paths. Both actual-ABI compilation checks pass in `lascl-compile-initial-01`.
The independent test source is written but not yet executed; there is no LASCL
registration, installed API or numerical acceptance credit. Finish its tests,
review/integration and the remaining P04–P09/P11 queue.
