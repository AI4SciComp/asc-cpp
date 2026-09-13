# Explicit provider workspace-context admission correction

This is a corrective P01/P04/P05 audit, not a routine-coverage expansion or a
full integrated product verification. All implementation work is isolated on
`feature/lapack-placement` in its separately linked worktree. The integration
feature worktree and original dirty release worktree are not modified here.

## Setup and exact preimage

The integrator supplied tree `eebeb3a3632e66f9d07d77dc3f522cb88dc404f1`
and parent `dde439dacc1b377b7064717daabbf7b572106b21`. The explicitly
authorized local commit-tree setup created anchor
`a3e1e61d9258f23f4e864d594f2b96e2770dc245`, with no ref change in the
integration worktree. This is an unverified local setup anchor, not a new
verified product identity. No further commit or push is authorized in this
bounded owner task.

Before placement edits, the five frozen LU integer-correction files were
imported through apply_patch. Their exact source identities are:

- `reference_lu.cc`: `887d1bb721557ec716d34d9829952bee787eecd916dd1b77bb9d41df9e6ccdff`.
- `reference_lu_expert.cc`: `55bd3cd61caa6bfc60b8b6b8cac24d4e725045fac822a0925648a399d8925426`.
- `internal_lu_counts.h`: `834a371b8b637f787a8dcb1c08526370f564ab408f35e4cbbf285a9c5fa8a777`.

The two corresponding tests are imported unchanged. These pre-existing
integer corrections are not attributed to the placement fix.

Reference Cholesky inputs were imported from external
`p05-cholesky-stride-04/source`, including its public/type header and private
helper/test dependencies. Its corrected source SHA256 is
`8262edc54fd742a4040f3575d332ad789dd036181f4bd4151530f46eec0826b1`
and provider header SHA256 is
`0125c2b9ed0ee5bcd9335f768abed5ff663521fa1d3c43fe1c94bd76afe8a166`.
Those earlier stride corrections and unmodified imported declarations are
not new placement API changes. Native Cholesky/QR and provider QR are outside
this task.

The complete placement preimage is preserved externally under
`provider-placement-7XFHFr5a/source-before` and `source-before.tar`, relative
to the existing program evidence root. Old-code negative controls must use
these exact inputs, not an earlier LU integer/Cholesky stride version.

## Audited correction

Core's current Serial::CanAccess admits kHost only. The neutral public
ValidateLapackWorkspace instead recognizes both host and pinned-host as
CPU-addressable. Those policies serve different purposes; the neutral
foundation remains unchanged.

All seven audited adapter files delegated scratch admission solely to the
neutral validator. This includes original LU, expert LU, equilibration,
condition estimation, refinement, the three GESVX modes, and reference
Cholesky. QueryGetriWorkspace performs an actual foreign query and had a
separate direct validator path requiring the same correction.

The new private `internal_workspace_context.h` first checks every nonempty
supplied region against provider.context().CanAccess, including unused roles,
then preserves the existing neutral capacity/alignment/alias/identity/ABI
validation. Zero-byte region metadata retains its previous neutral policy.
All seven private plan validators and the separate GETRI query use this
helper before conversion, packing, numerical writes or foreign calls.
Existing routine-specific report-reset/alias-failure behavior is retained.
No public API, generic workspace policy, provider identity or floating-point
algorithm is changed. The zero-byte compatibility assertion specifically covers
unused workspace-region metadata, not a new permission to pass inaccessible
operands. Device/managed rejection remains unchanged in the neutral validator.

The operand audit separately identified four missing context checks. Original
GETRF and expert GETRF2/GETF2 admitted writable pivots on host or pinned-host
without consulting the provider. GETRS read factor pivots, and GETRI query and
execution read raw pivots, without admitting their storage through the provider.
All four checks now precede pivot validation/conversion and foreign calls.
GESV inherits the checked GETRF operand route. These defects predate the imported
LU count correction: the anchor's expert query delegated to the original
GETRF query, which already had the same host-or-pinned-only condition. The
count refactor duplicated, but did not introduce, that admission behavior.

