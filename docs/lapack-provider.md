# Explicit Reference-LAPACK development facet

`ASC::dense_lapack` is an optional Dense-owned facet, not a seventh module.
`ASC::dense`, `ASC::cpp` and every other base component remain provider-free.
The `incremental-lapack-v20` facet implements checked `Getrf`, `Getrs`,
`Getrf2`, `Getf2`, `Getri` and `Gesv` for `float`, `double` and their complex
counterparts, including N/T/C reusable solve modes. These and `Geequ`/`Geequb`
support both layouts, including independently selected factor/RHS layouts,
through explicit caller-owned packing. `Gecon`, `Gerfs` and explicit GESVX
FACT=N/E/F drivers add condition estimation, refinement and expert solves for
the same four scalars, with independently selected A/AF/B/X layouts. The
additional Cholesky, QR, least-squares, indefinite, LU helper and Sylvester
routes below bring the development mapping to 322 partial scalar routines.
The other 1,791 required
LAPACK routines
and shared-facet isolation remain incomplete. Native coverage is separate;
registration and scoped tests
do not close the full routine/mode/evidence manifest.

`lapack_triangular_band.h` adds S/D/C/Z `Tbtrs` with the neutral
`LapackTriangularBandView`. The descriptor supports bandwidths equal to or
greater than order, with full physical backing and explicit row/column encoding.
A/B layouts are independent, N/T/C preserve complex conjugation, and unit
coefficients and unused corners are never read. Row conversion uses caller
workspace; queries inspect metadata only. A nonunit zero-RHS solve still checks
the actual diagonal and preserves first-zero native INFO. Zero-order completion
is local with absent INFO. All structural checks precede numeric mutation.

The admitted solve candidate passes seven tests on both integer ABIs in Release,
Debug and ASC-only sanitizers, including scoped Linux protected-memory checks.
Bounded root v19 checks pass: four affected GNU suites52/52, both ASC-only
sanitizer suites7/7 and four relocated23-family packages. Provider-free static
and shared descriptor/header/package suites pass16/16 each. Zero skips. The
117-header documentation and six static checks pass. These checks retain
prior unchanged full-suite evidence explicitly; they are not a new full-suite
pass or complete routine/mode verification. Complete routine/mode evidence and platform gates remain required. Native LAPACK coverage is separate.

`lapack_triangular_band_condition.h` and
`lapack_triangular_band_error_bounds.h` add S/D/C/Z `Tbcon` and `Tbrfs` using
the checked triangular band descriptor. `Tbcon` estimates reciprocal condition
in the one/infinity norm. `Tbrfs` reports FERR/BERR for supplied X without
refining or modifying it. A/B/X layouts are independent; row conversion uses
explicit caller workspace and stays banded. Unit diagonals and unused corners
remain unread. Queries inspect metadata only and bind all strides, shapes,
flags, source integer bounds and provider identity.

The candidate's public/failure/count/signature tests pass in both ABIs under
Release, Debug and ASC-only sanitizers. Its eight required mathematical gates
fail for finite tiny/maximum inputs: zero/NaN condition estimates and infinite
forward error estimates. Direct pinned calls reproduce them. The pinned complex
LATBS dependency also omits a one-element lower-band dot product on its slow
transpose/conjugate path; direct finite equation tests fail. These remain
required mathematical gaps. Negative or unwritten INFO and negative estimates
are defects; nonfinite estimates remain visible warnings. Full v20 Release suites
on both ABIs each execute 880 tests: 810 pass and 70 required mathematical gates
fail, with zero skips. Affected Debug suites each pass 60 of 68 and ASC-only
sanitizers pass 11 of 19, retaining eight mathematical failures each. All 65
primary Core/Dense/provider translation units compile freshly in each lane.
Four relocated packages pass all 24 families; six static checks and strict
Doxygen cover 119 headers and 2,148 public members without warnings. Fresh
provider-free static/shared suites pass 16/16 each. The foreign libraries remain
unsanitized. Complete modes, stronger memory acceptance and platform gates
remain open. No native capability is added.

