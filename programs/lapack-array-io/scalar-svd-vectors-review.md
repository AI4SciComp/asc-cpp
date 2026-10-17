# Real triangular 2-by-2 singular decomposition

The S/D LASV2 slice exposes two checked `asc::Lasv2` overloads through the
Dense LAPACK facet. The signed diagonal values and left/right rotations satisfy
`[cl sl; -sl cl] [f g; 0 h] [cr -sr; sr cr] = [large 0; 0 small]`, with
`abs(small) <= abs(large)`. This operation includes the singular vectors.
The separate LAS2 single-infinity prerequisite blocker remains open.

## Exact pinned operation

Both definitions at LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca` were read in full. They use scalar
arithmetic, comparisons, SQRT, ABS, SIGN and LAMCH; there are no loops, arrays,
INTEGER/CHARACTER/INFO arguments, hidden arguments, mutable SAVE state or
allocation. Their internal LAMCH call uses the already reviewed pinned machine
constant implementation. Nine mutable real pointers cross the native ABI.
Local integer and logical variables do not cross it. Both installed headers
omit LASV2; both actual archives define the symbols, and exact GNU11 LP64 and
global-ILP64 emissions agree with the private declarations without normalization.
A read-only search of 18 registered worktrees found no adapter or named commit.

Native prerequisites pass 77,824 cases per ABI: 65,536 finite triples and 12,288
triples containing exactly one infinity. Signed zeros, subnormals, minimum
normal values, scaling boundaries, mixed signs and values near maximum finite
are represented. All input and output guards remain intact. The comparison
uses independent widened symmetric eigenvalue and determinant formulas,
left/right unit norms and both off-diagonal residuals; the smaller value has
its own relative/underflow tolerance. Infinite inputs use explicit limiting
axes and dominant signs. No NaN or multiple-infinity native call is needed.

Among the finite cases, 2,048 per ABI have a coefficient slightly beyond one
in magnitude and still satisfy the independent equations; all are within
`1 + 16*epsilon`. There are 3,744 finite-input cases with infinite large output.
The source permits overflow close to the finite boundary; the comparison
retains the stricter requirement that its widened norm is at least maximum
finite when native large is infinite. No below-boundary failure was waived.
Sources, emissions, probes and exact provider digests are retained externally
in `completion-execution-01/lasv2-prerequisite-review-01.json`.

## Checked contract

Finite triples and exactly one infinity are admitted. NaN or multiple infinities
return local numerical/accuracy-warning status with unchanged outputs and no
native call. Each of six outputs is a live scalar of length and increment one
in host or admitted pinned-host storage. All reachable output storage and
provider/report metadata must be disjoint. Metadata overlap rejects before any
report reset or numeric access. Other structural errors reset the report and
preserve every output. By-value inputs may originate in output objects.

The native operation receives three private input copies and six NaN-seeded
outputs. Changed input representations, nonfinite small, NaN large, unsorted
magnitudes, nonfinite coefficients or coefficient magnitudes above
`1 + 16*epsilon` yield provider/partial/unusable status and withhold all outputs.
Epsilon denotes the C++ scalar type's machine epsilon. Otherwise the six values
publish together without clipping: finite large succeeds; either infinite large
returns numerical/accuracy-warning status with complete publication validity.
The adapter neither proves arbitrary finite tuples accurate nor replaces the
native algorithm. Wrong finite and zero tuples are explicit accepted fixtures.

There is no workspace, query, allocation, transfer or synchronization. Native
INFO, argument index, diagnostic index and factor family remain absent. Floating
point controls are unchanged, while ordinary arithmetic flags may be raised.
Concurrent calls share only an immutable provider; mutable scalars and reports
stay private. The pinned Fortran and BLAS internals are uninstrumented.

## Maintained validation

Tests cover the complete prerequisite grid, independent mathematics, exact
native fidelity, first/reused outputs, guarded storage, allocation audits,
four-worker concurrency, actual emitted signatures and installed public-only
consumers. Twenty-nine ordinary-return modes include every missing output,
invalid values, input mutations, ordering, coefficient threshold boundaries,
finite tuples outside the runtime accuracy contract and signed infinities.
Every wrapper first executes and checks a guarded real native call. Modified
values stay inside live private scalar objects.

Structural tests exercise six operands, all pairwise overlap directions,
metadata aliases, lengths, increments, device/managed placement and local
nonfinite rejection. No deliberate memory violation, crash observation,
ASLR-changing run or remote operation is part of this slice. Current results
are recorded in the external LASV2 profile and integration records; these
checks do not close the earlier provider or platform admission blockers.

Eight ordinary Release/Debug static/shared profiles pass 88/88 tests with no
failures or skips. Maintained emitted-signature probes pass 77,824 cases in
each actual ABI. Metadata tests reject 444 single-precision and 222
double-precision aliases; 57 nonfinite triples per scalar reject locally.
Each of four relocated packages passes its 77,824-fixture public consumer.
All six implementation/test translation units pass strict analysis. Initial
view initialization and function-size diagnostics were corrected before the
passing profile matrix; original diagnostics and successful rechecks remain
recorded without weakening any case or tolerance.

Four standalone header checks, ten package-manifest tests and three architecture
checks pass. Source and installed public surfaces each contain 172 headers.
The actual CI selector retains all 1,983 preceding tests and adds eleven
operation tests plus two header checks, selecting 1,996 tests with its required
case guard passing. Documentation covers 172 headers, 2,844 public members and
34 explicit programme pages without warnings. These local results support two
callable-unverified real routes and two reviewed modes; they grant no Reference
verification and do not alter Native20 or earlier execution outcomes.

The installed consumer also passes strict analysis, bringing the strict total
to seven translation units. Both ABI export comparisons add exactly two Lasv2
overloads and remove none. All five source/installed header surfaces pass.
