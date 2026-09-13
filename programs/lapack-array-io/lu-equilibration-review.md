# Reference general-matrix equilibration evidence

The historical integration-pending statements below are superseded by the
[current continuation](#master-continuation-normalized-geequgeequb-modes).

This bounded slice implements actual S/D/C/Z GEEQU and GEEQUB, in both
column-major and row-major layouts. It does not close P04, the full provider,
owner/license approval, or the unmet LP64 extreme numerical gate below.
The full upstream denominator is unchanged. All eight actual source argument
sections and executable bodies were inspected before binding.

## Frozen implementation and dependency identities

The source/compiler/ABI identity in `provider-abi-review.md` applies: upstream
commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, Reference-LAPACK 3.12.1;
source-input manifest
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
LP64 build identity is
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
true ILP64 is
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.

| File | SHA256 |
| --- | --- |
| `include/asc/dense/providers/lapack_lu_equilibration.h` | `d6cb99386c03335eeff8fa26ac2df2105225920865b63252a109cf116edaa416` |
| `src/dense/lapack/reference_lu_equilibration.cc` | `a2ca41503531bc104888ef33d88278314969d4c28dd4236c8708511548e5fd8b` |
| `tests/dense_lapack/lu_equilibration_test.cc` | `e79d25c100f5ad1837c92a218309205c7b5cd2ff7441279fd2f87967996ecb09` |
| `tests/dense_lapack/lu_equilibration_faults.h` | `602697f95e4cd160ddc611b88c37c070a0f6c1ca71cacc7a434a5803b9b907f9` |
| `tests/dense_lapack/lu_equilibration_faults.cc` | `ae70a26391206330a1fe8bc385722d0b9f5d1d2111bb17087b0f26ddef52fe9a` |

External snapshot `lu-equilibration-base-LQ9hMBIL` copies the prior frozen
first-sixteen dependencies, then adopts the separately reviewed next-slice
foundation correction: `src/dense/lapack_foundations.cc` SHA256
`df1d29ca7dd14307e0c0e834bfcc24d786a6a1e6157a9d048cf0210a47b468e2` and
`include/asc/dense/lapack/workspace.h` SHA256
`39f15a4cdb5d486dd8a79755b8b89cf98f9033b44dc17288be7a348e8f881232`.
Only layout-conversion storage counts are ASC-sized rather than narrowed as
foreign LWORK; checked byte products, capacity, alignment and aliases remain.
The old first-sixteen frozen snapshot and evidence are unchanged.

The equilibration header's four GEEQUB declaration return comments were then
clarified to identify computed-scale failure and the nonzero-input limitation;
no signature or behavior changed. Numerical logs used the preceding
comment-only header SHA256
`eb7385a0caa7baf34572ecde9b19699220c53d4648495c420a78b141a2b6cb61`.
The table records the corrected header, whose format/header/tidy checks were
rerun. Production source and numerical test identities are unchanged.

## Actual contracts

`QueryGeequWorkspace` and `QueryGeequbWorkspace` are checked formula queries,
not foreign LWORK calls. Column-major requires no numerical scratch. Row-major
requires explicit caller-owned `m*n` live scalar entries and packs original A
to column-major. Equilibrating transpose(A) and swapping output scales would
not preserve sequential row/column semantics and is not used.

Outputs are separate contiguous underlying-real R/C vectors and a caller-owned
`LapackEquilibrationStatistics<Real>` containing ROWCND, COLCND and AMAX. Complex
magnitudes here are `abs(real)+abs(imag)`. GEEQU AMAX is the original maximum;
the pinned GEEQUB implementation computes AMAX from radix-quantized row maxima
before inversion/clamping. The distinction is normative, not an adapter
substitution. Independent tests quantize using binary decomposition, not the
provider's logarithm/exponentiation route.

Every structural error precedes numerical mutation and foreign entry. All
input/output/workspace aliases, CPU placement, exact output dimensions/strides,
shape/leading-dimension ABI limits and `m+n` INFO representability are checked.
Plans bind the routine/scalar/provider and actual descriptor metadata.
Execution preserves A/padding. Empty dimensions produce ratios one and AMAX
zero without foreign entry, leaving R/C untouched.

Negative or excessive positive INFO is a provider defect. Actual positive INFO
retains a zero-based row/column diagnostic index and documented partial
validity: row failure leaves only AMAX valid; column failure also leaves valid
R and ROWCND. GEEQU identifies exact-zero input rows/columns. GEEQUB reports
`kPartialResult`, not a false certificate that A is singular: computed radix
scales can fail even for nonzero input as documented next.

## Preserved unmet LP64 numerical-success gate

For the same nonzero 3-by-2 fixture scaled by `2^-140` (float/complex float) or
`2^-1050` (double/complex double), the pinned LP64 S/D/C/Z GEEQUB returns INFO=1,
zero row scales and AMAX=0; column scales and both ratios are unwritten. Every
original input entry is nonzero and A remains unchanged. GEEQU succeeds for
this fixture in both ABIs. True ILP64 GEEQUB also succeeds for this fixture.

Original failed mathematical-success expectations are retained in
`logs/p04-lu-equilibration-lp64-test-s-02.log`,
`logs/p04-lu-equilibration-lp64-test-s-03.log`, and
`logs/p04-lu-equilibration-lp64-extreme-diagnostic-{d,c,z}-01.log`.
The dedicated final fidelity test asserts exact raw failure and partial
validity; its passing does not satisfy the LP64 mathematical-success gate.
No source patch, changed provider identity or owner approval is inferred.

Independent `lu-equilibration-direct-probe.cc` calls the pinned upstream
functions without ASC on exactly that fixture. Both ABI probes pass their
distinct raw-result expectations:
`logs/p04-equilibration-direct-{lp64,ilp64}-{compile,run}-01.log`.
Actual LP64 objects call libgcc `__powisf2`/`__powidf2`, whose reviewed machine
code forms a positive power before taking its reciprocal; intermediate
overflow loses the representable tiny result. True ILP64 objects call
libgfortran `_gfortran_pow_r4_i8`/`_gfortran_pow_r8_i8`, which invert the base
first. Evidence is `logs/p04-equilibration-{lp64,ilp64}-pow-symbols-01.log`,
`logs/p04-equilibration-powi{sf,df}2-01.log`,
`logs/p04-equilibration-pow-r{4,8}-i8-01.log` and
`logs/p04-sgeequb-object-disassembly-01.log`.

Runtime identities are libgcc SHA256
`fc9d43b2f6c20e53b009238f767c5b949d202389e20de9e202ea684b4ba3729a` and
libgfortran SHA256
`f7379c9331de9d66c03b439a070e8056b7419fc4213320d647952b998d026b30`.
Reviewed power-helper machine bodies contain no function or indirect calls.

## Executed bounded verification

All paths below are relative to the external program evidence root. Both ABI
builds and all four independent scalar processes exited zero:

- Optimized final frozen sources:
  `logs/p04-lu-equilibration-{lp64,ilp64}-compile-07.log` and
  `logs/p04-lu-equilibration-{lp64,ilp64}-test-{s,d,c,z}-07.log`.
- ASan/UBSan final frozen sources:
  `logs/p04-lu-equilibration-{lp64,ilp64}-asan-compile-02.log` and
  `logs/p04-lu-equilibration-{lp64,ilp64}-asan-test-{s,d,c,z}-02.log`.
  New and existing provider adapters, foundations and test support are
  instrumented; baseline Core/Dense and Fortran/BLAS archives are not. This is
  not full-library sanitizer coverage.
- Clang 18 repository formatting, self-contained C++20 public header and strict
  source/test tidy: `logs/p04-lu-equilibration-frozen-{format,header,tidy}-01.log`.
- Corrected-header standalone strict Doxygen HTML/XML:
  `logs/p04-lu-equilibration-frozen-doxygen-03.log`. Earlier XML-only minimal
  configurations reported missing parameter documentation despite generated
  XML containing those documents; failed logs 01/02 are retained. Enabling
  HTML and XML together matches the repository documentation workflow.
- Exact static archive call closure:
  `provider-lu-{lp64,ilp64}-01/lu-equilibration-static-call-closure-01.json`.
  Runtime leaf inspection is distinct from shared-runtime interposition;
  XERBLA argument-error branches are preflight-excluded.

Tests cover real/complex 3-by-2 padded matrices in both layouts, independent
scale/statistic equations over normal small/unit/large and near-overflow
scales, the separately classified subnormal fixture, exact-zero row/column
failures, empty shapes, repeated calls, query nonmutation, workspace redzones,
invalid shape/stride/placement/alias/plan/capacity/alignment, and injected
negative/excessive INFO. C++ new and static provider malloc/calloc/realloc/
aligned allocation probes observe zero calls on tested successful and failing
paths. No zero-test or skipped run is counted.

Still required: atomic CMake/public-header/coverage integration, installed
component tests for this new header/source, full-profile regression and the
unmet LP64 numerical case. Next implementation remains GECON, GERFS and GESVX,
then required helpers and remaining ordinary-LU layout routes. It is not a
claim that P04 or later packages are complete.

## Separate ASC-stride / foreign-LDA correction

After the original equilibration slice was frozen, ordinary-LU integration
identified a role error also present here: the formula query treated an ASC
row-major leading stride as a foreign INTEGER LDA. For a packed row-major
matrix the actual provider LDA is max(1,m), not the original row stride.
The eight pinned source routines require LDA >= max(1,m); their quick return
sets ROWCND=COLCND=1 and AMAX=0 without reading or writing R/C. Those argument
and quick-return sections were reinspected for this correction.

This is not a replacement equilibration algorithm. Nonempty execution still
calls actual S/D/C/Z GEEQU/GEEQUB, directly for column-major and on explicitly
caller-packed original A for row-major. Empty execution retains the existing
local source-defined quick return. The new private leading-dimension helper
is shared by validation, plan identity and foreign execution. Identity
dimensions bind actual foreign LDA; options additionally bind the original
ASC leading stride. Row strides above INT32_MAX are therefore legal under
LP64 when their real views are valid, but a changed original stride still
invalidates the plan. A genuinely foreign column-major LDA above INT32_MAX
remains rejected under LP64. Shape, m+n INFO, byte-product, capacity,
alignment and alias checks are unchanged.

New tests use real null-backed empty shapes 0-by-0, 0-by-2 and 3-by-0 with
original stride INT32_MAX+1, not fictitious huge backing spans. Both layouts,
all four scalar types and both routines exercise query nonmutation and zero
allocation, exact successful empty outputs without native INFO/provider
entry, unchanged R/C, and stale original-stride rejection without mutation.
LP64 oversized column-major leading dimensions retain their overflow error;
true ILP64 permits them. The new tests against unchanged production source
`a2ca41503531bc104888ef33d88278314969d4c28dd4236c8708511548e5fd8b` fail six
LP64 row-major assertions in
`logs/p04-lu-equilibration-stride-lp64-optimized-test-s-old-01.log`.
Immutable `lu-equilibration-stride-old-79lN2A7d` preserves that old-source/new-
test regression snapshot and executable.

The corrected candidate is separately frozen in
`lu-equilibration-stride-68Q9JMsI`, using the original frozen dependencies,
not the integrator's concurrently changing original-LU layout sources:

- Source SHA256:
  `7b21e989423fa4d09874e96c11fd4aee3c81d1f47ca9a937d0f66f27d6e776c2`.
- Numerical test SHA256:
  `2f4530ef6cf3c1d75b13e074797905054db607e51bc6487271f86aa8435cc7ba`.
- Public header remains
  `d6cb99386c03335eeff8fa26ac2df2105225920865b63252a109cf116edaa416`;
  fault injection sources and all GESVX driver files remain unchanged.

Both-ABI optimized builds and all four scalar processes exited zero in
`logs/p04-lu-equilibration-stride-{lp64,ilp64}-optimized-compile-01.log` and
`logs/p04-lu-equilibration-stride-{lp64,ilp64}-optimized-test-{s,d,c,z}-01.log`.
Both ASan/UBSan builds and all four scalar processes also exited zero in
`logs/p04-lu-equilibration-stride-{lp64,ilp64}-asan-compile-01.log` and
`logs/p04-lu-equilibration-stride-{lp64,ilp64}-asan-test-{s,d,c,z}-01.log`.
The instrumented adapters/foundation/tests still link uninstrumented baseline
Core/Dense, Fortran/BLAS and runtime libraries; this is not full-provider
sanitizer coverage. No tests were skipped or disabled.

Clang 18 format, self-contained C++20 header, complete source/test/fault tidy,
and strict Doxygen HTML/XML checks each exited zero in
`logs/p04-lu-equilibration-stride-{format,header,tidy,doxygen}-01.log`.
The reproduction entry points are `build-lu-equilibration-stride.sh` (snapshot,
ABI, unique log revision, optional `asan`) and
`check-lu-equilibration-stride.sh` (snapshot, unique log revision). Raw provider
source, ABI, numerical dependencies and the previously audited call closure
are unchanged by this metadata correction.

Next required integration is the integrator's separate advanced-LU checkpoint,
atomically registering GECON/GERFS/GESVX together with these two corrected
equilibration source/test files, then rerunning installed/component and full
profile gates. Their local verification is recorded separately, not attributed
to the earlier ordinary-LU/P10 checkpoints. This bounded correction does not
satisfy the distinct LP64 GEEQUB subnormal mathematical-success gate above.

## Master continuation: normalized GEEQU/GEEQUB modes

All eight public scalar routines and their installed declarations already exist
in the integration tree. This continuation retains those adapters and adds the
missing bounded concurrency/output-observation evidence and a maintained true
mathematical gate. `master-continuation-20260910-01/geequ-continuation-contract-01`
binds the eight inspected pinned definitions and 17 unchanged selected inputs.
There is no FACT, UPLO, factor-reuse token or Hermitian-only interpretation.
S/D input storage is float/double; C/Z is complex float/double. All five output
operands use the underlying real scalar, including the three statistics.
Complex magnitudes use CABS1. The eight modes per scalar/routine are two layouts
and four shape paths: both empty, rows empty, columns empty and active rectangle.

The original workspace, metadata-only preflight, aliasing, INFO and partial
validity contracts above remain unchanged. Column-major has no numerical work;
row-major packs the original matrix into M*N explicit live scalar entries.
Plans bind actual native LDA separately from the original ASC row stride.
M+N remains bounded for native INFO. No numeric old R/C/statistics values are
inputs. On local completion only statistics are written; R/C remain untouched.

The existing first-party Sample and ExpectedScales template blocks are retained
unchanged in the new test-only support header, with their origin hash in
`geequ-continuation-contract-01/fixture-oracle-origin.json`. The original test
unit remains unchanged and independently compiled. This reuses the same
nonzero fixture, binary-decomposition quantization and 16-epsilon scale oracle;
no upstream algorithm was copied and no tolerance was relaxed. In particular,
the oracle does not replace GEEQUB's quantized AMAX with the original maximum.

The four new concurrency processes each cover both routines and layouts. Four
workers use independent matrices, scales, statistics, explicit workspace,
reports and serial provider contexts, sharing only a matching immutable plan.
Each makes 32 real-provider calls on independently scaled ordinary fixtures
and checks the existing scale equations. Final outcomes differ between an
exact zero row, exact zero column, stale plan and success, checking private
INFO/validity/index and output isolation. No mutable fault or allocation-audit
callback participates in the threads. Both static TSan profiles pass 4/4;
provider/runtime objects are not instrumented.

The calibrated observer passes 592 controls over 64 scalar/routine/layout/shape
cases: 320 intentional old-output reads, 64 restored writes, 64 protected
queries, 64 stale plans, eight exact one-byte-short packing regions, 48 local
completions, 16 validated native entries and eight legitimate row-packing
input reads. Initialized, aligned containing arrays have valid object lifetimes;
PROT_NONE pages and default terminating child signals observe attempted access.
Each of the three statistics members is calibrated separately while sharing its
containing object's page. Exact SDK-signature wrappers validate dimensions,
LDA, routine and every output address before restoring access and entering the
real provider. Metadata remains readable. Local statistics stay writable and
their write-only completion is source-reviewed; local R/C are protected.
This observes executed ASC handling before native entry, not foreign code.

Initial compile attempts in both ABIs and the first modes strict check failed
because the reused private test helper needs the generated configuration include
path. `geequ-test-include-amendment-01` preserves that attempt and the CMake-only
correction. Both new units now pass strict analysis; the unchanged observer's
first strict result is reused. No mathematical or production C++ change was
made for this correction.

| Cause | Fixture and unchanged requirement | Actual result | Disposition |
| --- | --- | --- | --- |
| LP64 radix power-helper intermediate overflow | Nonzero 3-by-2 S/D/C/Z fixture from Sample, scaled by 2^-140 for S/C or 2^-1050 for D/Z, both layouts. Require successful equilibration and the existing 16-epsilon scale/statistic equations. | LP64 GEEQUB returns INFO=1, R=0, AMAX=0; C and both ratios retain initialized sentinels. The already recorded direct calls and helper disassembly reproduce this. LP64 libgcc forms the positive power before reciprocal; true ILP64's libgfortran power helper inverts the base first and succeeds. GEEQU succeeds in both ABIs. | Preserve the mathematical failure. A separately authorized provider/compiler-runtime strategy or explicitly named first-party radix evaluation is needed; no hidden remapping, provider patch, runtime change, waiver or numerical promotion. |

Each LP64 profile has four failed mathematical processes, 88 assertions, four
scalar fixtures and eight layout executions, sharing this one recorded cause.
The extra assertions expose the already required scale and diagnostic outputs;
they are not additional independent bugs. The maintained failure is separate
from the existing provider-fidelity test. The source/fixture/direct evidence in
the earlier section remains the cause evidence; no large reproduction sweep
was restarted.

Static Debug/Release/ASC-only ASan+UBSan in actual LP64 each pass the five new
engineering checks and retain the four mathematical failures. True ILP64 passes
all nine new checks in each profile. Both static TSan profiles pass all four
concurrency processes. `geequ-static-audit-01/audit.json` records exact selected
IDs, statuses, no skips and identical raw mathematical observations across
profiles. Sanitizer instrumentation covers ASC/test code, not provider/runtime.

Forty-eight existing ordinary/INFO processes are reused across those six static
profiles. The selected compiler dependency closure has 41 first-party files;
40 are byte-identical and the remaining difference is exactly four previously
reviewed GESV RHS documentation lines. All six prepared provider archives were
already proved identical by the prior GECON comparison, with no subsequent
provider/compiler/runtime change. `geequ-existing-evidence-reuse-03/audit.json`
retains exact IDs and records. Attempts01/02 failed in the audit parser's
comment text and old record-schema assumptions; no tests were rerun for them.

Four actual installed/relocated consumers are reused from the fresh static/shared
LP64/ILP64 PTSVX packages. Actual GEEQU/GEEQUB calls are in
`tests/dense_lapack/installed_lu/main.cc`, executable `installed_lu`, test
`installed_lu_families`; the initial task pointer incorrectly called this the
advanced LU consumer. Both routines, all four scalars and both layouts check an
independent diagonal system's exact scales and diagnostics. Six source inputs
are unchanged, 16 public functions are exported per package, and compile/runtime
inspection confirms public relocated includes and the intended separate provider
closure. `geequ-installed-reuse-01/audit.json` binds the actual executions.

All shared Debug/Release/ASC-only ASan+UBSan profiles are complete. LP64 passes
13/17 with the same four mathematical failures and 88 assertions; true ILP64
passes 17/17. Both shared TSan profiles pass 4/4. The eight-profile audit in
`geequ-shared-audit-01/audit.json` preserves exact IDs, exit statuses, no skips
and identical raw mathematical observations. Shared observation uses the
existing test-only DSO of production ASC objects; the actual installed exported
DSO remains unwrapped. No foreign instrumentation is claimed.

The frozen source is tree `f425f42893786c76280a58a465528252e3c891a5`, archive
SHA-256 `553aa16cd04f1225b9b450e2ca2763552827ba2ab30e0c928dc85b6a72ed09f7`,
based on feature revision a1ca7051. The 1,204 recorded product/build/test/example
inputs are separate from final narrative changes. Both new test units passed
strict analysis, and scoped coverage/backlog, format, links and whitespace
checks passed. No public declaration, production source, export, ABI or
documentation-generation input changed; prior matching package/header/Doxygen
evidence remains reusable for those inputs. A later feature commit still needs
its own hosted checks.

All eight rows are callable-unverified with normalized modes; no Reference row
is promoted to verified. Native20/array-I/O and the separate robust PPSVX
milestone remain unchanged. Wider provider environments and owner decisions
remain separate. The next ready existing family is GERFS, with its typed
review in `gerfs-continuation-contract-01/review.json`. Its query legitimately
validates raw pivot values, and X is an input/output. Those reads must be
distinguished from old FERR/BERR reads when adding its missing observation.

### Hosted completion for the integrated continuation

Revision `45b8bb5073d72afb3af208a25c71c91deb3ed52a` was ordinarily pushed.
`geequ-hosted-audit-01/audit.json` records CI34644695593 passing19/19 and
CodeQL34644695420 passing. Selected-family34644695416 completed with static/
shared LP64 each212/245 (33genuine mathematical failures), and true ILP64
each216/245 (29failures). No test was skipped. The prior29failure outputs are
byte-identical to the GECON hosted baseline; the four new LP64GEEQUB failures
contain88assertions matching local raw values. The19added selectors comprise
17family checks and two public-header checks. Each provider passes111tests,
and all five PT public examples pass. Hosted merge
`eb529589cee585c66bdc73dc788ce913560f2ee7` has the exact feature tree
`ad6e7b680798c3527cf13616f186bf4368f1d192`. This is affected hosted evidence,
not numerical acceptance or wider optional-provider platform admission.