The general-band expert headers `lapack_general_band.h` and
`lapack_lu_band_{equilibration,condition,refinement,driver,expert}.h`
add S/D/C/Z `Gbequ`, `Gbcon`, `Gbrfs`, `Gbsv` and `Gbsvx` routes.
Compact original bands place the diagonal at KU; expanded LU storage places
it at KL+KU and retains factor fill-in. These are distinct checked descriptors.
Raw band pivots carry the sequential band-swap convention and do not certify
a successful factorization. B and X independently support both layouts with
explicit caller packing; refinement and expert drivers support N/T/C.

`Gbsvx`, `GbsvxEquilibrated` and `GbsvxFactored` select FACT=N/E/F
explicitly. Equilibration modifies only the operands documented by that
entry point. The report preserves native INFO, singular partial factors,
accuracy warnings and raw condition/error statistics. FERR is an estimate,
not a guaranteed error bound. The native-width integer workspace separates
converted pivots from IWORK, and output sentinels detect missing or partial
native writes before publication.

Required extreme-scale mathematical checks remain failing in the pinned
provider: GBEQU can return an incorrect scale ratio, GBCON can return zero
for a condition-one system, and GBRFS/GBSVX can return nonfinite error
estimates. The ordinary failing tests remain registered. Complete routine/mode/platform
acceptance remains required.

`lapack_lu_band_equilibration_radix.h` adds S/D/C/Z `Gbequb` for compact
original general-band storage. This is the exact pinned radix-quantized
operation, separately reported from `Gbequ`. Its original-maximum, extreme
scaling and scale-ratio mathematical checks still expose failures in the
prepared provider. The raw diagnostics and partial-output contract remain
visible; four callable paths do not constitute verified mathematical coverage.

`lapack_triangular.h` adds S/D/C/Z `Trtri`, `Trti2` and `Trtrs` on full checked
square Dense BLAS views. Upper/lower storage, implicit unit/nonunit diagonals,
and independently selected matrix/RHS layouts are explicit. `Trtrs` supports
N/T/C, preserves A and overwrites B; the inverse operations overwrite only
the selected triangle. Row-major calls use live scalar objects in the caller's
`kLayoutConversion` workspace. Metadata-only queries bind the original
strides, options, actual foreign dimensions and exact provider identity.

`Trtri` and `Trtrs` preserve the native positive INFO for an exact zero
nonunit diagonal, with unchanged output and a zero-based diagnostic index.
`Trti2` has no singularity scan: a zero diagonal can produce nonfinite values
with INFO=0. Its successful native completion is not an invertibility or
finite-output certificate. Zero-order calls complete locally; zero-RHS
`Trtrs` still enters the native routine and scans a nonunit diagonal.
`lapack_triangular_condition.h` and `lapack_triangular_error_bounds.h` add
S/D/C/Z `Trcon` and `Trrfs`. `Trcon` computes a reciprocal condition estimate
in the one/infinity norm. `Trrfs` estimates FERR/BERR for the supplied solution
and preserves A, B and X; it performs no iterative refinement. The matrices
may use independent layouts. Both operations use explicit scalar/real/native
integer workspace and metadata-only formula queries. Unit diagonals and the
unused triangle remain unread; complex N/T/C retains its exact meaning.

Zero-order condition estimates return one locally; zero-order or zero-RHS
error estimates return zero locally, with no fabricated INFO. Neither routine
has a positive singular-pivot INFO contract. Negative estimates are provider
defects and nonfinite estimates remain visible accuracy warnings. Required
finite-scalar tests reproduce native tiny-condition and tiny/large FERR failures;
they remain ordinary failing mathematical gates. Banded triangular
storage and remaining memory, extreme-range, platform and normalized-mode
acceptance remain separate required work.

`lapack_triangular_packed.h` adds S/D/C/Z `Tptri` and `Tptrs` using checked
`DenseBlasPackedMatrixView` storage. Upper/lower, unit/nonunit and row/column
packed orders remain explicit. `Tptrs` supports N/T/C and independent RHS
layouts. Row-packed A needs p=n*(n+1)/2 caller scalar objects; row-major B
adds n*nrhs. No dense expansion occurs. Unit diagonals remain unread and
unwritten. Nonunit singularity preserves A/B and the exact native INFO;
zero-RHS solves still perform that source-defined scan. Metadata-only queries
protect actual native packed intermediates and bind the exact provider/plan.

