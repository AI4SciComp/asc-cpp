# S/D/C/Z GBTF2 continuation

Status: bounded local implementation and engineering verification complete;
mathematical acceptance blocked. No full-family or full programme completion
is claimed.

The four required GBTF2 rows had no public implementation at `bdfaeb8`.
The existing GBTRF/GBTRS implementation is retained. This continuation adds
explicit `QueryGbtf2Workspace` and `Gbtf2` overloads and accepts their actual
successful provenance in `ReferenceLuBandFactorView`. The selected routine
is always GBTF2; GBTRF keeps its existing provider-selected blocked route.
The factor view's representation, stored metadata and ownership are unchanged.

## Pinned contracts

The review uses Reference LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, and the existing generated inventory.
The exact sources are
[SGBTF2](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/sgbtf2.f),
[DGBTF2](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dgbtf2.f),
[CGBTF2](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/cgbtf2.f) and
[ZGBTF2](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/zgbtf2.f).

| Contract | ASC mapping |
| --- | --- |
| Scalar | float, double, complex<float>, complex<double> respectively; complex general matrices have no Hermitian assumption |
| Dimensions | Rectangular m-by-n, nonnegative KL/KU; empty dimensions retain the actual native quick return |
| Storage | Existing column-major `LapackLuBandView`; zero-based A(i,j) at KL+KU+i-j+j*LDAB; LDAB >= 2*KL+KU+1 |
| Fill | Top KL rows reserve U fill; U has KL+KU superdiagonals; lower stored values are elimination multipliers |
| Pivots | min(m,n) signed one-based step swaps, j+1 <= IPIV[j] <= min(m,j+KL+1); interleaved swaps are not a dense GETRF permutation |
| UPLO/transpose | No GBTF2 option arguments; successful square factors feed the existing N/T/C GBTRS solve with independent RHS layout |
| INFO | Zero means native completion; positive i records exact zero U(i,i) and completed raw factors, with zero-based diagnostic i-1; negative, impossible or omitted values are provider failures |
| Workspace | No native WORK argument, workspace query or local work array; ASC requires min(m,n) aligned native INTEGER entries for checked pivot conversion |
| Aliasing | Operands, workspace, provider/plan/report metadata are checked disjoint before mutation; unsafe report aliasing preserves the report |
| Mutation | Native factorization overwrites band storage; padding is preserved, source-defined fill corners may change; checked pivots publish only after native INFO/pivot validation |

The scalar matrix, integer pivots and all metadata remain borrowed. Queries
read metadata without touching coefficients or pivot values. Native failure
cannot roll back directly supplied band storage. Positive INFO cannot create
a successful reusable factor. INFO zero is not a finite-factor certificate.
There is no implicit equilibration, inverse, densification or provider change.

## ABI and source arithmetic

GBTF2 is absent from the pinned installed `lapack.h`. Private declarations
follow eight untouched GFortran 11.4 `-fc-prototypes-external` emissions,
with `-fdefault-integer-8` for true ILP64. There is no CHARACTER parameter or
hidden length. The admitted integer width, symbol mangling and C++ complex
representation remain the existing provider contract. Both C++ syntax probes
include the untouched compiler emissions alongside ASC's private declarations.
Source and emission identities are external in
`master-continuation-20260910-01/continuation-20260912-01/gbtf2-contract-01`.

The existing checked band arithmetic now selects unit-step GBTF2 bounds
explicitly. Existing GBTRF admission and its blocked cursor/pivot-row bounds
are unchanged. KV=KU+KL and KL+KV+1 must fit before the native empty return.
For nonempty calls KU+2, terminal DO cursors, J+KV and J+KU+JP before the
subsequent subtraction are bounded. The existing SWAP/GER bound covers the
last nonunit cursor update, 1+length*(LDAB-1), as well as each accessed index.
Pure limit tests exercise both actual signed integer maxima without creating
imaginary huge operand spans. GBTF2 does not inherit GBTRF's step-32 bound.

## Completed bounded validation

The first LP64 four-scalar numerical subset passes. Maintained tests now run
their independent full-matrix reconstruction and N/T/C solve oracles for both
factorization entries, with unchanged tolerances and assertions. Coverage
includes rectangular and empty matrices, wide bands, forced noncommuting
pivots, large/small scales, exact singularities, source fill and padding,
independent RHS layouts, repeated solves, metadata rejection, aliases,
workspace corruption, native INFO faults and invalid/partial-width pivots.

The direct ABI probe and maintained relocated installed example have explicit
GBTF2 modes. A separate range test uses a scaled identity with exact zero
multiplier. Its mathematical assertions require finite exact factors and
independent reconstruction; direct native fidelity is reported separately.
The source computes a reciprocal before scaling that multiplier. Both actual
integer ABIs reproduce nonfinite multipliers at min-normal/1024 and twice the
smallest positive subnormal. The twice-min-normal control passes. Each scalar
fails six assertions: finite multiplier, exact zero multiplier and independent
reconstruction at both small scales. Native INFO remains zero and direct
provider outputs match. This is a pinned-provider numerical limitation, with
one shared reciprocal-before-SCAL cause, rather than an invalid exact oracle,
an ASC scalar mapping defect or a reason to relax tolerances. The explicit
Reference route preserves the pinned provider; no hidden scaling or algorithm
replacement is introduced. These four required mathematical tests remain live.

Every static Debug, static Release, static ASC ASan/UBSan and shared Release
profile, in both LP64 and true ILP64, has **33/37 passes, four mathematical
failures, 24 failed assertions and zero skips**. The 33 passes include all
preexisting GBTRF/GBTRS tests selected by this dependency change, the new GBTF2
ordinary/failure/boundary cases, both actual ABI probes, source-limit checks,
four direct range comparisons and both public-header modes. The foreign
provider is uninstrumented in the sanitizer profiles. No shared Debug,
shared sanitizer, TSan, hosted CodeQL or wider platform result is inferred.

All four independently installed, relocated static/shared LP64/ILP64 public
consumers pass 1/1 with zero skips. Their metadata, include paths and runtime
dependencies are isolated from the source and producer trees. The shared ABI
audit preserves every old strong ASC export and adds exactly eight symbols:
four queries and four executions. Sixteen producer/test strict checks and two
installed-consumer strict checks pass. Doxygen covers 140/140 headers and
2,422 public members with zero warnings.

Raw records are under the existing external continuation directory:
`gbtf2-{static,shared}-{mode}-{abi}` (static Release uses suffix `-final`),
`gbtf2-installed-{linkage}-{abi}`, `gbtf2-strict-{abi}` with the final limit/range
companion `gbtf2-style-final-{abi}`, `gbtf2-consumer-abi-{abi}` and
`gbtf2-documentation-final`. The final audit binds these results to current
inputs. The pure-limit test's added checks were extracted into a separate
function to satisfy the strict size rule; all assertions remain, with final
static/shared Release limit companions retained. Initial missing-target,
unused test-header constant and Doxygen-link failures are preserved too.

The current contract index adds four source-derived reviewed GBTF2 contracts
and updates 28 existing GB dependency indexes through a schema-2 baseline
bridge. Historical executions and native20 remain unchanged. The inventory
still has 2,113 required rows: 80 implemented-unverified, 326 in progress and
1,707 not started; zero Reference rows have full verification credit. These
four new APIs are recorded in progress because their mathematical/full-profile
gates remain open. Existing PTTRS and general-solver numerical blockers,
XBLAS dependencies and wider provider/platform obligations remain open.

Next: finish the existing GBTRF/GBTRS normalized contracts and classify their
inherited range limitation using the completed ordinary/failure evidence.
