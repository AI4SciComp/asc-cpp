# Rook indefinite factorization and solve

Status: **implemented, numerical acceptance blocked; FULL_PROGRAM_INCOMPLETE**.

The eighteen pinned S/D/C/Z SYTRF_ROOK/SYTF2_ROOK/SYTRS_ROOK and
C/Z HETRF_ROOK/HETF2_ROOK/HETRS_ROOK routines have explicit public query and
execution APIs in `asc/dense/providers/lapack_indefinite_rook.h`. The classic
family and its recorded range failures are preserved.

## Contracts and provenance

`ReferenceRookFactorView<T>` carries same-operation provider identity, scalar,
originating rook routine, selected triangle, symmetry and borrowed raw pivots.
It uses the existing `kRook` family tag and the existing mathematical symmetry
enum. Classic factor views, reports and pivot tags are not interchangeable.
Adjacent negative pivots each describe an independent interchange, in source
order; they need not be equal. Each target must be inside the active principal
block. Validation does not infer stronger pivot-search restrictions or content
provenance from the values. Caller ownership and same-operation provenance
remain preconditions.

S/D use real coefficients. C/Z symmetric operations preserve full complex
coefficients and transpose symmetry; C/Z Hermitian operations use conjugate
transpose. Original Hermitian imaginary diagonals are ignored by explicit
packing in both layouts. Consuming a raw factor preserves its full selected
coefficients. The factor equations are U*D*U^T or L*D*L^T, with conjugate
transpose for Hermitian factors; U/L incorporate the ordered permutations.
Only selected factor entries and logical RHS entries are published.

Queries read descriptor metadata, allocate nothing and call no provider.
Blocked factorization requires scalar WORK at least one, with preferred
capacity from pinned NB=64 and actual S/C rounding. Reduced workspace retains
source thresholds eight for SY and two for HE. Unblocked factors and solves
require no scalar WORK. Every active operation uses n provider-width integer
scratch entries. Row-major factors and every original Hermitian factor require
n*n layout entries; row-major RHS adds n*nrhs. Empty operations make no native
call. Plans bind actual routine, scalar, layouts, strides, dimensions, triangle,
symmetry and provider ABI. All active mutable buffers, scratch and live
metadata are disjoint, accessible to the explicit serial CPU context and
checked before numerical mutation. INTEGER intermediates and terminal BLAS
cursors are checked using the reviewed common count utilities.

Factor INFO>0 retains documented partial raw output and checked pivots; it
cannot create a reusable successful factor view. Selected scalar NaN returns
INFO=1 through the pinned GNU MAX(NaN,0) behavior, despite the rook source
having no separate DISNAN test. A successful return does not certify finite
values. Solves reject exact zero block divisors before mutation without
fabricating native INFO. Unexpected, omitted or partial-width INFO is a
provider defect; the exact signed value and unusable-output status remain.

## Independent numerical decisions

The required range cases include scale*I at orders 2 and 67, both triangles,
both layouts and blocked/unblocked entries. The exact factor oracle requires
D=scale*I, identity U/L and pivots, and explicitly finite coefficients. Scales
are twice minimum normal, minimum-normal/1024, twice minimum subnormal and
maximum finite. All six factorization classes pass in the initial LP64 and
ILP64 Release profiles, consistent with the rook safe-minimum division guards.

Scalar solves use actual factors of A=scale, B=scale, two RHS columns and both
RHS layouts. The exact solution is one. At the two tiny scales every scalar
and symmetry class instead returns nonfinite output with INFO=0. TRS_ROOK
forms a reciprocal before SCAL. Direct typed native calls agree, while the
finite-value, known-solution and original-matrix residual requirements fail.
The normal/maximum controls pass. These are live required CTest failures,
not an accepted accuracy waiver or a reason to replace the selected algorithm.

Ordinary numerical tests reconstruct factors through an independent reverse
Schur-complement calculation and undo both interchanges in reverse order.
A three-by-three case forces both swaps to be nontrivial. Both triangles,
independent factor/RHS layouts, complex symmetry, ignored Hermitian diagonal
components, minimum/preferred/reduced work, scaling, padding, alias rollback,
empty cases, factor reuse and allocation constraints are exercised. Concurrent
workers use independent mutable storage and share actual immutable factors;
no global allocation observer is used in the concurrency test.

## Evidence checkpoint

Raw evidence remains below
`master-continuation-20260910-01/continuation-20260912-01` in the existing
external evidence root. `rook-prerequisite-01` preserves all six omitted TF2
compiler declarations for both real integer ABIs. `rook-contract-review-01`
binds eighteen entry sources and six panel sources. Its amended review
preserves and corrects the initial NaN inference: `rook-emitted-abi-*-01`
checks compiler-emitted function types, executes 24 direct TF2 ABI cases and
24 direct NaN factor calls per ABI. No generated declaration is guessed.

`rook-first-01` passes the smallest direct ABI and double numerical checks.
The first full Release profiles retain 32/44 passes, six new preflight failures
from the incorrect NaN expectation and six required solve-range failures.
The NaN expectation was corrected from direct native evidence; all failed logs
remain. The second LP64 and ILP64 profiles each pass 44/50 with only the six required
solve failures and 1,152 failed assertions. All twelve actual static/shared
Debug/Release/ASC-ASan+UBSan profiles reproduce that result. Four further TSan
profiles each pass six concurrency processes. No unexpected skip occurs.

The final source audit binds 624 selected profile results: 552 passes and 72
required failures, totaling 13,824 failed mathematical assertions. Later strict
fixes add one direct pivot-view include and extract the unchanged reverse-swap
oracle into a helper; only the affected numerical/preflight tests are refreshed
where input hashes differ. Every earlier result remains external, including a
lexer crash when an additive edit overlapped one analyzer invocation. That
invocation was repeated after the source was frozen; no checker was suppressed.

Strict checks pass twenty implementation/test translation units and both
copied installed examples. Four relocated static/shared LP64/ILP64 consumers
pass using public package targets, without source/producer include paths.
Both standalone header modes, with and without exceptions, pass in both ABIs.
Shared symbol comparisons add exactly forty supported rook symbols and remove
none in either ABI. No existing public declaration or numerical implementation
is modified. All actual C++ profiles use GNU 11.4.0; sanitizers instrument ASC
and the tests, while the pinned Fortran provider remains uninstrumented.
The first evidence audit incorrectly required bit-exact control solutions; its
correction uses the existing 64-epsilon test requirement and the original logs,
without changing any test or tolerance.

`rook-final-audit-02/audit.json` binds the final test selection, exact executable,
source, build, compiler, provider and installed-package identities. The atomic
`lapack-rook-evidence-extension.json` registers eighteen implemented-unverified
Reference rows and 168 reviewed mode cases, refreshes affected registration
hashes and preserves historical executions/native20. The subsequent delivery
audit binds documentation, metadata and the actual CI selector checks. No full
Reference verification is claimed; the next independent family is the six
SYCON_ROOK/HECON_ROOK condition-estimation routines.