Historical v17 root checks pass all four new packed inverse/solve tests in each
GNU Release/Debug and ASC-only Clang19 sanitizer configuration, for both
LP64 and ILP64. They include representable extreme scales, first/last
singularity, native INFO, stale plans, live aliases, workspace and rejected
placement metadata. Each configuration freshly compiles all 62 primary
Core/Dense/provider translation units. All four relocated packages pass
their 21 installed family consumers. Twelve strict checks and six static
checks pass; Doxygen documents all 113 public headers and 2,100 members.

Each full Release suite executes 837 tests: 783 pass and 54 existing required
mathematical gates fail. Affected Debug suites pass 38 of 46; ASC-only
sanitizer suites pass 17 of 25, retaining eight existing mathematical failures
each. There are zero skips. The foreign GNU archives are unsanitized.
Complete normalized modes, stronger memory observation and platform gates
remain open. Other required packed/banded routines remain separate work.

`lapack_triangular_packed_condition.h` and
`lapack_triangular_packed_error_bounds.h` add S/D/C/Z `Tpcon` and `Tprfs`.
They use ordinary packed triangular A and explicit caller scalar, real/native
integer and layout workspace. `Tpcon` estimates reciprocal one/infinity-norm
condition. `Tprfs` reports FERR/BERR for the supplied solution and leaves
A, B and X unchanged; B/X layouts are independent. It performs no refinement.
Unit diagonals remain unread. Metadata-only queries bind actual source cursor
bounds, layouts, flags, strides, scalar and provider identity.

Zero-order condition returns one locally; zero-order/zero-RHS error estimates
return zero locally, with absent INFO. Negative or unwritten native INFO and
negative estimates are defects; nonfinite estimates remain visible warnings.
The scoped candidate passes public/fault/count/signature tests on both ABIs in
Release, Debug and ASC-only sanitizers. Its eight ordinary mathematical gates
fail on finite extreme cases, reproduced by direct pinned calls: tiny scalar
RCOND zero, tiny/huge FERR infinity, and maximum-scale packed matrix RCOND
zero or NaN. These are unresolved required gates. No numerical success is
inferred from native INFO zero.

Corrected v18 root integration freshly compiles all 63 Core/Dense/provider
translation units in each of six configurations. All four relocated packages
pass their 22 family consumers; 14 strict and six static checks pass. Doxygen
covers 115 headers and 2,116 public members without warnings. Both full
Release suites execute 853 tests: 791 pass and 62 required mathematical gates
fail. Affected Debug passes 46 of 62, and ASC-only sanitizer passes 21 of 37,
each retaining 16 required mathematical failures. There are zero skips.
The foreign archives remain unsanitized. Complete modes, stronger memory
observation, concurrency and platform acceptance remain open.

Historical bounded v16 verification covers both actual LP64 and ILP64 static providers.
Each full GNU Release suite executes 831 tests: 773 passes, 54 required
mathematical failures and 4 header-count failures. The corrected header oracle
subsequently passes all 4 checks and 6 fixture setups in each ABI. Each affected
Debug suite records 28/40 passes with the 8 estimator math failures and the
4 corrected header failures; each ASC-only ASan/UBSan suite records 13/21
passes with the 8 math failures. There are zero skips. All four Release/Debug
relocated installations pass 20 family consumers; 14 strict and 6 static checks
pass. All 61 primary ASC
Core/Dense/provider TUs are freshly compiled per numerical lane. The foreign
archives remain unsanitized. The header-only test correction changes no
production/header/compiler input; no new full suite is claimed for that
corrected snapshot. These results establish bounded integration evidence,
while the finite-extreme and full routine/mode gates remain open.

The extra LU operations are declared in `asc/dense/providers/lapack_lu.h`;
equilibration is in `asc/dense/providers/lapack_lu_equilibration.h`. GETRI uses
an actual nonmutating workspace query on the supplied raw LU/pivots, with
explicit integer conversion storage. Execution consumes the checked plan
without querying again. Singular GETRI preserves its input; singular GESV
preserves completed raw LU/pivots and leaves the RHS unchanged.