The remaining operand audit found existing explicit admission at these sites:

| Adapter | Already checked operands |
| --- | --- |
| Original/expert LU | Matrix/factor matrix and right-hand sides |
| GEEQU/GEEQUB | A and both real row/column scaling vectors |
| GECON | AF; norm and result are scalar values/plain host references, not tagged views |
| GERFS | A, AF, raw pivots, B, X, FERR, BERR, before pivot reads |
| GESVX N/E/F | All eleven operand/metadata spans, before selected scale/pivot reads |
| Reference Cholesky | Every tagged matrix/factor/right-hand-side operand |

Neutral raw-pivot/factor view factories remain unchanged. Their broader CPU
storage validation does not authorize a particular provider context.

Actual AGENTS/CONTRIBUTING, full applicable runbook mission/protocol/architecture,
P01/P04/P05 and cross-cutting sections were reread. The live
[Google C++ guide](https://google.github.io/styleguide/cppguide.html) and
[Python guide](https://google.github.io/styleguide/pyguide.html) were retrieved
on 2026-09-07; the established repository-specific compatibility rules and
Clang 18 configuration remain applicable.

## Exact candidate and test scope

The external evidence directory is `provider-placement-7XFHFr5a`, relative to
the program evidence root. Complete preimage and candidates are retained as
`source-before`, `source-candidate-01`, `source-operand-test-01`, and
`source-candidate-02`, each with its corresponding tar archive. Candidate 02
is the final currently frozen production/test source, not a committed product
identity. Its placement-owned SHA256 identities are:

| File (under src/dense/lapack unless noted) | Candidate 02 SHA256 |
| --- | --- |
| reference_lu.cc | `9a63417370b14ab0988a89d088c0f9f33594cf5aeedf3b7b0a6c384357ad40d1` |
| reference_lu_expert.cc | `1d11e67e74457b4a3a454b06417798075491c0f7b4e65cd7f0e8ceb6c1ce22eb` |
| reference_lu_equilibration.cc | `ff417ba716bd686b5c011f462e7bafc98d24ee495241cb65c5b1facfd8d6e924` |
| reference_lu_condition.cc | `42979060a1098d4ab770796f98c4c9eba1cceca4f0d5aa73192cef5ea708da38` |
| reference_lu_refinement.cc | `0fa4a951799a3bba0251d916e05503f09adf54f032a69d15eb24550ebfc13e55` |
| reference_lu_driver.cc | `59191180a374ebe8076c99170d6cab5f9bf41cc13f7b2881b395ff19c3bec5f8` |
| reference_cholesky.cc | `607780868ca22886644486a235483bb01a9d9ab8145e0e7f16051c5b026756ba` |
| internal_workspace_context.h | `86c8606e1590945f35781c041249380214ac4e0f39cee9bf1309d311ea2fdb94` |
| tests/dense_lapack/provider_placement_test.cc | `a0a86a9869d60e065d8f1904b4ba980783d46c06e9dfcf7622266bd33d28f31a` |
| tests/dense_lapack/provider_placement_support.h | `e8e47fda46a38b333a5170e3941b981810daf5eb06f0770b9483e0d37b36189f` |
| tests/dense_lapack/provider_placement_routes.h | `88d297045727b64b8c1ed0d01674e028b1e2ad572c05172766c1b23aa712b128` |
| tests/dense_lapack/provider_operand_placement_test.h | `c6f3e10c7969e733a5b455c5eb2900d7d2082e27e74db6078761cc36d011c9b5` |

The other five adapter preimage hashes, in addition to the two imported LU
files above, are:

| File | Pre-placement SHA256 |
| --- | --- |
| reference_lu_equilibration.cc | `7b21e989423fa4d09874e96c11fd4aee3c81d1f47ca9a937d0f66f27d6e776c2` |
| reference_lu_condition.cc | `9bcfaca3a712db83e0647c9cc183231b040db24eac286b78aa164c95af811212` |
| reference_lu_refinement.cc | `df9b2b62d0ff0b2e72c6388062084605a65754a9c5bb7f838f1c43b0d1c1010a` |
| reference_lu_driver.cc | `a0daaf90e677dccde805af4e7f7e37564d174c5ce72c87c385f8804d017cb43b` |
| reference_cholesky.cc | `8262edc54fd742a4040f3575d332ad789dd036181f4bd4151530f46eec0826b1` |

The new executable covers twenty actual typed routes: GETRF, GETRS, GETRF2,
GETF2, GETRI query and execution separately, GESV, GEEQU, GEEQUB, GECON,
GERFS, all three GESVX modes, POTRF, POTRF2, POTF2, POTRS, POTRI, POSV.
Each scalar has forty host numerical controls across both layouts, plus 960
workspace rejection cases (twenty routes, two layouts, eight supplied roles,
three rejected tags). All four scalars run with each actual provider ABI.
Fourteen additional host and fourteen pinned pivot cases per scalar exercise
both query and execution in the seven applicable LU routes/layouts. Neutral
CPU workspace acceptance and unused zero-byte pinned scratch compatibility
have explicit controls.

Every buffer is real, fully sized host backing. Pinned/device/managed metadata
tests admission, not physical device execution. The old pinned-accepting
controls may call the provider, but use valid fully sized arrays/workspaces,
actual successful factor provenance, and valid pivots; no fake nonempty
backing or unsafe pointer reaches a foreign call. New rejection cases require
kMemoryAccess, complete unchanged operand/workspace/statistics bytes,
called_provider=false, absent native INFO/argument/index, unchanged-output
validity and retained provider/routine identity. Source audit establishes that
the guard precedes the adapters' marked foreign calls; the new placement test
does not claim an independent foreign-call interposition counter. Existing
family fault-injection tests remain in the numerical regression lane.

C++ allocation probes and static-link C allocation wrappers surround queries,
preparation, successful executions and rejections. These scoped probes do not
claim to intercept private allocations inside an uninstrumented runtime DSO.
The host controls check the independent 4I factor/inverse/solve/equilibration
oracle, condition=1, and finite nonnegative error estimates. The unchanged
family suites retain their broader residual, tiny-input, fault and sentinel
coverage; this admission fixture is not their replacement.

## Retained evidence

- `test-old-lp64-01.log` and `test-old-ilp64-01.log`: expected CTest exit 8,
  four of four scalar tests fail on exact pre-placement production. Each log
  contains 1,280 pinned workspace-admission case failures; all eight roles,
  both layouts and all twenty routes are exercised.
- `test-operand-old-lp64-01.log` and `test-operand-old-ilp64-01.log`: expected
  CTest exit 8, four of four scalar tests fail. These use the already corrected
  candidate-01 workspace production with the added operand tests, isolating
  the still-missing pivot checks rather than conflating the two defects.
- `test-release-lp64-01.log` and `test-release-ilp64-01.log`: earlier
  candidate-01 regressions pass 35/35 each; these do not verify the later
  operand correction.
- `tidy-candidate-01.log`: retained strict failure for an over-wide private
  test enum; candidate 02 uses explicit uint8_t. No assertion was removed.
- `harness-failure-02.md`: four normal candidate-02 shell lanes exited 2 on
  an unterminated quote. They are not verification credit. A syntax-checked
  frozen runner uses fresh normal `*-03` lanes.
- `test-asan-lp64-02.log` and `test-asan-ilp64-02.log`: candidate 02 passes
  35/35 each, no skipped tests. ASan/UBSan use actual Clang 19 and instrument
  the seven adapters, foundation and test translation units. Prebuilt Core,
  Dense, upstream Fortran/BLAS and runtime dependencies are not instrumented;
  this is scoped sanitizer evidence, not whole-program instrumentation.
- `format-candidate-02.log`: Clang-format 18 dry-run with warnings as errors
  passed on the placement-owned source/test files.
- `test-release-lp64-03.log`, `test-release-ilp64-03.log`,
  `test-debug-lp64-03.log`, and `test-debug-ilp64-03.log`: final candidate 02
  passes 35/35 in each lane, zero skipped tests. Each runner exits zero.
- `tidy-candidate-02.log`: Clang-tidy 18 passes the seven production source
  files and the new four-scalar test translation unit with the repository
  header filter and warnings as errors. Additional explicit owned-header
  checks pass in `tidy-private-headers-candidate-02.log` and
  `tidy-private-helper-candidate-02.log`; no new suppression was introduced.

The 35-test regression denominator is four new scalar placement tests,
twenty-eight unchanged original LU/layout/expert/equilibration/condition/
refinement/driver scalar tests, the imported LU count test, and two imported
reference Cholesky/stride tests. Both ABIs use the exact pinned provider
archives and audited ABI headers, not a typedef-only ILP64 simulation.

## Provider and tool identities

The pinned source remains commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`,
tree `7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, tag object
`5ebe92156143a341ab7b14bf76560d30093cfc54`, inventory input manifest
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
LP64 provider build identity is
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`;
true ILP64 is
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
The audited lapack.h SHA256 remains
`225f20409a7d6674953f5af1de8c47188bb792ca11d46d465bb28dde917fcafc`.
Exact dependency identities are retained in the existing provider ABI review.
This lane also records the exact linked Core/Dense, LAPACK/BLAS, ABI config,
lapack.h, libstdc++, libgfortran and libquadmath bytes in the external
`dependency-identities-candidate-02.txt` (SHA256
`4e9a946478535364689fcf52499ff07fd4632a5de1a4fb7fd3ccd6c60c0019fc`).
The immutable final source archive `source-candidate-02.tar` has SHA256
`b749747709a1cbd6a337e1cebbcf9c7835f99317d3c88bee880aad8ba67b78e3`.
This final review is maintained separately from that immutable code snapshot;
the snapshot's earlier review text is not the final verification summary.

Normal lanes use GCC 11.4 with C++20, strict warnings and exceptions disabled.
Clang-format/tidy are 18.1.8. Scoped sanitizers use Ubuntu Clang
19.0.0 (++20240717031316+351a4b27da7d-1~exp1~20240717151452.1809),
the audited libstdc++ build, and the same real LP64/ILP64 archives.
The external CMake harness SHA256 is
`3187566b46f729e20398a7ca7fffc8688e578b21a43828760e0d6e4bc749deb5`;
`run-candidate-02-frozen.sh` is
`8d446b958a0921a7fab98afc1c1fec1077d8d5efc4ee06550de09e95e6d26171`.

## Integration handoff and remaining gates

The scoped corrective implementation, its negative controls, both-ABI
Release/Debug/scoped-sanitizer regression lanes and strict checks are complete.
The exact next task is for the integrator to import only the seven adapter
deltas, one new private helper, four new test files and this review. Imported
LU count and Cholesky stride inputs remain separately owned pre-existing
dependencies. This isolated owner made no product commit, push or shared
registration change after its expressly authorized setup anchor.

Registration needs one executable using provider_placement_test.cc, existing
allocation_audit.cc and dense/allocation_probe.cc; link ASC::dense_lapack and
the established C allocation wrappers, then register s/d/c/z invocations.
The new private support headers and helper are not installed public API.
No public-header/ABI inventory update is required by the placement delta.

To reproduce a scoped lane, from the external `provider-placement-7XFHFr5a`
directory use the frozen runner with a fresh output name, for example:

```sh
bash run-candidate-02-frozen.sh source-candidate-02 source-candidate-02 \
  64 review-ilp64-01 Release ON
```

After actual target registration, run the integrator's complete frozen LP64,
ILP64 and provider-free suites, installed consumers, and manifest/docs gates;
the external 35-test harness is deliberately not credited as those full gates.

Full integration, installed consumers, component isolation, other-family
admission, cross-platform/provider/license owner gates, and the existing
GEEQUB/GERFS/GESVX/LAQGE tiny-input mathematical limitations remain required
outside this bounded correction. No numerical limitation is cured or hidden
by rejecting inaccessible storage, and no additional LAPACK inventory row is
credited by this work.
