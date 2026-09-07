# Ordinary Sylvester slice: source and interface review in progress

## Resumed V8 verification and finite coefficient correction

Uncorrected frozen V8 tree `37be13252b938e8761842ca6060d31bfb03cf9b6`
passes full LP64/true-ILP64 431/431 each, including eleven actual relocated
installed consumers each; provider-free Release277/shared279 also pass,
zero skips. Debug has276/277 passing and one Core build-tree consumer timeout;
its unchanged targeted retry passes1/1 without increasing the timeout.
Both affected all-ASC-C++ sanitizer selections pass60/60 and all eight strict
Sylvester TUs pass per ABI. These are actual command results, not evidence
that the previously untested finite-large mathematical mode was correct.

Independent review then reproduced the blocker
TRSYL-FINITE-DIAGONAL-OVERFLOW for all scalars and both ABIs; root read the
entire independent probe and all actual outputs. The new pre-entry check
bounds real/imaginary diagonal sums after the exact sign/conjugation choices,
before packing or any numerical mutation. It returns numerical not-run with
unchanged outputs and absent INFO; no provider source or equation is changed.
The public contract also now accurately describes preservation of the report
on any metadata alias rejection, matching existing tests.

Corrected tree `2affa1ab28f0be368cc42ca96ed5179aa104237f`, archive SHA-256
`9196bac1eab0dfa5ee41ba1d1ece02f495a8d6e6b249c17ec526eca9d833a6e4`,
is external `p08-sylvester-overflow-01`. Both ABIs pass15/15 affected tests,
including the public consumer, in Debug and all-ASC-C++ Clang19 ASan/UBSan,
zero skips. Foreign Fortran/reference-BLAS/runtime archives are unsanitized.
The same new four scalar contract tests all fail against old V8 LP64, with
their original assertions intact. New cases exercise every legal operation,
both signs, polarities, components and layouts, untouched scratch, real
foreign-call counts and finite-boundary/cancellation controls. Both changed
TUs pass strict Clang18 per ABI; Doxygen documents88/88 headers and1751
public members with zero warnings. An initial external runner record-directory
collision was corrected; all original runner logs remain, and no test ran
under that failed invocation.

Independent correction review found no further flaw in this bounded gate.
Its separate scalar-helper probe checked992107 binary32 and999030 binary64
finite pairs without a missed actual overflow under the audited default
rounding mode. That is additional gate evidence, not a proof of the complete
provider or alternate rounding modes. Exact corrected full integration and
installed packaging remain for the next combined snapshot. Required large
finite mathematical solves, TRSYL3/TGSYL and remaining P08 scope stay open.

Root-owned isolated worktree `../asc-cpp-p08-sylvester`, branch
`feature/lapack-p08-sylvester`, base
`31e93935f8db4ac685c2224abf7860a058040117`. This is dependency-satisfied P08
work using existing P01/P04 infrastructure while v7 integration runs elsewhere.
It does not close P07, P08 or any other required family. No shared registration
or coverage records are changed from this worktree.

Exact inventory confirms all S/D/C/Z TRSYL and TRSYL3 at `SRC/<name>.f` are
required. Initial bounded implementation owns the four ordinary TRSYL routes;
TRSYL3, generalized TGSYL and all other inventoried matrix-equation operations
remain required. Root has read the full P08 and applicable P01, architecture,
cross-cutting verification and review sections of the actual runbook, full
S/D/CTRSYL sources, and the full C/Z source diff. No MdeCpp material is used.

## Frozen initial interface direction

Use a new optional Dense provider header `lapack_sylvester.h` with ordinary
typed `QueryTrsylWorkspace`/`Trsyl` overloads, existing const/mutable full-matrix
descriptors and operation enum, a typed plus/minus sign, and an explicit
underlying-real scale output. Retain exact source equation
`op(A) X + sign X op(B) = scale C`. Real N/T/C and complex N/C are distinct;
complex plain transpose is invalid. Inputs A/B are upper (quasi-)triangular,
not automatically reduced to Schur form. Real nonzero subdiagonals encode
nonoverlapping canonical 2x2 blocks, with equal diagonals and opposite-sign
offdiagonals; validate their structure before numerical mutation.

