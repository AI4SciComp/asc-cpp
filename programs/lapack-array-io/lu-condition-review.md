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

## Master continuation: normalized GECON modes

The integration-pending statements above describe historical checkpoints.
GECON's existing public declarations, adapter, ordinary tests, full-width INFO
checks and installed advanced LU consumer are already integrated. The current
P04.required.gecon continuation adds bounded missing evidence and normalizes
four routine contracts; it does not implement the estimator again. The eight
modes per scalar are one/infinity norm, row/column storage and empty/active
order. Original ANORM is a finite nonnegative input value, not another flag or
catalogue entry. No FACT, UPLO, pivot array or Hermitian-only interpretation
is introduced for general LU.

`master-continuation-20260910-01/gecon-continuation-contract-01/review.json`
binds all four pinned definitions, existing declarations and finite remaining
work. Real S/D storage and RCOND/ANORM are float/double, with 4N scalar and N
native-integer work. Complex C/Z factor/work storage is complex float/double;
ANORM/RCOND and 2N real work use the underlying real scalar, with 2N complex
work. Complex native adjoint solves are internal estimator operations, not a
reinterpretation of caller LU. The earlier integer, layout, aliasing, output,
local-completion and raw INFO contracts remain unchanged.

The old composed root evidence is reusable for 48 ordinary/INFO processes:
eight per static Debug/Release/ASC-only sanitizer profile in both actual ABIs.
The selected compiler-dependency comparison found 39 identical files and one
exact comment-only change: four GESV RHS documentation lines now say both
layouts. No declaration or executable token changed. All six LAPACK/BLAS/
LAPACKE archives match the current prepared providers byte for byte. Exact
prior test IDs, commands, JUnit hashes and the comparison are retained in
`gecon-existing-evidence-reuse-02/audit.json`; these tests were not rerun.
The first audit attempt's unnormalized archive path is preserved separately.

New Release concurrency tests pass for S/D/C/Z in both actual ABIs. Each norm
and layout uses four independent caller/provider-context/workspace/report
states, sharing only an immutable plan. Each worker performs 32 real-provider
estimates on independent diagonal systems with analytic reciprocal condition
one quarter, followed by distinct invalid-norm, singular, stale-plan or normal
outcomes. There is no shared workspace or mutable global fault callback. The
first version incorrectly assumed a specific INFO=1 outcome for an order-three
nonfinite complex factor fixture, outside the accepted finiteness promise.
That failed attempt and the exact diagnostic-fixture correction are retained
in `gecon-concurrency-fixture-amendment-01`. The existing accepted INFO=1 test
and every scalar mathematical assertion remain unchanged.

The new observer passes 208 controls over 32 scalar/norm/layout/path cases in
both actual ABIs: 32 intentional old-RCOND reads, 32 restored writes, 32
protected queries, 32 stale plans, 40 exact one-byte-short workspaces, 16 local
completions, 16 validated actual native entries and eight legitimate row-pack
input reads. It uses initialized aligned containing arrays, PROT_NONE and
terminating child signals. Exact SDK-checked entry wrappers restore only the
output page before calling the real provider. Descriptor metadata remains
readable. The observable region is executed ASC handling before native entry;
provider/runtime internals are not instrumented. Local outputs remain writable,
so local no-read semantics are source-reviewed rather than inferred from
PROT_WRITE. `gecon-modes-{lp64,ilp64}-02` records the five passing new processes
per ABI; `gecon-modes-style-01` records both strict-analysis passes.

| Mathematical cause | Precise fixture and requirement | Observed result and comparison | Disposition |
| --- | --- | --- | --- |
| Guarded inverse-scale restoration | S/D/C/Z, N=1, raw LU=ANORM=minnormal/1024, one/infinity norm, both layouts. The exact nonzero scalar condition is one; retain the existing 32-epsilon scalar assertion. | RCOND=0, INFO=0, unchanged factors; direct pinned GECON matches. In xGECON, SCALE less than abs(WORK(IX))*SMLNUM (CABS1 for complex), or zero SCALE, returns before restoration and final condition evaluation. | Preserve the failed mathematical gate. Numerical acceptance needs a separately authorized scale-invariant inverse-estimation strategy or provider decision. Reference fidelity alone does not pass it. |