Row-major conversion uses the plan's `kLayoutConversion` region containing
live scalar objects, disjoint from every operand and other workspace. Its
count is ASC-sized, not foreign LWORK. Each matrix contributes rows*columns;
all products and simultaneous byte totals are checked. Packing preserves the
original mathematical orientation, and only defined outputs are unpacked.
Padding, const factors and failed structural-validation destinations remain
unchanged. Actual foreign leading dimensions are ABI-bounded; original ASC
strides remain part of plan identity without unnecessary foreign narrowing.

Known provider limitation: pinned GNU LP64 GEEQUB can produce a zero computed
radix scale from nonzero subnormal input. ASC preserves exact INFO, returns
`ErrorCode::kNumerical` with `LapackOutcome::kPartialResult`, and does not
certify singular input or successful equilibration. The same fixture succeeds
in the separately verified true ILP64 provider; the LP64 mathematical-success
gate remains unmet. GEEQUB's pinned AMAX is radix-quantized, unlike GEEQU's
original maximum. Public declarations document exact partial-output validity.

## Condition estimation, refinement and expert drivers

The dedicated provider headers `lapack_lu_condition.h`,
`lapack_lu_refinement.h` and `lapack_lu_driver.h` expose these distinct checked
routes; none changes the native or default provider-free API.

`Gecon` takes immutable raw square LU and a caller-supplied finite original
one/infinity norm. It does not need pivots. Complex matrix norms use Euclidean
scalar modulus. `Gerfs` takes immutable original A, matching raw LU/pivots,
original B and a separate mutable initial X. It supports N/T/C and returns
underlying-real FERR/BERR. A supplied factor's original/scaled provenance is a
caller obligation; these interfaces do not invent a successful factor view.

`Gesvx` selects FACT=N and preserves original A/B. `GesvxEquilibrated` selects
FACT=E with explicitly mutable A/B and R/C/EQUED outputs. `GesvxFactored`
selects FACT=F with immutable already-scaled A/factors/pivots/used scales and
explicitly mutable original B. X always describes the original system;
RCOND describes the equilibrated matrix. Statistics also retain reciprocal
pivot growth. Unused supplied scales are not read. Output-only AF/X are not
read during row-major packing.

These workspace queries evaluate checked formulas without foreign calls.
Regions distinguish scalar, underlying-real, foreign-width integer and
ASC-sized layout capacities. The integer region contains foreign pivots and
any IWORK; it is not the ASC-index-width pivot-conversion role. All operands
and live regions must be disjoint. There is no implicit allocation, transfer,
precision conversion or provider fallback.

Raw GESVX INFO=1..n preserves completed singular factors/scaling and the
source-defined diagnostics, leaving X/FERR/BERR unchanged. INFO=n+1 is an
accuracy warning with a completed solution. Nonfinite error/statistic outputs
are retained with numerical warnings; negative outputs are provider defects.
Arbitrary nonfinite inputs or unrepresentable solutions have no finite-X
promise. GERFS similarly preserves nonfinite estimates with an accuracy warning
even when upstream INFO=0; it checks exact-zero U before refinement.

Required mathematical limitations remain explicit: tiny finite unscaled scalar
systems can overflow GERFS's intermediate inverse application and produce
nonfinite FERR; GESVX FACT=N/F can additionally return RCOND=0 despite exact
condition one. Both ABIs reproduce these limitations. Explicit FACT=E succeeds
on that recorded fixture, but is never substituted implicitly. Passing fidelity
tests do not close finite-condition/error mathematical gates. Full normalized
mode/evidence closure remains required; this development profile is not full
capability. Original LU now checks source-specific foreign loop/cursor
arithmetic and floating workspace-query conversions before foreign entry.

## Cholesky, QR and LU helpers

`lapack_cholesky.h` exposes S/D/C/Z `Potrf`, `Potrf2`, `Potf2`, `Potrs`,
`Potri` and `Posv`. Only the selected upper/lower triangle is packed or
published, with independent A/B layouts and explicit caller storage. Complex
Hermitian factorization ignores imaginary input diagonals; triangular solves
and inverse consume actual factor components. Failed leading minors preserve
documented partial factors, never a successful factor certificate. `Potri`
returns the selected triangle of the inverse, not a reusable factor.

`lapack_qr.h` exposes S/D/C/Z `Geqrf`/`Geqr2`, real `Orgqr`/`Ormqr` and
complex `Ungqr`/`Unmqr`. The generation/application experts accept raw partial
reflectors, not just complete factors. They support both layouts, left/right
application, real N/T and complex N/C. Actual foreign workspace queries are
distinguished from GEQR2's formula-only query. Caller-owned scalar WORK and
layout storage are separate; output-only upper entries/extra Q columns are
not read while packing reflector tails.

