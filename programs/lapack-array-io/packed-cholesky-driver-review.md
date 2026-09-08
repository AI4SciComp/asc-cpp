# Ordinary packed positive-definite driver candidate

The ten candidate05 files implement actual S/D/C/Z PPSV and explicit metadata
queries under [the frozen admission](packed-cholesky-driver-admission-review.md).
They remain external and unregistered until the current PPTRI v23 slice is
committed. Native PPSV factors every positive order, including zero RHS. Only
zero order completes locally. Both layouts use explicit caller packing;
complex input imaginary diagonals are ignored. Native INFO and source-specific
partial-factor publication survive, with B unchanged on nonpositive minors.
No provider patch, refactorization substitute, rescaling or native credit occurs.

Audit `3b89b1597405d5fe617cb7356d054307a771470b8c848d59f5745fdc7bb0f7cb`
and its seal bind 90 original records. Final six GNU Release/Debug and ASC-only
Clang19 sanitizer configurations each pass 5/5 tests with zero skips and no
sanitizer diagnostics. They compile 60 fresh adapter/test/support objects,
explicitly reusing and rehashing the earlier 65 primary TUs per lane. This is
not fresh root compilation; the actual candidate source has 1095 files, from
v23's 1085-file baseline plus ten owned additions. Foreign archives remain
unsanitized. Fourteen exact-input strict checks, four normal/no-exception
isolated headers and current ten-file formatting pass.

Each lane passes 14,784 public workflows and 441,841 checks: four scalars,
N0/1/2/3/5, RHS0/1/3, both triangles and independent A/B layouts, seven dyadic
scales, repeated query-plan use with restored original matrices, and ignored
complex imaginary diagonal NaNs. Independent known Cholesky factors, widened
infinity-norm reconstruction, normalized solve residuals and forward solutions
use scalar-epsilon/dimension tolerances without additive-one denominators.
First/last nonpositive minors verify exact partial A and unchanged B, including
zero RHS. Every factor/RHS padding slot and packing guard is checked.

The 2,976 failure profiles and 16,513 checks cover real/repeated and injected
calls, full-width omitted/partial/invalid/positive INFO, exact argument and
hidden-length mapping, all preflight/metadata/plan/workspace/placement cases,
actual A/B overlap, and scoped C++ plus wrapped static-libc allocation counters.
Native single-column LDB is compact N; original ASC B stride remains separately
bound. Positive injected INFO is labeled synthetic partial data; real numerical
failures have their own independent expected factors and preserved RHS.

Linux protected containing arrays prove 192 metadata-only queries, 64 local
zero-order executions and 64 stale-plan rejections. Another 64 zero-RHS calls
factor A while the entire B array is inaccessible. Ninety-six native calls
with first/last nonpositive minors likewise leave protected nonempty column B
unread. Nonempty row B requires explicit pre-entry conversion and is not given
this conditional native-read claim. C++20 placement array new establishes
real containing lifetimes; full backing, all guard values and zero observed
C++ allocations are checked. Other platforms/concurrency and complete read/
allocation observations remain separate requirements.

## Retained failures and corrections

Candidate01 selected the old inverse adapter in its external CMake harness;
both Debug links failed. Candidate02 selects the driver TU with all six source
files byte-identical and passes 3/3. The original failed logs, actual compiled
inverse objects and harnesses are retained. No correct-driver execution is
credited to those failed builds.

Candidate03's new argument tests incorrectly expected original column LDB for
one RHS, producing 128 check failures in each Debug ABI. A debugger observation
on its original binary confirms N=2, NRHS=1, LDB=2 and hidden length1. Existing
common::Leading explicitly compacts a single column. Candidate04 corrects three
expectation expressions, including isolated stale-plan construction; all
assertions remain, and production is unchanged. Both Debug 5/5 runs pass.

Candidate04's memory test fails strict checks for an 81-line function and nested
fixture conditionals. Candidate05 moves the physical-slot loop to a helper and
uses ordinary branches for the same expected values. Its complete diff is bound;
every assertion and provider call remains. Both original strict failures remain.
All six final 5/5 runs and current strict checks pass after these corrections.

The 91 new records bring the normalized ledger to 3,630. The earlier admission
19 records are already normalized and not counted again. The released mapping
remains 334 Reference in_progress/1,779 not_started, native20
implemented_unverified and zero fully verified. Complete PPTRI v23 full suites,
audit/documentation and commit; then import these ten audited files and run
fresh root v24. All routine/mode/concurrency/platform closure, P00-P11, 70 prior
ordinary mathematical failures and 130 missing required XBLAS remain required.
