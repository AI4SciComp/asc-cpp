# Advanced LU integration checkpoint

This adds the twelve actual S/D/C/Z GECON, GERFS and GESVX routes to the
explicit optional Dense-owned provider, with the separate GEEQU/GEEQUB
empty-wide-row-stride correction. The profile is `incremental-lu-v4`:44
partially registered reference operations out of2113 required. No row is
promoted to fully verified. Native LU has eight implemented, not fully
contract-bound verified, rows. All other required families remain in scope.

The implementation self-review covers all three complete public headers,
sources and family reviews before registration. The provider identities,
workspace roles, actual foreign ABI, conditional input/output roles, raw INFO,
partial-output behavior and all independent operand layouts remain explicit.
No allocation, conversion, provider enablement or CPU/CUDA fallback is hidden.
This is not repository-owner, license, redistribution or release approval.

## First integrated candidate and retained failures

Candidate01 is Git tree `6146d7572dcc19f49e4764ce2f298c10d7484b5d`, based
on code commit `dde439dacc1b377b7064717daabbf7b572106b21`. External
`p04-lu-v4-01/source.tar` has SHA256
`3dfc3c97c5ea1dbfe40eca363df95609e1b3251eaddea77daa557dfc8fa0a4a8`.
The mapping SHA256 is
`7fb04db1e23199b8167e23d09616a4083c26838bea9dc3715a2a03096f5efbc9`.
Concurrent native/provider Cholesky/QR, LU helper and original-LU integer
corrections are excluded; their separate evidence is not this candidate's.

All four full builds succeed. Provider-free Debug and Release each execute271
CTest cases, with269 passing and2 failing. LP64 and true ILP64 each execute315,
with312 passing and3 failing. All have zero skips. The two shared failures are
exact public-header baseline comparisons: the three new entries were appended
out of the collector's lexicographic order. Their hashes were correct. Sorting
those entries restores the exact74-header comparison without removing any
header or comparison. All twelve new scalar numerical tests pass in each
provider suite. Strict Doxygen passes74/74 headers,1457 documented members,
zero warnings; the Markdown checker passes.

The third failure is the newly installed advanced consumer, which compiles
and links but has an invalid condition-independent forward-error assertion
after refinement of a strongly row-scaled fixture. Its original source SHA256
is `553407b4dd99bff48f6273917820a315b7570d069c4d13ef9c8f7bf0991e6788`.
Candidate01 and the diagnostic runs retain that failing assertion and result.
The older installed LU consumer passes. Package tests are not credited as
passing merely because compilation succeeds.

## Independent refinement-oracle correction

The real fixture has rows `[2^-6, 2^-7]` and `[32, 64]`. Its transposed
system has exact infinity-norm condition8193. For exact first RHS solution
`[1,3]`, starting from a one-percent perturbation, pinned SGERFS returns
`[0.99976563453674316,3]`, INFO0, FERR about0.00195332360 and BERR0.
The observed relative infinity-norm forward error is about7.81218e-5, while
the independently widened componentwise backward error is about1.90696e-8.
The exact inverse-norm residual bound is about1.56244e-4. The second RHS has
the same conclusion. Direct pinned Fortran calls without ASC reproduce both
ABIs identically. GERFS stops on rounded componentwise backward error, not a
universal128-epsilon forward error. A rounded BERR of zero is not proof of an
exact stored-system residual of zero.

The direct probe is external `gerfs-condition-oracle-probe.cc`; raw compile
and run logs are `logs/p04-gerfs-condition-oracle-{lp64,ilp64}-{compile,run}-01.log`.
Further installed diagnostic02 retains an analogous complex-float transpose
FACT=E forward-error assertion failure (absolute first-component error about
0.000805343). Neither failure is repaired by increasing a fixed tolerance or
modifying the provider's refinement operation.