Source `xLANGE('M')` reads full A/B. Thus both layouts require explicit caller
packing for A/B, copying only upper entries and real first subdiagonals and
filling implicit lower zeros. No ignored triangle or padding is read. C is
direct column-major or explicitly packed row-major; no dense expansion of a
sparse/band representation is involved. The query is formula-only and reads
no numerical values; no fictitious LWORK query or INFO=0 is fabricated.
Plans bind all shapes/layouts/original strides/options/provider identity.

All four sources evaluate M*N in provider INTEGER when nonempty. Real block
indices and DO/min expressions require safe successor values. DDOT/CDOTU/C
strided row cursors require source/operation-specific bounds, not merely last
address bounds. Canonical packed A/B strides and actual foreign C stride
control these checks; original ASC strides remain in the plan. Query/empty
exceptions and auxiliary source closure are still being audited.

SCALE is never silently divided out. Source INFO=1 means perturbed local
coefficients were used: preserve scale/X and report an accuracy warning, not
successful solution of the unperturbed equation or singular factorization.
Invalid source INFO/scale remains a provider defect with raw diagnostics.
Allocation, alias/placement, partial publication, source-conditioned call
closure, real ABI, numerical residual and installed tests remain required.

## Current verification

The four typed adapters, public contract and private checked counts are now
written. Initial math tests cover nonzero known solutions, scaled-equation
residuals, both signs, every legal real/complex operation, independent layouts,
real 2x2 blocks, scale below one and INFO=1 warnings. They audit C++ and libc
allocations during calls. No runtime pass is claimed yet.

First frozen diagnostic `p08-sylvester-diagnostic-MCxeTLOs` has archive
`8bec57cc00c1a61165222d63ccec761b0a12e626f8fe35b02094c184884d41a7`.
Both actual-ABI compilations fail the same signed-versus-unsigned test-oracle
comparison (`size_t` allocation count against integer zero). Root changes only
that zero literal to `std::size_t{0}`; old archive/logs stay unchanged. No
numerical test executed in that attempt. Freeze the correction and run actual
tests, then add preflight, fault, pure boundary, ABI and installed evidence.

Second diagnostic `p08-sylvester-diagnostic-d6RVqTFH`, archive
`e813641c50a59ffc216464ae135e65e63f5a1de7a051f4b8c11815c39a7b0e33`,
passes all four actual scalar executables in both true ABIs. Per ABI there
are1732 S,1732 D,388 C and388 Z nonempty numerical cases, with independent
scaled residuals/forward solutions and no observed call-time C++/libc
allocation. These are direct Release diagnostics linked to committed-v6 ASC
dependencies, not full/installed/sanitizer verification or routine counts.

First combined contracts freeze `p08-sylvester-contracts-8TY8kqeH`, archive
`d8308a2b57d4d1d8690c3f65993bce7b48db6ad6d0057e685c175d36da0763e9`,
executes9 CTests per ABI: all four mathematical tests and integer test pass,
all four contract tests fail, zero skips. LP64's genuine one-element-backed
INT64_MAX unused-stride regression exposes original ASC strides incorrectly
placed among foreign-bounded identity dimensions. The correction keeps them
in the signed ASC option key instead; actual foreign dimensions remain checked.
Both ABIs also reveal an invalid test oracle expecting shape failure for a
call whose A/B already alias: execution rejects operand alias first. Retain
that alias fixture with its actual invalid-argument contract and add a separate
genuinely backed wrong-C-shape fixture with disjoint operands. No shape/alias
check or numerical assertion is removed. Rejection diagnostics now identify
the exact invoking test line. All original failed source/logs stay unchanged.

