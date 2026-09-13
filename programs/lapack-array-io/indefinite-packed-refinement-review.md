# Packed Bunch--Kaufman iterative-refinement contract

The six SPRFS/HPRFS routes extend condition checkpoint
`1332f35ce673e4202e2a3b76b17bc5cac0df2dfa`. The adapters, public queries,
maintained tests and installed consumer are implemented. Engineering validation
is complete; all six routes remain numerically unverified. The sixteen profiles
select 552 processes: 480 pass, 72 mandatory mathematical failures, zero skips.
All six required extreme-input mathematical tests remain failed. This is an
active continuation of the whole programme, not a completion claim.

## Exact source and public scope

S/D/C/Z SPRFS and C/Z HPRFS use the pinned Reference-LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. The six inventory source instances,
argument-contract hashes and twelve actual GNU LP64/global ILP64 emissions are
recorded under `completion-execution-01/packed-refinement-source-review-01`
and `packed-refinement-emissions-01`. No source, dependency or notice changes
are made. There are twelve new public declarations and 192 reviewed legal
triangle/original/factor/RHS/solution-layout combinations, across six routes.

AP, matching AFP, exact-N classic paired/directional pivots and B are immutable.
Execution improves caller initial X and computes contiguous real FERR/BERR
vectors of exactly NRHS entries. Factors and pivots must come from a matching,
completed nonsingular SPTRF/HPTRF call on the same original system and provider.
The RHS and initial solution must belong to that system. Raw views cannot
prove provenance or certify present contents.

Original and factor packed layouts and B/X dense layouts are independently
row-major or column-major. Symmetric transpose and Hermitian adjoint semantics
remain separate. Original Hermitian imaginary diagonals are ignored, including
NaNs. Row packing normalizes only those original imaginary diagonals; actual
factor coefficients retain their meaning. Column-major original input stays
direct after the source residual-loop audit.

## Plans, storage and mutation

Queries inspect metadata only. Plans bind provider, scalar, routine, dimensions,
triangle, symmetry, all four layouts, original and effective dense strides,
exact pivot count, and both error-vector lengths and unit increments. They do
not bind buffer addresses or numerical values. Empty order or zero RHS is a
successful noncall: no scratch, provider, INFO or numerical input reads; X is
preserved and live error outputs receive zero.

Active real native WORK contains 3N scalar entries; complex WORK contains 2N.
N converted native INTEGER pivots coexist with real-only N IWORK. Complex
native RWORK contains N real entries. Both variants additionally use two
NRHS-sized private real error buffers in caller-owned `kReal` workspace,
after complex RWORK when present. Every private error entry starts as NaN.
These buffers detect omitted writes without exposing the caller's old errors.

Row conversion storage is the sum of packed AP, packed AFP, logical B and
logical X counts, in that order. These regions are simultaneously live.
Scalar, real and packing objects already exist; execution placement-constructs
native INTEGER objects at the admitted ABI width. Checked arithmetic covers
the full N(N+1) product before division, N+1, NRHS+1, LACN2's 3N and the inner
TRS one-compact-RHS 1+N endpoint. Entry and byte totals are checked separately.
Physical dense spans are proved by descriptors. RFS does not introduce a
caller NRHS*LDB bound absent from its actual per-column source calls.

Operands, workspace regions and live provider/plan/workspace/report metadata
must be accessible and disjoint. Structural rejection preserves numerical
operands and scratch. Unsafe metadata aliasing preserves the report too.
All signed pivot entries, adjacent equal-negative pairs, bounds and
triangle-specific directions are validated before conversion or numerical
mutation, including safe treatment of the minimum signed integer.

Exactly zero evaluated native TRS 1-by-1 or 2-by-2 divisors reject with
numerical/singular and a zero-based block index before mutable work. RFS does
not use CON's successful singular zero-estimate path. Full-width
minimum-seeded INFO must return zero, and private native pivots must remain
unchanged, before any caller error estimate is published.

INFO or pivot defects preserve caller FERR/BERR and row-major X; directly
passed column-major X may retain native effects. After protocol validation,
the actual native errors are published. Negative estimates, including
negative infinity, are provider-invalid/unusable and withhold row X.
Otherwise nonfinite estimates retain numerical/accuracy-warning and
documented-partial output validity, publishing logical row X. Finite
nonnegative estimates, including negative zero, subnormals and values above
one, are complete. No estimate is clipped. NaN/Inf X alone does not imply
an error-estimate protocol defect: this API makes no universal X-finiteness
or verified error-bound promise and issues no factor certificate.