The corrected consumer verifies the original exact dyadic A/B relation in
long-double complex arithmetic for every scalar, transpose and independent
A/AF/B/X layout. All four solves (FACT=N, standalone perturbed GERFS, FACT=E,
FACT=F) must have independently computed componentwise backward error at most
eight scalar epsilons, the conservative rounding allowance for this two-term
complex residual. FERR and BERR must be finite and nonnegative; BERR must
satisfy that allowance. The actual max-CABS1 relative forward error must be
covered by the returned FERR on these fixtures. This is a tested property of
these independently known examples, not a universal error-estimator theorem.
Zero denominators have explicit zero/infinity behavior.

A second complete well-conditioned fixture undoes the original row scaling.
It retains the128-epsilon componentwise forward-error assertion for every
solve/mode, in addition to the residual and error-estimate checks. The original
scaled fixture, the one-percent perturbation, all immutable-input checks,
factor reuse, forced FACT=E scaling and condition-norm oracles remain.
No test is disabled, skipped or reduced to a round trip. Diagnostic05 retains
an ILP64 harness link-path error (`lib64/` instead of actual `lib/*64.a`);
this is a failed diagnostic, not an ABI result. Strict-analysis failures and
their subsequent code-only style corrections are also retained externally.

## Remaining gates

The separate [condition](lu-condition-review.md),
[refinement](lu-refinement-review.md), [driver](lu-driver-review.md) and
[equilibration](lu-equilibration-review.md) reviews bind exact family source,
header, scalar-test and scoped sanitizer/allocation evidence. Tiny-input
GEEQUB, GERFS and unscaled GESVX mathematical-success limitations are still
open; faithfully translating their numerical warnings does not make those
mathematical gates pass. The source-specific original LU integer correction
has isolated passing evidence but is deliberately excluded from candidate01.

A later helper adversarial test also found that neutral workspace validation
admits pinned-host spans while the explicit Serial provider context admits
only host memory. Existing provider routines require a separate whole-facet
workspace/context admission correction and regression. This checkpoint must
not be interpreted as complete placement-contract closure. The generic public
workspace validator's documented host/pinned-host policy is not to be changed
to conceal a provider-specific admission check.

Full P00–P11,
normalized mode/contract evidence, all advanced/specialized routines, foreign
instrumentation/platform gates and owner/license review remain required.

## Corrected candidate02: available integrated verification passed

Candidate02 is tree `eebeb3a3632e66f9d07d77dc3f522cb88dc404f1`, external
`p04-lu-v4-02/source.tar` SHA256
`3b39468e3e13c0dbe66685c595fcdb167613f6529dcf67053a61691661a9f9cc`.
Its mapping SHA256 is
`955af4f0258eefa156d5e2434277724e8570fb7c6d729409a92b40a01be9fb77`.
Installed advanced consumer SHA256 is
`722a38e5958f54d02619084ff6e6c644c0b4e961cb0a8563248983611d631c02`.
Compared with candidate01, only the exact header baseline, consumer,
consumer-bound mapping hashes and program records differ. All production
sources, public headers and scalar numerical tests are byte-identical.

Fresh full LP64 and true ILP64 each pass315/315 CTests. Fresh provider-free
Debug and Release each pass271/271. All have zero failures/skips and include
the BLAS/Random regressions and installed component checks. Each provider's
relocated C++-only package executes both LU consumers (2/2), using only public
headers/exported targets; the no-Fortran-consumer checks remain active. This
is not a shared-provider or another-platform result. Strict Doxygen passes
74/74 headers,1457 documented members,zero warnings; Markdown and the exact
incremental inventory validator pass. Its2113 required denominator,44
reference in-progress rows and zero fully verified native/reference rows
remain unchanged.

All fifteen candidate01 and fifteen candidate02 configure/build/test/docs
records are sanitized into verification-checkpoints.json with original hashes;
raw commands, source-before/after identities, failures, logs and JUnit remain
external. No older scoped sanitizer result is relabeled as a new integrated
sanitizer run. The separately frozen source-audited original LU integer
correction, native/provider Cholesky/QR, LASWP/LAQGE and placement follow-up
are not part of candidate02. Those are exact next integration work, followed
by every remaining advanced/specialized P04–P11 routine and final gates.
