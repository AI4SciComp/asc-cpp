# Explicit Reference-LAPACK development facet

`ASC::dense_lapack` is an optional Dense-owned facet, not a seventh module.
`ASC::dense`, `ASC::cpp` and every other base component remain provider-free.
The `incremental-factorizations-v5` facet implements checked `Getrf`, `Getrs`,
`Getrf2`, `Getf2`, `Getri` and `Gesv` for `float`, `double` and their complex
counterparts, including N/T/C reusable solve modes. These and `Geequ`/`Geequb`
support both layouts, including independently selected factor/RHS layouts,
through explicit caller-owned packing. `Gecon`, `Gerfs` and explicit GESVX
FACT=N/E/F drivers add condition estimation, refinement and expert solves for
the same four scalars, with independently selected A/AF/B/X layouts. The
additional Cholesky, QR and LU helper routes below bring the development
mapping to 92 partial scalar routines. The other 2,021 required LAPACK routines
and shared-facet isolation remain incomplete. Native coverage is separate;
registration and scoped tests
do not close the full routine/mode/evidence manifest.

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

The prior v4 product tree `eebeb3a3632e66f9d07d77dc3f522cb88dc404f1`
passed full LP64/true-ILP64 315-test suites and provider-free Debug/Release
271-test suites with zero skips. Those results do not verify this newer v5
source. Frozen native/provider diagnostics are recorded separately; the
combined v5 and new installed consumers remain integration gates.

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