`lapack_lu_helpers.h` exposes S/D/C/Z `Laswp` and `Laqge`. LASWP uses raw
one-based sequential swaps, with explicit slot spacing and forward/reverse
application. LAQGE consumes explicit scales/statistics and reports the actual
N/R/C/B branch. Neither underlying routine has INFO, so an actual call never
fabricates one. The source's C*R*A order can overflow for a finite tiny input
whose exact scaled result is one. The adapter preserves those raw outputs
with a numerical accuracy warning; this fidelity is not mathematical success.

All nonempty workspace regions must be admitted by the explicit provider
context in addition to neutral capacity/alignment validation. The Serial
context admits host storage, not an implicit pinned/device/managed transfer.
The original LU routes retain unused zero-byte workspace compatibility.
The provider-free [native Cholesky/QR contract](contracts/lapack-native-cholesky-qr.md)
is separate and does not credit the reference Q experts to native coverage.

The committed v7 product matches tree `a7aa1b35798b6e7062be22990895360232fb4ef4`
outside program records. It passed full LP64/true-ILP64 415-test suites, provider-free Debug/Release
277-test suites and shared provider-free Release 279 tests, with zero skips.
Each installed provider package executed ten consumers. Its affected
Clang19 ASan/UBSan suites passed 46 tests per ABI with Core/Dense/reference
C++ instrumented; Fortran/BLAS archives and dynamic runtimes were not.
Those historical results do not verify V9. The corrected Sylvester admission
and new indefinite expert/SVD least-squares routes require fresh combined-tree
and installed checks.

## Positive-definite band factorization and solves

`lapack_cholesky_band.h` exposes actual S/D/C/Z `Pbtrf`, `Pbtf2` and
`Pbtrs` with explicit positive-definite band descriptors. Both triangles and
layouts preserve band-sized caller packing, unused corners and padding; no
dense expansion occurs. Complex original imaginary diagonals are not read.
On positive factorization INFO, row-major publication updates only the
imaginary diagonal components actually normalized by the source's completed
factor/update path, leaving untouched trailing components unwritten. Raw
PBTRS factor coefficients are not normalized as original Hermitian input.
There is no native band coverage claim or complete structured-family claim.

## Classic symmetric and Hermitian indefinite factors

`lapack_indefinite.h` separately exposes classic S/D/C/Z SYTRF/SYTF2/SYTRS
and C/Z HETRF/HETF2/HETRS. The optional provider-specific borrowed factor
retains symmetric versus Hermitian operation, triangle, actual originating
routine and checked signed paired pivots; it is not an LU swap list or a
generic LDL certificate. Original Hermitian factor inputs use explicit n*n
selected-component packing in either layout, while raw factor coefficients
retain their defined complex values. Positive factorization INFO preserves
completed factors/pivots but does not certify a successful reusable factor.
These eighteen routes have passed the recorded v7 numerical and installed
checks; full normalized routine/mode coverage remains incomplete.

## Rank-revealing QR and least squares

`lapack_rank_revealing.h` exposes S/D/C/Z GEQP3 and GELSY, with separate
fixed-column input flags and checked one-based final column permutations.
The provider-free permutation validators/converter reject LU and block-pivot
encodings. GEQP3 reports a column-pivoted factor family, not an unpivoted QR
certificate. GELSY preserves every finite RCOND value and returns the source's
leading-block rank decision, not a singularity or original-matrix rank proof.

Actual foreign queries and checked source-specific execution minima account
for nested workspace validation and INTEGER cursors. All packing, scalar,
underlying-real, foreign-integer and ASC64 conversion storage is caller-owned.
Zero-row GEQP3 still enters the provider with a backed n-scalar surrogate;
GELSY reads only B's m input rows and publishes only its n solution rows.
The known forced-zero-column GELSY failure remains a required mathematical
gate: INFO=0 and faithful rank-zero output do not establish optimal residuals
or minimum norm. Other QR/orthogonal, least-squares and SVD families remain
required; these eight routes do not close P06.

