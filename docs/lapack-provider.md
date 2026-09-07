# Explicit Reference-LAPACK development facet

`ASC::dense_lapack` is an optional Dense-owned facet, not a seventh module.
`ASC::dense`, `ASC::cpp` and every other base component remain provider-free.
The `incremental-lu-v2` facet implements checked column-major `Getrf`, `Getrs`,
`Getrf2`, `Getf2`, `Getri` and `Gesv` for `float`, `double` and their complex
counterparts, including N/T/C reusable solve modes. `Geequ` and `Geequb`
also support row-major input through explicit caller-owned packing. All other
required LAPACK routines, remaining layout routes and shared-facet isolation
remain incomplete. Native coverage is separate; registration and scoped tests
do not close the full routine/mode/evidence manifest.

The extra LU operations are declared in `asc/dense/providers/lapack_lu.h`;
equilibration is in `asc/dense/providers/lapack_lu_equilibration.h`. GETRI uses
an actual nonmutating workspace query on the supplied raw LU/pivots, with
explicit integer conversion storage. Execution consumes the checked plan
without querying again. Singular GETRI preserves its input; singular GESV
preserves completed raw LU/pivots and leaves the RHS unchanged.

Known provider limitation: pinned GNU LP64 GEEQUB can produce a zero computed
radix scale from nonzero subnormal input. ASC preserves exact INFO, returns
`ErrorCode::kNumerical` with `LapackOutcome::kPartialResult`, and does not
certify singular input or successful equilibration. The same fixture succeeds
in the separately verified true ILP64 provider; the LP64 mathematical-success
gate remains unmet. GEEQUB's pinned AMAX is radix-quantized, unlike GEEQU's
original maximum. Public declarations document exact partial-output validity.

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