Initial full scoped strict checks finish five TUs per ABI: adapter passes;
four test TUs fail style checks. Corrections preserve the independent rollback
snapshot as an explicit value copy, split an oversized preflight test, isolate
intentional invalid-enum casts with narrow test-only analyzer exceptions, and
replace a nested INFO expectation with an equivalent switch. The scaled
float predicate is split into separate strict-positive and less-than-one
assertions after the analyzer incorrectly treats their conjunction as an
impossible integer interval. Unused array inclusion is removed; all four GNU
test wrappers now additionally have exact function-pointer type assertions
against pinned lapack.h declarations. Re-run strict and unchanged numerical
oracles on a fresh corrected snapshot; no old failure is relabeled.

## Corrected scoped results and remaining integration

Contracts snapshot `p08-sylvester-contracts-g73W72tm`, archive
`90a2ab2a2e1fa665591fbf1baec7fdeec01da7b986e32b7e603a9e4af4e5298f`,
passes 9/9 tests in each true ABI with zero skips, and all five strict TUs
per ABI. Alias snapshot `p08-sylvester-alias-MX9v4Gek`, archive
`6d913a7393b40e3194444c594ae12d5fb6a5360275933dea724dae532f470cdd`,
adds all six operand pairs, eight workspace roles against each operand, and
all four metadata objects against workspace. Complex scale alias fixtures
use the standard-permitted real component of a genuinely backed complex
array; no unrelated-object typed write is used. Rejected report aliases
preserve their original native-info sentinel.

Each ABI passes 13/13 tests, zero skips, in GNU Release, Clang 19 ASan/UBSan,
and GNU Release with dynamic libc allocation interposition. The sanitizer
lane rebuilds all applicable ASC Core, Dense and registered provider C++
sources; the linked Fortran, reference BLAS and runtime libraries are not
sanitized. Every one of the 13 dynamic-allocation processes confirms both
direct libc and shared-libstdc++ allocation positive controls. First foreign
calls are not warmed away. Those observations supplement, rather than
replace, the exact-source and linked-runtime closure review.

The first alias strict runner accidentally included a literal `+` as a
seventh filename. All six real TUs passed in each ABI, but each overall
attempt failed for that nonexistent seventh file. Preserve those records.
The corrected `strict-02-*` records pass all six actual TUs per ABI against
unchanged product bytes. The ordinary header separately compiles standalone
without exceptions or foreign include directories under GNU 11 and Clang 19
for both configured ABIs. Full frozen-source Doxygen checks pass 85/85
headers, 1663 documented public members, zero warnings.

The public-only consumer in `p08-sylvester-consumer-llyXs68F` passes one CTest
per ABI and strict Clang checks. Each process checks 416 independently
constructed nonzero solutions, all legal operation/sign/layout combinations,
rollback and padding, plus four actual INFO=1 warnings. It is a direct-link
diagnostic, not yet an installed-package test. Its source is independently
hashed in that record's metadata and is not part of the earlier alias tar.

The direct ABI probe in `p08-sylvester-abi-gP6PS5Ed` passes one CTest and strict
Clang checks per ABI. Each process enters the four actual foreign symbols
60 times without ASC adapters or fault wrappers. It checks full-width
INTEGER and INFO sentinels, two explicit trailing CHARACTER lengths, complex
interleaving through nonreal solutions, SCALE guards, matrix guards/padding,
both signs, all legal operations, INFO=1, and empty quick returns. TRSYL has
no external LOGICAL, pivot, or workspace-query arguments; evidence for those
different interfaces is not inferred from this probe.

## Conditional source and runtime closure

The exact static graph has 45 objects per ABI, including XERBLA; the checked
execution closure has 44. The four TRSYL argument checks are the only route
to XERBLA in this graph. ASC admits only each scalar's legal operations,
sign +/-1, nonnegative representable dimensions and valid foreign leading
dimensions. Nonempty A/B use leading dimensions m/n; C is validated or
normalized to m. Empty calls are handled before foreign entry. Thus the
error-print/STOP branch is excluded by explicit preconditions, not by
blindly allowing a runtime symbol. There is no ILAENV, callback, dynamic
workspace query, or precision/provider substitution in this closure.