## Maintained checks and retained numerical failures

Ordinary mathematics and separately guarded native fidelity each cover 576
cases per scalar/symmetry class: N=0/1/2/5/17/65, NRHS=0/1/3, both triangles
and all sixteen layouts, with sampled dyadic scales and diagonal/paired
systems. Wide arithmetic independently checks known solutions, residuals,
FERR plausibility and BERR. No-allocation checks include query and execution.

The mandatory range suite covers 512 admitted finite-input cases per class
and mode: scalar or paired order, both triangles, every layout combination,
and eight scales from min-normal/8 through 0.75*maximum. No inputs are
excluded. The first BERR oracle incorrectly omitted source safe-minimum
terms. That failed attempt is preserved. The corrected independent oracle
uses SAFE1=(N+1)*TINY and SAFE2=SAFE1/(epsilon/2), as established from the six
RFS sources and the pinned SLAMCH/DLAMCH routines. Independent unregularized
solution-quality tests, tolerances and input cases were retained.

After that correction, both initial Release ABIs still fail mathematical
checks on 96 groups for each real variant and 144 for each complex variant,
out of 512 per class. Tiny and very large inputs retain nonfinite estimates
or failed independent solution quality. All six native-fidelity tests pass.
These are recorded observations, not an asserted general root-cause proof.
The provider is unchanged, and these mandatory failures are not waived.

Fault tests execute 2,304 cases per class across 24 fault modes. They check
full-width and partial INFO writes, negative/positive/maximum INFO, pivot
corruption, omitted individual or complete error writes, negative/NaN/Inf
estimates, subnormal/zero/negative-zero/greater-than-one errors, NaN/Inf X,
and precedence when defects coexist. The shim separately verifies native
output mathematics before injecting faults.

Validation covers all 21 unordered operand-overlap pairs, independent layout
and leading-dimension plan mismatches, shape/error/pivot metadata, malformed
classic pivots, used workspace-region defects, exact zero divisors and
provider/plan/workspace/report aliases. PROT_NONE pages enforce metadata-only
query, empty/no-RHS and insufficient-workspace rejection boundaries. Pure
count tests exercise both integer limits without invented backing storage.

Concurrency has 192 groups per class with four workers and four repetitions,
shared read-only protected AP/AFP/B/IPIV, and disjoint X/errors/scratch/reports.
Each result matches an independently checked serial call; stale plans preserve
outputs and scratch. The allocation observer remains outside concurrent
workers. Mapping/protection failures are failed assertions, not test skips.

Ten strict translation-unit checks pass, including production, the fault
shim, seven maintained test executables and the public installed consumer.
The final maintained probe compiles against actual GNU-emitted signatures
and passes 1,296 guarded native cases per ABI. Its first harness stopped on
ambiguous recursive discovery of old archives inside a reused build root;
completed compilations were preserved, and exact archive paths from the
current CMake target completed the previously unstarted links and tests.

Four relocated consumers, actual LP64/global ILP64 and static/shared, each
pass 6,912 factorization/refinement workflows using installed public headers.
Prefixes contain spaces and are moved before consumer configuration. Consumer
compile commands and package metadata contain no source/build-tree paths;
runtime dependency checks pass. Exact private-error and padding guards,
independent mathematics, immutable operands and stale-plan preservation are
checked. The sixteen actual LP64/global ILP64 static/shared Release, Debug,
ASC-ASan+UBSan and TSan profiles retain six mathematical failures per normal
or sanitizer profile. All four TSan profiles pass 6/6. Four standalone header
tests, ten package manifests, three architecture checks and five public
surfaces pass. Both ABIs add twelve exports and remove none. The actual CI
selector includes 1,787 tests and its guard requires all 44 refinement runtime
processes. Strict documentation and final evidence binding complete this
engineering checkpoint without granting numerical acceptance.

All raw attempts, logs, identities and generated evidence stay outside the
source tree under the existing
`../asc-cpp-evidence/lapack-array-io/master-continuation-20260910-01/continuation-20260912-01/completion-execution-01`.
Pinned Fortran/BLAS internals remain uninstrumented in ASC sanitizer checks.
Native20, earlier numerical failures and historical evidence remain intact.
No provider/platform/security/XBLAS/notice or full-program acceptance follows
from this implementation. Continue packed SV/VX and the remaining ready queue.
