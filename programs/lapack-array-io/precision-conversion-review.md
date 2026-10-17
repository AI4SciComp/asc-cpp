# Checked full-matrix precision conversion

This P09 leaf adds the four separately named Reference-provider routines
SLAG2D, DLAG2S, CLAG2Z and ZLAG2C, with eight public query/execute declarations.
It uses the unchanged pinned LAPACK 3.12.1 provider; it implements no copied
numerical algorithm, hidden conversion or fallback. The initial admitted build
scope remains Linux x86-64, GNU 11.4, static, actual LP64/global ILP64. Four
catalogue rows receive callable-unverified status after the checks below;
normalized Reference execution records, wider platform and owner admission
remain separate. This does not promote Reference PPSVX or close P09.

## Contract and source review

Queries inspect only shape, placement, strides, alias spans and provider
identity. Input and output are same-shaped full matrices with independent
padded row/column layouts: four mode IDs per routine. Every output is staged
in explicit live output scalars (`kScratch`); row input packing uses separate
live input scalars (`kLayoutConversion`). Column input is passed directly.
Preflight and native range/provider failures preserve all caller output bytes;
the provider may modify scratch. Input and padding stay unchanged. No numeric
output is read before foreign entry. Empty M or N completes locally, without
native INFO or numeric access. Reusable plans bind the conversion direction,
scalar pair, dimensions, both layouts/strides and provider identity.

The four pinned source definitions were read. Nonempty foreign dimensions are
strictly below the selected signed integer maximum to protect M+1/N+1 loop
terminals, and actual foreign leading dimensions fit that ABI. Checked size
arithmetic bounds simultaneous live staging/packing and reachable spans.
`conversion-native-address-review-01` extracts and hashes the eight actual
archive members and retains their disassembly: native address/stride
arithmetic uses 64-bit registers in both prepared ABIs, while loop/INFO widths
match LP64 or global ILP64. This compiler-specific observation does not widen
the supported compiler/platform policy. Boundary tests exercise pure count
arithmetic, without fabricated large arrays.

Finite widening is exact for the supported scalar representations. Narrowing
has the pinned component conversion's rounding and underflow; conversion is
not reversible and no lost bits are reconstructed. DLAG2S/ZLAG2C INFO=1 means
a component lies outside finite single precision range. ASC reports
`kNumerical/kAccuracyWarning`, actual INFO=1, unchanged caller output.
Only INFO=0 is valid for widening. Negative, excess, omitted or selected
short-width INFO is a provider defect, with unchanged caller output. All INFO
storage starts at the selected full-width signed minimum.

NaN comparisons in the pinned narrowing routines do not report range overflow.
After valid INFO=0, ASC publishes numeric output and reports a numerical
accuracy warning if any published component is nonfinite; the value remains
visible. Scratch NaN seeds make omitted writes visible, but cannot distinguish
an omitted write from a legitimate NaN conversion. The report does not claim
that distinction. No rounding mode, FTZ/DAZ setting, tolerance or domain is
changed. The maintained oracle uses actual rounded input components and
ordinary scalar casts, exact dyadics, signed zeros and representable endpoints;
it does not require a wider long-double exponent range.

## Maintained tests and observable boundary

Ordinary tests cover four shapes per dimension (including zero), all layouts,
padding, aliases, stale plans, exact one-byte-short active workspace, finite
endpoints, signed zero, underflow, NaN/Inf and narrowing range failure. Serial
calls check allocation freedom. Four concurrent real-provider threads own
separate contexts, buffers, plans, reports and workspace. The existing global
allocation-audit counters are excluded from concurrent calls; their harness
race in an earlier attempt is not attributed to production code.

Four fault tests exercise real calls followed by negative/excess/one/omitted/
short-width INFO publication and a separately identified synthetic omitted
output control. Full-width INFO and output validity are checked explicitly.
Thread-local wrapping state isolates the ELF test mechanism. Exact installed
provider declarations are checked by four compile-time signature comparisons.

