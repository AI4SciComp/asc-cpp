# Packed Bunch–Kaufman solve contract

Active task: `P05.required.hptrs`, following packed producer checkpoint
`e40387f140208b823f012fd255c248915df59ae2`. This slice contains exactly
SSPTRS, DSPTRS, CSPTRS, ZSPTRS, CHPTRS and ZHPTRS. The engineering
checkpoint below is complete; required numerical acceptance remains open.

## Source and interface

The exact six required source instances match the maintained inventory and
pinned Reference commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`.
All twelve actual GNU emissions, LP64 and true ILP64, confirm eight pointer
arguments UPLO,N,NRHS,AP,IPIV,B,LDB,INFO and the GNU size_t CHARACTER length.
No native WORK argument or workspace query exists. Independent native probes
pass 1,080 cases per ABI: full-width INFO/pivots, guarded operands, factor
reconstruction, known solutions and residuals, all six scalar/symmetry classes,
both triangles, orders 0/1/2/5/17, zero/one/three RHS, and ordinary dyadic scales.
Factors and pivots remain unchanged after each solve. These allocated-buffer
probes establish neither arbitrary memory-read safety nor range acceptance.

The new explicit-provider header exposes QuerySptrsWorkspace/Sptrs for S/D/C/Z
and QueryHptrsWorkspace/Hptrs for C/Z. Immutable packed factors and raw pivots
require successful matching SPTRF/HPTRF provenance, scalar/provider identity,
triangle and symmetry. Raw descriptors cannot prove historical provenance.
The existing Bunch–Kaufman paired-pivot tag is required; dense-factor factories
are unchanged. SPTRS uses ordinary transpose, HPTRS conjugate transpose.

## Metadata, workspace and numerical behavior

Metadata-only queries bind N/NRHS, triangle, symmetry, factor/RHS layouts,
exact pivot count and original/effective RHS stride to the provider/scalar.
Nonempty calls require N caller-owned native INTEGER objects, plus packed AP
entries for row-major factors and N*NRHS live scalar entries for row-major RHS.
The simultaneous packing spans occupy the common layout-conversion region.
No allocation, transfer, fallback or global handler change is introduced.

Both packed source branches form N*(N+1) before division. That full product
bounds packed cursor sums and paired-block updates; strided RHS BLAS terminal
cursors 1+NRHS*LDB are checked independently. Public descriptor and byte bounds
remain in force. Empty order and zero RHS are checked noncalls without numeric
reads, scratch access or native INFO. Unused original strides remain bound in
the plan even when the native single-column stride is compacted.

All live operands, scratch and metadata must be disjoint and provider-accessible.
Structural rejection preserves numeric storage/scratch; unsafe metadata aliases
also preserve the report. Nonempty calls validate all signed one-based paired
pivots and directional bounds, then reject exactly zero source-evaluated block
divisors before mutation with a singular outcome and zero-based block index.
Factor entries are not normalized as original Hermitian inputs. This preflight
is not a broad finite-data scan or an invertibility certificate.

Only INFO=0 is valid after a checked solve. Missing or nonzero full-width INFO
is a provider defect. Row-major RHS publication is withheld on a defect;
direct column-major RHS can retain native effects. Successful execution
publishes complete RHS and preserves padding, factors and public pivots.
Provider fidelity and independent mathematical acceptance remain separate.

## Executed engineering and current numerical disposition

The maintained ordinary suite executes 864 cases per scalar/symmetry class
and test mode, with independent solution/residual and separate raw native
fidelity. The range suite executes 128 cases per class/mode across all eight
triangle/factor-layout/RHS-layout combinations. Its original finite A and B
have known finite solutions. Scalar and zero-diagonal 2x2 blocks use scales
min-normal/8, min-normal/2, min-normal and 0.75max. Checked producer
reconstruction succeeds for these fixtures, isolating the solve limitation.

Both actual ABI initial suites have six required mathematical failures and
six range-fidelity passes. No failed test is inverted, disabled or skipped.
The pinned source uses a reciprocal followed by scaling for scalar blocks;
small finite divisors can therefore produce nonfinite solutions with INFO=0.
Complex 2x2 block divisions retain their separate native range behavior. The
full logs identify each triangle, layout, order, RHS count and scale. These
outcomes need an explicit permitted numerical/provider strategy for acceptance;
they do not authorize a hidden algorithm or source/provider change.

Validation includes malformed/directional/paired pivots, exact source-zero
1x1 and 2x2 divisors, stale identities and requirements, original RHS strides,
short/misaligned/inaccessible or overlapping scratch, metadata alias rules and
operand preservation. Linux protected pages prove metadata-only queries,
zero-RHS noncalls and early scratch rejection do not read numerical inputs.
The final maintained validation set has 328 cases per scalar class. Pure
integer tests exercise both native widths, full packed products and strided
BLAS terminal cursors without allocating impossible matrices.

Each scalar fault test executes 672 cases using actual native solves and
separate omitted, partial-width, negative, positive and maximum INFO injection.
Checks preserve native raw diagnostics, row-RHS withholding, direct-column
native effects, padding and immutable factors/pivots. Four concurrent workers
share read-only factors, pivots, provider and plans in 144 groups per class,
four repetitions, with independent mutable RHS/scratch/reports, independent
mathematical serial baselines and stale-plan rejection. Empty and zero-RHS
paths remain checked noncalls. The initial fault/concurrency suites pass all
12 tests in each actual ABI.

Both maintained emitted-ABI probes pass 1,080 cases per ABI. Eight production
and test translation units plus the installed consumer pass strict analysis;
the bounds proof is local and all numerical assertions/tolerances remain.
Four relocated static/shared LP64/ILP64 consumers each pass 576 guarded packed
producer workflows and 2,160 successful/empty solve workflows. They use only
installed public headers, preserve factor/RHS/pivot/scratch boundaries and
reject stale solve plans. All five source/installed public-surface comparisons
pass with 160 headers and six components; both ABI export comparisons show
12 additions and no removed ASC symbols.

The final sixteen actual LP64/true ILP64 static/shared Release, Debug,
ASC-ASan+UBSan and TSan profiles execute 552 processes: 480 pass, 72 required
range-mathematical failures and zero skips. Each of the twelve normal/sanitizer
profiles passes 38/44 and retains the six range failures. All four TSan profiles
pass 6/6. The pinned Fortran/BLAS internals remain uninstrumented. Final
source, binary, provider and exact selected-test identities are in each
`packed-solve-{linkage}-{mode}-{abi}-01` record and the combined audit.

Four standalone public-header tests, ten package-manifest checks and three
architecture checks pass. The workflow's actual listing and execution regexes
match and select all 44 solve runtime tests; its complete required-set guard
passes on 1,649 configured selected tests. An initial guard-extraction command
omitted the actual positional argument and is retained as a setup failure.

The final strict manifest binds nine passed translation units to source hashes.
An unnecessary all-header override also scanned immutable third-party
lapack.h/lapacke_config.h and failed on their C/Fortran naming conventions.
Those failed attempts are retained; all diagnostics are classified to those
two provider headers. The five affected TUs pass under the unchanged root
.clang-tidy configuration and its complete prescribed first-party header
filter. No provider bytes, tests, assertions or lint policy were changed.

These six routes and 48 modes remain implemented but unverified; no failed
execution enters a passing ledger. Required solve-range/producer-endpoint,
provider/platform, security, XBLAS and notice gates remain unresolved. The
next independent family is SPTRI/HPTRI: its source hashes, both actual emitted
ABIs and 432-case native inverse prerequisites already pass. Full programme
status remains **FULL_PROGRAM_INCOMPLETE**.

External records are under the existing continuation's
`completion-execution-01`: `packed-solve-source-review-01`,
`packed-solve-emissions-01`, `packed-solve-native-final-{lp64,ilp64}`,
`packed-solve-contract-review-01`, `packed-solve-range-validation-01-{abi}`,
`packed-solve-fault-concurrency-01-{abi}`, `packed-solve-strict-0*`,
`packed-solve-installed-02-{linkage}-{abi}` and
`packed-solve-public-surface-01`. Failed setup/strict attempts remain retained.
