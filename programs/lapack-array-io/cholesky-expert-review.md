# P05 full-storage Cholesky expert slice

Status: all 20 typed source routes implemented and exercised in isolated
LP64/true-ILP64 correctness tests; final isolated checks pass and parent
integration remains pending. This review owns only the 20 source rows S/D/C/Z POCON,
PORFS, POSVX, POEQU and POEQUB (16 computational plus four driver rows).
The other required P05 structured and expert routes remain outside this slice.

## Isolation and immutable inputs

Worktree: sibling `../asc-cpp-p05-expert`, branch
`feature/lapack-p05-expert`, base
`dde439dacc1b377b7064717daabbf7b572106b21`.
Only new expert headers, sources, private helpers, tests and this review are
owned here. No existing CMake, ABI, schema or coverage record is modified.
No commit or push is authorized for this delegated slice.

The external evidence root is sibling `../asc-cpp-evidence/lapack-array-io`.
Frozen test dependencies are `p04-lu-v4-01/source.tar`, SHA-256
`3dfc3c97c5ea1dbfe40eca363df95609e1b3251eaddea77daa557dfc8fa0a4a8`,
and `p05-cholesky-stride-04/source.tar`, SHA-256
`799882d1f15c997ba2236e351e4a3d4e10109f7e149473f806fe0156227abdd0`.
The corrected Cholesky dependency source has SHA-256
`8262edc54fd742a4040f3575d332ad789dd036181f4bd4151530f46eec0826b1`.