The original full S/D/CTRSYL sources and complete C/Z diff were reviewed,
along with real local 1x2/2x1/2x2 solve helpers and their scalar differences.
Real block validation guarantees finite canonical nonzero offdiagonals
before those helpers use block structure. xLANGE('M') reads the full logical
squares, motivating explicit A/B packing and zero-filled ignored entries;
its norm choice does not use WORK or enter the LASSQ branch. Conservative
static reachability still retains LASSQ and its dependencies in the audit.
The pinned xLAMCH uses intrinsic machine constants, not saved mutable
initialization. BLAS calls have positive unit or validated row strides.

The integer proof separates foreign expressions from ASC addressing:
nonempty m and n have representable successors; m*n fits native INTEGER;
row-dot terminal cursors `(m-1)*m+1` for untransposed A,
`(n-1)*n+1` for transposed B, and `(n-1)*ldc+1` fit the same ABI. The
checked packing squares, sum, optional m*n C buffer and byte products fit
ASC limits separately. Pure tests exercise both actual 32/64-bit limits
without forged huge descriptors. Single-column C's original unused stride
is retained in the plan but is not narrowed as a foreign argument.

Both exact object audits leave only cabs, cabsf and memset unresolved into
the standard runtime. Actual executable linkage identifies libm SHA256
`df621c68dbfed7e843434ef2faedb9f4d4b0543ad161e9a55eaf4d4ce2443176`
and libc SHA256
`e01b1ce7be2987f3b8560e26d0df2623f9dd5cec17be923ae28a785bc0d32d50`.
`runtime-numeric-01` follows cabs/cabsf into both actual hypot implementations;
the reviewed branches have no allocation or escaping runtime call.
`runtime-memory-01` follows the memset IFUNC resolver and every selected
implementation target; no unexamined indirect transfer is accepted. These
are exact Linux runtime evidence, not a portable guarantee for arbitrary
toolchains or libraries. All closure and runtime records have exit zero.

Full root registration, fresh combined builds, actual installed consumer,
complete per-mode evidence normalization, and independent final review are
still required. Meaningful nonfinite-input behavior has not been exhaustively
verified; the implemented postflight warning policy must not be described
as a universal correctness claim. Only four ordinary TRSYL routes are
implemented here; TRSYL3/TGSYL and all other P08 families remain required.


## Root native INFO-output correction

Four TRSYL native INFO locals now start at full-width MIN. Actual-call test-only INFO destination omission reproduces old wrong success4/4 tests perABI:104 invalid success/status/scale/publication profiles,312 changed packed cells. Corrected source passes Debug/Release/all-ASC-C++ ASan each16/16 perABI and strict3TUs eachABI, format4 and root coverage validation. All prior assertions and finite-diagonal overflow gate remain; no upstream patch. Scoped bases are exact V9-02 Release and V10-01 instrumented ASC archives; foreign unsanitized, no new full-suite/installed claim. Six link-map relinks are byte-identical to executed contract-test binaries and pull only the corrected Sylvester member.

The correction is exactly four initializer changes and additive test fault/mode sweeps. All four actual pinned source routines assign INFO on entry; omission is explicitly injected evidence. All valid operation pairs, signs and layouts have104 ordinary positive controls and104 withheld-output cases per ABI. MIN is retained without signed-minimum negation or an invented native argument; row-major C and scale are not published, direct column-major C retains actual provider mutation and is reported unusable.

External source identities and31 command records are audited in `p08-sylvester-info-output-01/completion-audit-01/audit.json`, SHA256 `6a019819be0883bd4026d0dda9e7630875919c99ed2aef98abcd6bbf5f8bd7fb`. The four production/test files were imported byte-for-byte, and all four mapping rows refresh their artifact hashes atomically. Subsequent root coverage-import command also passes. The V10 full suite is not evidence of this later numerical-source correction. Current combined revalidation follows with GT integration.
