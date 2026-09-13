# Packed Bunch–Kaufman inverse contract

Active task `P05.required.hptri` follows pushed solve checkpoint
`b779474925ccc4275303512e7ee18ce2d3489483`. These six SSPTRI/DSPTRI/CSPTRI/
ZSPTRI/CHPTRI/ZHPTRI adapters have completed their engineering gates. Their six rows and 24
triangle/layout modes remain implemented_unverified; numerical acceptance is
incomplete and no Reference verification is granted.

The six exact pinned source instances match the maintained inventory at
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. Both GNU integer ABIs have actual
emitted signatures UPLO,N,AP,IPIV,WORK,INFO plus the size_t character length.
Independent guarded native prerequisites pass 432 ordinary cases per ABI:
orders 0/1/2/5/17/65, both triangles, three dyadic scales, diagonal and paired
blocks, source-independent factor reconstruction and A*inverse(A) residuals.
These are prerequisites, not finite-range acceptance.

The checked API follows the existing dense classic inverse mutation contract.
Mutable packed factors and immutable exact-N raw Bunch–Kaufman pivots require
matching completed SPTRF/HPTRF origin, including completed singular factors.
Raw descriptors do not prove provenance or numerical content. Existing dense,
Rook, RK and Aasen factories remain unchanged. SPTRI uses transpose and HPTRI
adjoint; factor packing preserves complete coefficients without original-input
Hermitian normalization. In-place inversion invalidates borrowed factor views.

Metadata-only queries bind order, triangle, symmetry, layout, pivot count,
scalar and provider. Active workspace is exactly N live same-scalar WORK and
N native INTEGER objects, plus N*(N+1)/2 live scalar packing entries for row
layout. Both triangles form N*(N+1) before division. That full product bounds
swap products, packed cursor sums and unit-stride BLAS endpoints; terminal
negative lower KCNEXT remains bounded by -2*N. Signed pair/directional
validation independently bounds all data-dependent pivot subscripts. Byte
totals and the explicit CPU accessibility/disjointness checks remain mandatory.

Empty order is a noncall with no numerical/scratch access or native INFO.
Nonempty execution mirrors only the source's immutable singular scan: exactly
zero positive-pivot 1x1 D entries, upper bottom-to-top or lower top-to-bottom.
HPTRI compares the full complex diagonal here before subsequently using its
real part in arithmetic. Actual positive INFO must match that source index;
AP is unchanged, no inverse exists, and the report is singular/documented-
partial with a zero-based diagnostic index. No invented 2x2 or finite-data
diagnosis is introduced. Native INFO starts at its full-width minimum and must
match the scan result; native input pivots must remain unchanged. A defect
withholds row-packed output and marks output unusable; direct column AP can
retain native effects. INFO=0 publishes the selected packed inverse. WORK has
no defined result and no new factor-family certificate is issued.

The final sixteen profiles use actual LP64 and true global ILP64 providers,
static and shared linkage, Release, Debug, ASC ASan+UBSan and TSan. Twelve
normal/sanitizer profiles each execute 44 tests: 40 pass and four required
complex mathematical tests fail. Four TSan profiles each pass all six
concurrency tests. Total: **552 selected, 504 passed, 48 required failures,
zero skips**. Exact source, binary, compiler-cache, provider-attestation and
archive hashes are retained per profile. Foreign Fortran/BLAS internals are
uninstrumented; process-local sanitizer ASLR handling does not suppress a
sanitizer finding or change provider code.

The maintained ordinary/fidelity suite executes 288 cases per scalar/mode,
including empty, scalar, paired, nontrivial and completed singular factors.
Native faults execute 640 cases per scalar, covering omitted/truncated/full-
width INFO, contradictory or wrong singular indices, maximum INFO and private
native pivot corruption. Structural tests execute 168 cases per scalar plus
Linux PROT_NONE query/empty/short-work observations. They verify pre-mutation
rejection, metadata aliases, all paired-pivot directions, stale layouts and
short/misaligned/overlapping scratch. Pure count checks cover both INTEGER
limits. Concurrency executes 72 groups per scalar with four workers/four
repeats, shared read-only pivots and plans, separate mutable buffers and
reports, independently verified serial inverses and stale-plan rejection.

The required range suite contains 72 cases per scalar/mode, including 60
representable inverses and 12 explicitly classified nonrepresentable controls.
Every normal/sanitizer profile retains four failed mathematical processes:
CSPTRI, ZSPTRI, CHPTRI and ZHPTRI. Failing inputs are 2x2 zero-diagonal paired
blocks whose off-diagonal real and imaginary components are each 0.75*max,
for both triangles and layouts. Exact inverse components are finite and
representable. All six native-fidelity processes and both real mathematical
processes pass. Faithful native behavior does not satisfy the unchanged
numerical requirements. HPTRI forms ABS(complex off-diagonal), which overflows
for this magnitude; its inverse normalization uses that value. SPTRI complex
arithmetic also fails these cases; no broader root-cause or provider/compiler
repair is claimed. No input, test, tolerance or acceptance rule is relaxed.

All four relocated public-only installed consumers pass 576 producer/inverse
workflows each. Prefixes contain spaces; copied consumers have no source-tree
or producer-build include paths. Runtime dependencies and installed metadata
are audited. Build the maintained `installed_indefinite_packed_inverse` target
in the configured installed-consumer project and run
`ctest --test-dir <consumer-build> -R '^installed_indefinite_packed_inverse$'`.
Ten strict translation units, both 432-case maintained emitted-ABI probes,
four standalone public-header tests, ten package-manifest checks and three
architecture checks pass. Source and four installed surfaces each have 161
headers. Each ABI adds twelve exported declarations and removes none. Actual
CI listing/execution selectors agree and select all 44 inverse tests among
1,695 configured tests; the full required-name guard passes. Documentation
checks cover the complete public surface with no warning waiver.

All raw records remain outside the source tree under the existing
`completion-execution-01/` evidence root. The final audit is
`packed-inverse-final-audit.json`, with `packed-inverse-profile-results.json`,
`packed-inverse-installed-final-results.json`, `packed-inverse-strict-final.json`,
`packed-inverse-native-final-results.json`, `packed-inverse-integration-01`,
`packed-inverse-public-surface-01`, `packed-inverse-ci-selector-01` and
`packed-inverse-documentation-01`. Preliminary and failed lint/setup attempts
remain separate from successful final-source records. The evidence extension
rebinds only reviewed additive integration artifacts; Native20 and historical
execution outcomes are unchanged.

Continue the six SPCON/HPCON packed condition estimators. Exact source rows,
twelve emitted declarations and both 648-case guarded native prerequisites are
ready in `packed-condition-source-review-01`, `packed-condition-emissions-01`
and `packed-condition-native-02-{lp64,ilp64}`. Provider range/endpoint, platform,
security, XBLAS, notices and full P00–P11 acceptance remain unresolved. This
engineering checkpoint is not whole-program completion.