Reference LAPACK is the locked commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, source-input manifest
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
The LP64 build identity is
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
the true ILP64 build identity is
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
Neither provider export presence nor generated declarations count as ASC
capability. The immutable required denominator remains 2113.
The exact inventory SHA-256 is
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`.

## Source-derived implementation decisions

- POCON, PORFS and POSVX use fixed caller workspace, not fabricated LWORK
  queries: real WORK=3*n plus IWORK=n; complex WORK=2*n plus RWORK=n.
  Both estimator branches evaluate provider INTEGER 3*n in LACN2. Checked
  arithmetic must cover that expression independently of byte-size checks.
- POSVX FACT=N/E calls LACPY on the selected triangle before POTRF. For
  complex input this otherwise reads the ignored imaginary diagonal.
  Complex A therefore has explicit mandatory caller packing in those modes,
  including column-major A. Packing reads only selected offdiagonals and
  real diagonal components. FACT=F does not perform that copy; its original
  A reaches LANHE/HEMV/PORFS, which consume real diagonal components.
- FACT=E publishes packed A only when the actual source EQUED is Y.
  B scaling precedes factorization and survives positive factorization INFO.
  Such failure leaves X/FERR/BERR untouched, retains RCOND=0 and selected
  partial AF, and never authorizes successful factor reuse.
- Row-major partial AF publication preserves previous imaginary diagonal
  components. It does not promise bitwise equivalence with unspecified
  column-major ignored output. Successful AF diagonals are actual real
  Cholesky factor coefficients; supplied factors are never normalized as
  Hermitian input.
- POEQU/POEQUB read only real diagonal components. POEQUB preflights finite
  diagonals before the source LOG-to-INTEGER path. Finite nonpositive
  diagonals retain actual source failure INFO and partial scale outputs.
  SCOND uses the square roots of the original diagonal extrema, including
  POEQUB; it need not equal the ratio of its radix-rounded scale extrema.
- Original ASC leading dimensions belong in plan options, not foreign-sized
  dimension entries. Effective packed leading dimensions are separately
  checked against the selected integer ABI.
- The compiler/STL/complex/character boundary remains the previously attested
  GNU 11.4/libstdc++ 20230528 development subset. This does not establish
  portability to an unprobed toolchain or other provider ABI.

## Verification and next task

Final code/test candidate: external `p05-cholesky-expert-05/source.tar`, SHA-256
`ae55b56e29776eed1a6d5b5e6f9f246cc2508facca1ceee1edf649cf3b72d217`.
This review is a subsequent evidence annotation, not a claim that the archive
already contained the final annotation. The external source manifest records
the exact 17 implementation/header/test identities separately:
`p05-cholesky-expert-05/implementation-files.json`, SHA-256
`5ed18ca3d503ed22f4088b8b50e4ce2ce64b5d444dabbfe188a8088bfd919e1c`.
The matching new-file patch SHA-256 is
`6b16ff1b926103de217c2203b9c94422a103ea3ef9c2cbe4ea3952afd40f866c`.
Candidate 05 differs from candidate 04 only in the public SCOND clarification;
all numerical and strict checks are nevertheless rerun on candidate 05.

The final candidate has four actually executed suites, with all four real/
complex scalar types and both true integer ABIs. Debug, standalone ASan/UBSan
and Release with Linux/glibc allocation interposition are separate lanes.
The sanitizer lane instruments new ASC wrappers/tests and the frozen Cholesky
dependency, not the previously built Core/Dense/provider archives or shared
Fortran runtime. This is not a whole-provider sanitizer claim.
All six final lanes pass four of four tests with zero skips: 24 actual CTest
executions. Their raw logs are external
`p05-cholesky-expert-05/ctest-{lp64,ilp64}-{debug,asan,libc}.log`.

Per lane, explicit counters record 64 successful equilibrations, 48 analytic
condition cases, 64 nonpositive-diagonal equilibration failures (negative and
zero, first and last), 128 refinement cases, 384 FACT=N/E drivers, 256 FACT=F
drivers, 128 direct successful-factor POTRS reuses, 32 FACT=N partial failures,
32 FACT=E partial failures, 48 blocked/scaled drivers, four real-backed wide
ASC-stride composite cases, 65 no-call preflights and 15 synthetic provider
defects. Additional assertions exercise empty/scalar/zero-RHS results, real
INFO=n+1 warnings, unused scales, NaN/Inf POEQUB rejection and metadata alias
rollback. These counters are not 2113-row full-profile closure.

Independent oracles use fixed integer SPD/HPD matrices and a closed-form
bidiagonal-factor n=65 family, long-double factor reconstruction and residuals,
analytic diagonal reciprocal conditions, scale identities and exact radix
powers. Scalar PORFS additionally checks its analytically derived
FERR=2*epsilon and BERR=0. Refinement begins with a known 0.125 perturbation;
the required small solution error proves improvement. FERR elsewhere is an
estimate, not falsely promoted to a universal rigorous bound.

All independent matrix layouts, both selected triangles, ignored NaN
offdiagonals/imaginary diagonals, padding, output-only AF/X and conditional
FACT=E A/B publication are exercised. IWORK starts with nonzero high-byte
markers; actual LACN2 writes must produce full-width signed +/-1 values for
the selected LP64/ILP64 ABI. Pure checked-count boundary tests use ordinary
integers, never fabricated large backing spans. Public wide-stride cases
have one genuinely backed element and ASC leading dimension INT64_MAX.

Structural tests check changed plans, insufficient capacity, alignment,
operand/workspace/metadata overlap and placement before foreign calls or
numerical mutation. A serial provider cannot execute on nonempty pinned,
managed or device spans. Zero-byte unused pinned workspace remains accepted.
The neutral foundation workspace policy is not changed. Synthetic GNU wrappers
inject negative, minimum-integer and impossible INFO; these prove defensive
reports, not LAPACK mathematical correctness, and install no XERBLA handler.

## Allocation and exact source closure

The existing C++ new probe and the external Linux/glibc diagnostic separately
observe complete calls, including first numerical operations and failure
paths. Dynamic interposition has positive controls for direct libc malloc
and shared-libstdc++ operator new before observation; no numerical warm-up
is hidden. It covers malloc/calloc/realloc and common aligned-allocation
entries, not arbitrary undisclosed private allocation mechanisms.

Source-conditioned archive closure is recorded in external
`p05-cholesky-expert-02/source-closure-{lp64,ilp64}-03.json`: twenty exact root
symbols and 128 included archive members per ABI, with source hashes and
library hashes. XERBLA argument-error edges are excluded only under the
reviewed preflight invariants; unrelated ILAENV branches are not traversed.
The only reached ILAENV case is ISPEC=1 for POTRF, with pinned NB=64. The
3*n estimator bound also dominates this branch's blocked-loop endpoint.

Actual runtime leaves differ by ABI. LP64 uses `__powidf2`/`__powisf2`;
true ILP64 uses `_gfortran_pow_r8_i8`/`_gfortran_pow_r4_i8`. Complete recorded
disassembly shows bounded multiply/divide loops with no outgoing calls.
POEQUB's finite-positive LOG operands exclude logarithm domain handlers;
all log/logf resolver alternatives and arithmetic kernels were inspected.
Other observed leaves are cabs/cabsf and memcmp/memcpy/memset. No generic
runtime name pattern is treated as allocation proof.

The reviewed runtime identities are libm SHA-256
`df621c68dbfed7e843434ef2faedb9f4d4b0543ad161e9a55eaf4d4ce2443176`,
libgcc_s SHA-256
`fc9d43b2f6c20e53b009238f767c5b949d202389e20de9e202ea684b4ba3729a`,
and libgfortran SHA-256
`f7379c9331de9d66c03b439a070e8056b7419fc4213320d647952b998d026b30`.
The raw artifacts are `p05-cholesky-expert-02/runtime-leaf-disassembly.log`
and `runtime-ilp64-power-disassembly.log` in that same directory.

Source-level semantic review uses the pinned implementations, including
[DPOSVX](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dposvx.f),
[CPOSVX](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/cposvx.f),
[CPORFS](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/cporfs.f),
[DPOCON](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/dpocon.f)
and actual S/D/C/Z POEQU/POEQUB variants. Their complete exact hashes are in
the closure records, not inferred from LAPACKE names or generated precision
assumptions. This is self-review, not invented independent owner approval.

## Remaining integration gates

No new CMake/ABI/central coverage registration or installed consumer is changed
in this isolated worktree. Parent integration must import the exact new files,
register all four suites and four optional headers, update contracts and
evidence atomically, and rerun the integrated optional component and installed
consumer gates. The isolated C++20/no-exceptions header checks use no foreign
include paths; strict Doxygen checks all 56 public function declarations.
Final Clang 18.1.8 strict analysis passes all nine new translation units with
no user-code diagnostics under the unchanged repository policy. All 17 code
files pass clang-format, and the Markdown checker passes 97 repository files.
The first candidate-05 tidy invocation started before the compilation database
existed; its failed log is retained as `p05-cholesky-expert-05/tidy.log`.
The successful unchanged-source rerun is `tidy-02.log` in that directory.
Exact commands, log hashes, binary hashes and scoped limitations are recorded
in external `p05-cholesky-expert-05/verification-summary.json`.

This slice does not complete P05, P09 or P11: all remaining structured storage,
indefinite/pivoted variants, additional expert/auxiliary rows, full-reference
profile closure, broader supported build configurations and release/owner/
license gates retain their established scope and state. No approvals, remote
push, PR, merge or release are claimed here. Exact next integration task:
import this slice only after the final external verification summary is green,
then register and test the actual optional component; do not restart its design.
