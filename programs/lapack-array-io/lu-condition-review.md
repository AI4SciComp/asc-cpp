# Reference GECON implementation and bounded evidence

This slice implements actual S/D/C/Z GECON in both matrix layouts. It is not
P04 completion, full-provider coverage, or owner/license approval. GERFS,
GESVX, required helpers and remaining ordinary-LU layouts remain separate
required work. The upstream denominator has not changed.

## Identities and actual signatures

The source/build/ABI identities documented in `provider-abi-review.md` apply:
Reference-LAPACK 3.12.1 commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, source-input manifest
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`;
LP64 build `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`,
true ILP64 build
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
All four complete routine argument sections and executable bodies, the pinned
`lapack.h` declarations and relevant LACN2/LATRS workspace/index arithmetic
were inspected before binding. Foreign character lengths and complex types
use the already audited GNU boundary, not invented public ABI declarations.

| File | SHA256 |
| --- | --- |
| `include/asc/dense/providers/lapack_lu_condition.h` | `9c43ae65b542e9c2b0353c6cc9539ce4b6ee09b9c7759849f5d77ab6a565f25e` |
| `src/dense/lapack/reference_lu_condition.cc` | `0d173dc774614d5cea27e6d89be349c125abccca3f0cdec2afce20641defc436` |
| `tests/dense_lapack/lu_condition_test.cc` | `1fc122f4e018e9281451b2be37d98d656bec9fe113f046abaaa0d0a0a210e100` |
| `tests/dense_lapack/lu_condition_faults.h` | `05a6362c8e6db902aac9652c9e172b2e3839ff33163b99ddd9cfc5d72cfc36bf` |
| `tests/dense_lapack/lu_condition_faults.cc` | `06bac12d3a20a95abfdd2cfac27f9a1c74240d331aa1d7bbfd8fe98466c43e77` |

External snapshot `lu-condition-frozen-whvbQ45C` retains exact headers, source
and test dependencies, including the ASC-sized layout-count correction
identified in `lu-equilibration-review.md`. It does not alter the earlier
frozen first-sixteen or equilibration numerical evidence.

## Workspace and numerical contracts

`LapackConditionNorm` represents one-norm and infinity-norm. The caller provides
the finite nonnegative norm of original unfactored A, immutable raw square LU
and a disjoint underlying-real RCOND output. GECON does not consume pivots.
Complex matrix norm means Euclidean scalar modulus, unlike GEEQU's CABS1.

`QueryGeconWorkspace` evaluates checked formulas without foreign entry:
real WORK is `4*n` scalar entries plus `n` ABI-width integer entries;
complex WORK is `2*n` complex entries plus `2*n` underlying-real entries.
Row-major adds explicit caller-owned `n*n` scalar packing for the original LU
entries, not transposed factor semantics. Every role's minimum equals preferred.
The implementation establishes trivial foreign-integer lifetimes in supplied
bytes; scalar/real/layout objects are already live. No allocation, LAPACKE
row-major conversion, hidden buffer or implicit transfer is used.

Each dimension/leading dimension, byte product, total simultaneously live byte
sum and ABI intermediate is checked. Complex LACN2 computes integer `3*n`
internally despite complex WORK/RWORK sizes `2*n`, so its order bound is
`ABI_MAX/3`; real WORK requires `ABI_MAX/4`. Plans bind scalar/routine,
shape/leading dimension/layout/norm flag and copied provider identity, not
numerical values or addresses. Revalidation rejects altered capacities,
staleness, placement, alignment and operand/workspace aliasing before writes.

Structural failure leaves RCOND/workspaces unchanged, with called_provider
false and absent INFO. Invalid/nonfinite ANORM is rejected before upstream
can mutate RCOND or call XERBLA. For n=0 RCOND becomes one without foreign
entry. On actual INFO=0 RCOND is a complete estimate, not an exact condition
number or a certificate of invertibility. In particular singular factors and
zero ANORM can return INFO=0, RCOND=0; this is not converted into a fabricated
singular-pivot INFO. Actual INFO=1 means failed estimation, with kNumerical,
`LapackOutcome::kAccuracyWarning`, documented partial RCOND and no index.
Negative or excessive positive INFO is a provider defect, retaining exact
raw signed INFO and foreign argument position where defined. A remains
unchanged on every return; nonfinite LU is not scanned or assigned a successful
finiteness promise.

## Executed checks and remaining gates

All evidence paths are relative to the external program evidence root.
Optimized final frozen builds and four scalar processes per ABI passed:
`logs/p04-lu-condition-{lp64,ilp64}-compile-07.log` and
`logs/p04-lu-condition-{lp64,ilp64}-test-{s,d,c,z}-07.log`.
Final ASan/UBSan builds and scalar processes passed:
`logs/p04-lu-condition-{lp64,ilp64}-asan-compile-03.log` and
`logs/p04-lu-condition-{lp64,ilp64}-asan-test-{s,d,c,z}-03.log`.
Adapters/foundations/tests are instrumented; baseline Core/Dense and static
Fortran/BLAS archives are not. This is not whole-library sanitizer evidence.

Tests factor padded 1/3/8-order matrices, preserve originals and independently
invert them with long-double Gauss-Jordan elimination. An independent identity
residual validates that oracle. Both norm flags and both layouts at binary
scales -60/0/60 check a bounded condition estimate, not unsupported exactness.
Scalar/diagonal cases check analytic condition numbers, including an
ill-conditioned diagonal. Other tests cover empty/zero-norm/singular cases,
actual positive INFO from infinite raw factors, injected negative/excessive
INFO, malformed workspaces, RCOND overlap, invalid flags/ANORM, stale norm and
provider identity, absent INFO on preflight, original-factor immutability,
exact minimum and oversized integer capacities, and unused/redzone sentinels.
C++ new plus static-provider malloc/calloc/realloc/aligned allocation probes
observe zero allocations on tested first/repeated/error calls.

Strict standalone self-contained header and Doxygen HTML/XML passed
`logs/p04-lu-condition-frozen-{header,doxygen}-01.log`; final formatting passed
`logs/p04-lu-condition-frozen-format-02.log`. Final strict tidy passed
`logs/p04-lu-condition-frozen-tidy-02.log`; the prior bounded test was split
without removing assertions after the 80-line function-size gate failed.

Exact archive object closure passed
`provider-lu-{lp64,ilp64}-01/lu-condition-static-call-closure-02.json`.
Only cabs/cabsf/memcpy/memset remain external to the traversed static objects;
preflight-excluded XERBLA is listed separately. No shared-runtime allocator
interposition guarantee is inferred. Earlier compile/script/style diagnostic
failures remain raw logs, not completion evidence.

Still required: atomic CMake/ABI/manifest integration,
installed component/header isolation, full-profile numerical regression,
GERFS/GESVX and the rest of P04. The independently preserved GEEQUB LP64
subnormal mathematical gate remains unmet and is not covered by GECON success.

## Leading-stride role correction before registration

The previous frozen identities and evidence above remain intact. A later
review found an overly conservative LP64 rejection: row-major source leading
stride is ASC-sized metadata, not the packed foreign LDA. The corrected query
narrows and binds only actual foreign LDA in dimensions and binds the original
source stride separately in options; no common foundation rule is weakened.
The regression uses real empty null-backed descriptors with stride
INT32_MAX+1, proving row-major acceptance in LP64, unchanged rejection for
oversized actual column-major LDA, and stale-source-stride plan rejection with
RCOND unchanged. The same new test fails against the preserved old source:
`logs/p04-lu-condition-stride-old-lp64-{compile,test-s}-01.log`.

Final registration identities in immutable `lu-condition-stride-e6A23kXP`:

| File | SHA-256 |
| --- | --- |
| `include/asc/dense/providers/lapack_lu_condition.h` | `e58df10aafddf95f2a00780dcfcf1db62a740ef43e62a8b9f02992c12be75224` |
| `src/dense/lapack/reference_lu_condition.cc` | `9bcfaca3a712db83e0647c9cc183231b040db24eac286b78aa164c95af811212` |
| `tests/dense_lapack/lu_condition_test.cc` | `fa01d707fa76fdb6cf43370a9d1dde3f94b7fe3c653c651f697d53042298cfd7` |

Fault support and every dependency remain the earlier frozen versions. All
four scalars in both ABIs passed optimized compile/test revision 08 and
ASan/UBSan compile/test revision 04, with the same partial-instrumentation
scope. The final strict checks passed
`logs/p04-lu-condition-format-frozen-03.log`,
`logs/p04-lu-condition-header-frozen-03.log`,
`logs/p04-lu-condition-doxygen-frozen-02.log`, and
`logs/p04-lu-condition-tidy-frozen-03.log`.
The missing temporary header-check translation unit failed in header revision
02 before compilation; revision 03 uses a real self-contained translation
unit. The exact provider archive closure is unchanged. GERFS now has its own
bounded evidence in `lu-refinement-review.md`; GESVX, integration, installed
isolation and all other remaining required gates are not completed by this fix.