## Cholesky expert drivers and least squares

`lapack_cholesky_condition.h`, `lapack_cholesky_refinement.h`,
`lapack_cholesky_driver.h` and `lapack_cholesky_equilibration.h` expose actual
S/D/C/Z `Pocon`, `Porfs`, explicit POSVX FACT=N/E/F, `Poequ` and `Poequb`.
They preserve selected-triangle semantics and independently selected operand
layouts. Complex original matrices ignore imaginary diagonals; supplied raw
factor coefficients are not normalized. POSVX N/E uses explicit selected-entry
packing even for column-major complex A so ignored components are not read.
Only an actual EQUED=Y publishes equilibrated A/B. FACT=F keeps already-scaled
A/AF and used scales immutable, while X solves the original system.

Caller regions separate scalar, underlying-real, provider-width integer and
layout storage. Condition estimates use the supplied original one norm;
refinement retains original A/B and a separate mutable X. Negative/invalid
foreign diagnostics are provider defects; positive INFO and nonfinite error
estimates retain documented partial outputs or accuracy warnings. An expert
driver's completed raw AF does not invent a successful reusable factor view.

`lapack_least_squares.h` exposes each actual S/D/C/Z `Gels`, `Gelst` and
`Getsls`, not one substituted algorithm. Real N/T and complex N/C cover tall
least squares and wide minimum norm under each routine's full-rank assumption.
B must provide max(m,n)-by-nrhs capacity, with explicit packing of input rows
only. GELS/GELST publish their documented residual coordinates; GETSLS publishes
only solution rows and leaves other B rows unchanged. Positive INFO preserves
the documented partial factor/RHS state without a rank or factor certificate.
Actual query calls are nonmutating and separated from execution; GETSLS uses
its distinct minimum and preferred queries. Counts also bound nested integer
arithmetic, floating workspace metadata and strided BLAS terminal cursors.
These checked routes do not promise rank-revealing or constrained least squares.

## Scaled ordinary Sylvester equations

`lapack_sylvester.h` exposes actual S/D/C/Z `Trsyl` and formula-only workspace
queries. Inputs must already be upper Schur forms; no Schur reduction is
selected implicitly. The equation is `op(A) X + sign X op(B) = scale C`,
with real N/T/C or complex N/C, both signs and independent A/B/C layouts.
Real nonzero subdiagonals must define nonoverlapping canonical 2x2 blocks.

Both A/B layouts use explicit caller-owned full-square packing, filling
ignored lower entries with zero because the source's max norm reads full
matrices. Row-major C needs a separate caller buffer; column-major C is
direct. Scale is never divided out. INFO=1 preserves X and scale with an
accuracy warning, without certifying the unperturbed equation. Invalid INFO
or scale withholds packed C and scale, marking direct C unusable. Empty
execution sets scale to one without a foreign call or fabricated INFO.

Finite diagonal coefficient sums outside the scalar component range are
rejected before packing or foreign entry, with `kNumerical`, unchanged
numerical buffers/workspace and absent INFO. The pinned provider can otherwise
return an incorrect finite solution with INFO=0 even for a well-conditioned
scalar equation. The required finite-large mathematical mode remains
incomplete; this check neither rescales inputs nor substitutes another driver.

Scoped real-provider mathematical, guarded ABI, allocation, sanitizer,
strict/header and documentation checks have passed separately. Combined
current-tree and installed-package checks remain pending. TRSYL3, generalized
TGSYL, all other spectral families and complete per-mode evidence remain
required; this four-route slice does not close P08.

## Building the explicit subset

The default is `ASC_CPP_ENABLE_LAPACK=OFF`. The current private ABI proof is
restricted to Linux x86-64, GCC/GFortran 11.4.0 and the recorded libstdc++ build;
unknown configurations are rejected. Configure a static build using an exact
Reference-LAPACK 3.12.1 prefix and its independently generated build attestation:

```sh
cmake --preset test-lapack-lp64 \
  -DASCCMake_DIR=/path/to/pinned/ASCCMake/package \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared/lp64/prefix \
  -DASC_CPP_LAPACK_ATTESTATION=/path/to/provider-attestation.json \
  '-DASC_CPP_LAPACK_RUNTIME_LIBRARIES=/absolute/libgfortran.so.version;/absolute/libquadmath.so.version' \
  -DCMAKE_Fortran_COMPILER=/path/to/audited/gfortran
cmake --build --preset test-lapack-lp64
ctest --preset test-lapack-lp64 --no-tests=error
```