The maintained scalar gate uses the already recorded GESVX/GECON tiny fixture
and its 2*minnormal safeguard, which passes. For a nonzero scalar,
abs(a)*abs(1/a)=1 regardless of either matrix norm; intermediate overflow in an
implementation does not invalidate this analytic oracle. Each actual-ABI
Release profile has four failed processes, 16 assertions, four failing
scalar/value cases and 16 norm/layout executions, with one inherited cause.
The direct and mathematical bodies are identical across the concurrency-only
amendment; their original failed executions in `gecon-modes-{lp64,ilp64}-01`
are reused explicitly. These failures are separate from passing concurrency
and observer controls, and are never registered as expected-success tests.

Static Debug/Release/ASC-only ASan+UBSan profiles in both actual ABIs complete
the five new engineering checks, retaining four mathematical failures and 16
assertions per profile. Both static TSan profiles pass all four concurrency
processes. The exact initial Release executions and unchanged mathematical
body comparison remain in `gecon-static-audit-01/audit.json`.

Shared Debug/Release/ASC-only ASan+UBSan each pass 13 of 17 processes in both
actual ABIs; the same four mathematical failures retain identical raw scalar
observations and 16 failed assertions. Both shared TSan profiles pass 4/4.
There are no skipped tests. `gecon-shared-audit-01/audit.json` retains actual
selected IDs, commands, exit statuses and comparisons for all eight profiles.
Sanitizer/race instrumentation covers ASC/test code, not the pinned provider
or runtime. Shared observation uses the existing test-only DSO assembled from
production ASC objects; the installed exported DSO remains unwrapped.

The public installed advanced LU consumer passed in all four fresh PTSVX
packages on feature revision 674c0987. The GECON source, declarations and
consumer inputs are unchanged. `gecon-installed-reuse-01/audit.json` binds
these actual relocated executions, the eight exported GECON functions per
package, public include paths and intended runtime closure. This reuse does
not claim a newly installed API or repeat native20/array-I/O acceptance.

The frozen continuation source is tree
`bc36402b0684791ff50e3996cd0b487f27b9939d`, archive SHA-256
`962f8044bf57a8b5461b3e27d569ef8c4593984dd4c9f18684be7c45b02a8402`,
based on 674c0987. Its 1,200 product/build/test/example inputs are retained in
`gecon-frozen-product-01`. Both new test units pass strict analysis. No public
declaration or production estimator changed. Final narrative updates do not
give a later whole tree execution credit. The four normalized Reference rows
remain callable-unverified; mathematical acceptance, wider provider platforms
and owner decisions remain open. The next dependency-ready continuation is
the existing GEEQU/GEEQUB family, whose prerequisites are recorded in
`geequ-continuation-prerequisite-01/review.json`.

Feature revision `a1ca705174e8e4f755c5a97a44efc488e5ab0ae6`, tree
`1097a5f7b4423750c9c0ef2b589884a53a8ad566`, was ordinarily pushed. Fresh
CI34641336597 passed all 19 jobs and CodeQL34641336612 passed. Hosted merge
3978a41bb6821ca1765884979b40d8acffe1c696 has the exact same tree. The family
run34641336577 remains failed: all four profiles pass 197/226 with 29 genuine
mathematical failures. The previous 25 failed process outputs are identical;
the four new GECON failures retain 16 assertions and match local raw values.
The 19 added selectors include 17 GECON processes and two public-header tests.
All 111 provider checks and five PT public examples pass per profile, with no
skips. `gecon-hosted-audit-01/audit.json` binds actual IDs and provider/source
identities. These checks do not certify a later GEEQU/GEEQUB or GERFS revision.