The calibrated Linux observer uses initialized, aligned containing arrays and
PROT_NONE. An actual wrapped foreign-entry callback restores output access
before entering uninstrumented Fortran. Per run, 64 intentional forbidden
output-read controls must fault, eight legitimate row-input packing controls
must fault before entry, 16 active conversions pass, and 48 empty contexts
complete locally. Metadata query/stale/workspace paths execute while numeric
pages are protected. No signal handler resumes a faulting instruction. The
observation covers ASC pre-entry accesses to the protected caller regions;
it does not claim foreign-code instrumentation or treat final bytes alone as
proof of absence of reads.

The maintained `examples/precision_conversion` uses public headers and only
`ASC::dense_lapack`. Both independent layouts, all four conversions, plan
reuse and actual narrowing range rejection execute after installation and
relocation. Package integration copies this example into its own isolated
consumer source. No historical evidence path or private header is required.
Separate provider-free consumers retain their dependency isolation.

## Execution records

Raw records are under `master-continuation-20260910-01/`. The configured ten
tests are ordinary238–241, fault242–245, observation246 and public example247.
These IDs are tied to retained configured listings, not permanent catalogue
IDs. `conversion-debug-lp64-05`, `conversion-debug-ilp64-01`, and both
`conversion-{release,sanitizer}-{abi}-01` pass ten of ten, with no skips.
Final full-width report assertions also pass four-test Debug repetitions
`conversion-debug-lp64-06` and `conversion-debug-ilp64-02`.

`conversion-tsan-{abi}-01` passes all four real-provider concurrent tests in
both ABIs, using the existing process-local setarch workaround. ASC/tests are
instrumented; provider Fortran is not. `conversion-debug-{abi}-headers-01`
passes normal/no-exception independent header probes784/785. Both
`install-conversion-{abi}-01` runs pass install, relocation, public execution,
export inspection and provider-free base runtime isolation. Strict Doxygen
generation/audit passes in `conversion-documentation-01`.

Earlier attempts are preserved: Debug LP64 attempt01 failed compilation on an
unchecked nodiscard status; attempt02 failed on incorrect test helper names and
memory-view construction; neither executed tests. Attempt03 passed seven of
nine tests and exposed the global allocation-audit race in two concurrent test
processes. Attempt04 passes all ten after the bounded test-harness correction.
Style attempts retain findings and subsequent fixes. The public report review
corrects two widening comments and adds an explicit INFO=1 direction control;
its original files and separate final repetitions are preserved.

The native20 source/contracts/routes are unchanged. The chained compact
mapping extension preserves the original native execution commit/tree and
records, with no later whole-tree credit. Existing PT/PPSVX and other required
mathematical failures remain failures. No required case or tolerance is
removed, and no new numerical acceptance is inferred from provider fidelity.

Final `conversion-{debug,release,sanitizer}-{abi}-report-final-01` passes
all five affected fault/observer tests after the INFO=1 direction control.
`conversion-executed-input-comparison-01` binds the unchanged numerical source
and ordinary/concurrency test inputs; the header change is comments only.
`conversion-final-style-docs-01` passes final strict analysis, format18 and
Doxygen (131 headers, 2277 public members, zero warnings).

`conversion-integration-{abi}-01` retains the failed stale header-count check
(131 listed, 130 required); the other seven checks pass. The bounded correction
updates the independent count and diagnostic to131. Both fresh guarded
`conversion-integration-{abi}-02` runs pass all eight affected architecture,
header, documentation and complete package tests. The capability header list,
installed exports, independent header oracle and ABI manifests change together.

The staged tracked-source tree `a3a8bc38aad27a4b6910577138cc89ac6e5db5f9`
is frozen in `conversion-frozen-product-01`, excluding both preserved user
instruction files. `conversion-fresh-{abi}-01` configures and compiles from
that source archive, then passes all13 selected numerical/fault/observer/public/
header/full-package tests in both actual ABIs with zero skips. Installation
and relocation use only maintained examples and explicit matching prepared
provider inputs; provider archives/runtimes are not bundled. Subsequent record
updates use an exact unchanged-product comparison, not another product freeze.

Scoped catalogue validation passes in `conversion-record-checks-01`: required
2113, callable-unverified Reference28, partial338, not-started1747, verified
Reference0, reviewed contracts52. Native20 remains callable/verified20. These
are366 registered Reference rows, not366 verified routines.