Use `test-lapack-ilp64` with a separately built **true** ILP64 prefix and
attestation, not LP64 libraries with a typedef changed. The configuration checks
the exact source-input manifest, installed file digests, integer ABI and linked
language/library probe. Compiler-driver paths and flags needed by a prepared
local toolchain must be supplied at the initial configuration.

`test-lapack-full` sets `ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE=ON` and fails until
every required inventory row, mode and evidence gate is complete.
`test-lapack-shared` currently fails the outstanding shared-isolation gate.
Neither preset is a successful verification lane merely because it exists.

## Installed C++-only consumers

ASC installation installs only ASC's facet, headers and relocatable dependency
metadata. It does **not** install or redistribute the upstream archives, source
or runtimes; separate owner/license approval has not been assumed. The user
supplies a prepared dependency prefix and the exact runtime files:

```cmake
project(MySolver LANGUAGES CXX)
set(ASC_CPP_LAPACK_ROOT "/path/to/prepared/prefix")
set(ASC_CPP_LAPACK_RUNTIME_LIBRARIES
  "/absolute/libgfortran.so.version;/absolute/libquadmath.so.version")
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_lapack)
target_link_libraries(my_solver PRIVATE ASC::dense_lapack)
```

Installed discovery enables neither C nor Fortran, invokes no Python, downloads
nothing and searches for no system BLAS/LAPACK. It verifies the prefix-relative
archive hashes and explicit runtime hashes before creating the private link
dependency. An optional unavailable facet is reported not found without making
a valid base component unavailable. An unrequested facet is never inspected.
The profile and upstream build identity are available as
`ASCCpp_LAPACK_PROFILE` and `ASCCpp_LAPACK_PROVIDER_ID`.

Calls require `ReferenceLapackProvider`, a matching query plan, caller-owned
integer workspace and a failure-surviving `LapackReport`. Factor storage and raw
one-based ASC pivots remain borrowed; LP64/ILP64 conversion uses checked caller
scratch. No global provider selection or fallback exists. See the public header
documentation and [LU contract](contracts/lapack-general-lu.md).

The [installed array-I/O example](../examples/lapack_array_io/) demonstrates
factor-once/two-RHS reuse, residual checking, INFO reporting, printing,
save/reload and failed staged-read rollback. The numerical and allocation
evidence scope, including the limits of static linker instrumentation, is
recorded in the program's private-ABI review and durable verification ledger.

## Classic indefinite expert systems

`lapack_indefinite_condition.h`, `lapack_indefinite_refinement.h` and
`lapack_indefinite_driver.h` expose S/D/C/Z `Sycon`, `Syrfs`, `Sysv` and
`Sysvx`, plus complex `Hecon`, `Herfs`, `Hesv` and `Hesvx`. Symmetric complex
routines retain transpose symmetry; Hermitian routines retain conjugate
symmetry. Each route consumes the selected triangle and classic paired raw
pivots. A, AF, B and X layouts are independent.

`Sysv`/`Hesv` publish selected factors/pivots and overwrite B. Refinement
preserves original A, factors, pivots and B while improving caller X. Expert
N drivers output AF/pivots; explicitly named factored F drivers preserve them.
Both expert modes preserve A/B and return X, real FERR/BERR and RCOND.
INFO=n+1 reports an accuracy warning. A raw factor's provenance remains the
caller's responsibility; a driver report does not certify a TRF factor view
or distinguish N from F by its routine name.

Formula queries bind modes and metadata without allocating or foreign entry.
Real condition/refinement paths reserve separate live native-width pivot and
estimator segments within integer workspace. Original Hermitian input ignores
imaginary diagonals; meaningful factor components are consumed as stored.
Singular-factor and malformed-return publication follows each documented
route rather than a common success assumption. Zero-RHS drivers still perform
the source-defined factorization/condition work.

## SVD least squares

`lapack_svd_least_squares.h` exposes S/D/C/Z `Gelss` and `Gelsd` with explicit
minimum/preferred workspace. B must hold max(m,n) rows, including the solution
capacity. The caller receives rank, underlying-real singular values and the
actual overwritten A/B outputs. Separate queries and execution bind scalar,
provider ABI, layouts, dimensions, strides and rank threshold.

The pinned source has required modes whose mathematical or return-safety
gates remain incomplete. Nonzero-A GELSD with zero RHS reaches a terminating
upstream error handler and is rejected before mutation; safe all-zero A and
empty shapes retain their distinct paths. Nonzero single-real/complex GELSD
with min(m,n)>=212992 is rejected because the source's compressed divide-tree
storage can be too small. Meaningful nonfinite A is also checked before
unsafe source entry. The corrected wide-real minimum accounts for the actual
source workspace accesses. GELSS's wide high-workspace path leaves LQ data
in A despite the upstream right-vector output description; ASC publishes that
actual raw output and does not certify it as right singular vectors.

These eight routes have scoped solution, singular-value, residual-optimality
and minimum-norm tests. They do not close those source failures or the full
routine/mode contract. All remaining LAPACK families and optional-upstream
dependencies remain required for the full reference profile.


The general-band LU routes `Gbtrf` and `Gbtrs` support all four scalars.
`LapackLuBandView` uses column-major `2*KL+KU+1` factor storage, with
`KL+KU` as the zero-based diagonal row. `ReferenceLuBandFactorView` validates
an actual successful matching GBTRF report and the complete one-based band
pivot encoding. It is a separate factor family; rectangular factors can be
inspected, while reusable N/T/C solves require square factors. RHS layouts
are independently selected using explicit caller-owned packing when needed.
The adapter checks pinned-source integer intermediates and detects unwritten
or invalid native INFO/pivot outputs before publishing converted pivots or
packed RHS results. These eight routes remain in progress until their full
normalized mode and evidence requirements are closed.


The general-tridiagonal headers `lapack_tridiagonal.h`,
`lapack_tridiagonal_condition.h`, `lapack_tridiagonal_refinement.h` and
`lapack_tridiagonal_driver.h` expose actual S/D/C/Z `Gttrf`, `Gttrs`, `Gtsv`,
`Gtcon`, `Gtrfs` and explicit FACT=N/F `Gtsvx` routes. Tridiagonal LU retains
its second superdiagonal and adjacent-swap pivot encoding. GTSV destructive
output is separate from reusable GTTRF factors. Reused factors and raw expert
pivots have nominal types, checked again during execution.

Plans include caller-owned native pivot and estimator integers in disjoint
parts of `kInteger`, plus explicitly requested RHS/solution packing. The
pinned real GTSV zero-RHS path accesses a first RHS internally: the plan
provides and initializes its scalar scratch while passing actual NRHS=0.
No hidden allocation, implicit densification or alternate solve is involved.
Native INFO and output pivots start with full-width invalid sentinels, so
missing or partial native writes cannot become success or reusable factors.

Required extreme-scale GTCON/GTRFS/GTSVX mathematical tests remain failing:
for the representable singleton at min-normal/8, the pinned provider reports
zero reciprocal condition or infinite error estimates. Those tests remain
ordinary failures and the corresponding required modes remain incomplete.


The five `lapack_cholesky_band_*` expert headers add actual S/D/C/Z `Pbsv`,
`Pbequ`, `Pbcon`, `Pbrfs` and `Pbsvx`. Each consumes the selected positive-
definite band directly, with explicit caller packing for independently chosen
band and RHS/solution layouts. PBSVX exposes factorization, equilibration and
factored modes separately. For a reused equilibrated factor, the caller supplies
its already-scaled A and scale metadata according to the factored declaration;
there is no implicit equilibration or alternate-provider fallback.

These declarations preserve directly bound native outputs and report their
validity separately. Packed factor/solution outputs are published only after
an admissible native INFO. Unwritten INFO retains a full-width invalid sentinel
and produces a provider error. Queries, descriptors, plans, scratch capacities,
aliases and host placement are checked before provider execution.

Required band-expert mathematical gates remain incomplete: complex lower
scaled PBCON can return an incorrect condition estimate; tiny PBRFS/PBSVX can
return infinite error estimates or zero condition; the pinned PBSVX
multiply order can overflow during explicit equilibration. Ten ordinary test
failures retain those required modes in the coverage backlog.
