# P05 band factor ignored-component amendment

Status: frozen isolated amendment candidate 01; available scoped verification
is complete. Parent integration and installed-consumer rerun remain required.
This corrects ignored complex diagonal reads in the previous twelve-route band
factor/solve slice. It neither adds routines nor completes the separate twenty
PBSV/PBCON/PBRFS/PBSVX/PBEQU routes.

## Exact scope and preserved evidence

The original `asc-cpp-p05-band` worktree and its fourteen-file freeze remain
unchanged. Parent review identified that row-major PackBand copied each whole
complex diagonal even though factorization ignores the imaginary component.
Old mathematical/fidelity passes did not prove that no-ignored-read contract.
The defect and old successful/failing evidence are retained, not relabeled.

The correction exists only in the copied dependencies of worktree
`asc-cpp-p05-band-expert`, based at
`9ffb62c183b22a1232790a01c0940a5d731630c2`, tree
`1939ba7b255b40d1d67d57b2360b540900345400`. New expert code is excluded
from the immutable amendment snapshot and all amendment build/test targets.
There are no shared CMake/ABI/coverage/state edits, commits or pushes here.

External evidence directory: `p05-band-imaginary-IAVa56W7`, relative to the
program evidence root. The eight changed dependency files are frozen in
`amendment-candidate-01.sha256`, SHA256
`d0f9cd2ad0618f5aefbec55195efe018a74aa836c103944af17c2c20b4048db1`.
The exact source snapshot is `source-candidate-01.tar`, SHA256
`6fca24f3d5ea3880017a4658925493791fbd721c25b60560f9aa4bf99b5bdcff`.
It is the base tree plus the fourteen copied band dependencies, not a claim
that the uncommitted snapshot is a verified repository commit.

## Source-derived publication boundary

Factor packing now takes an explicit Hermitian-input flag. PBTRF/PBTF2 pass
true and construct packed complex diagonals from `.real()` and zero only.
PBTRS passes false and retains every raw triangular component. No original
imaginary diagonal is read to save or restore it.

Row factor publication assigns the real component separately. It assigns the
imaginary component only within the actual source-normalized diagonal prefix;
outside that prefix the original imaginary component is left unwritten.
Actual libstdc++ 20230528 float/double complex component setters use separate
`__real__`/`__imag__` assignments, not a read of the other component.
The audited `/usr/include/c++/11/complex` bytes have SHA256
`5816151c9816930182a0cb8d7932dcae2480d57cc14b5329b1d8254d9c041c43`.

Let f be positive one-based INFO and n the order. On INFO=0 the prefix is n;
invalid INFO does not publish packed output. For unblocked PBTF2 (including
PBTRF when kd<=64), f=1 gives prefix 1. Otherwise it is
`max(f, min(n, f-1+kd))`: every previous successful column j invokes
HER on j+1 through min(n,j+kd). HER has alpha=-1 and positive order on
that route, so its early return is impossible. Even an exactly zero vector
component executes `A(j,j)=REAL(A(j,j))` (DBLE for Z), normalizing the
whole update diagonal.

Blocked PBTRF uses the pinned NB=32 when kd>64. Let
`i=1+32*floor((f-1)/32)` be the failed block start. If i=1, POTF2 writes
only through f. Otherwise the preceding complete block's A22/A33 HERK
updates form a contiguous normalized prefix through min(n,i+kd-1);
take the maximum with f. These HERK calls have alpha=-1, beta=1,
K=IB>0 and positive output order. Their quick-return condition cannot hold.
The no-transpose path real-normalizes before its coefficient loop, including
all-zero coefficients; the conjugate-transpose path assigns a real diagonal
unconditionally. POTF2's failed pivot is also real-assigned. The helper uses
subtraction-clamped arithmetic, never overflowing f-1+kd or i+kd-1.

Exact unchanged source hashes at pinned LAPACK commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`:

| Source | SHA256 |
| --- | --- |
| SRC/cpbtrf.f | b0d0f9dd19a5169b186088f9788c15e9d641d1c7febee325c04813b666389e6b |
| SRC/zpbtrf.f | 344a27d33a0c168abb9a2799b4e182cb14d4bdd65231d06eaf8f9b686ef38a9d |
| SRC/cpbtf2.f | 6866521b4f051da45fd21afdf44ab0ae7fe141b920b8190ea6c2ee66a7d4c8be |
| SRC/zpbtf2.f | 5dc0e526428254e287c5856598b30581c79155522211f933a5e5c0d42cc3143f |
| SRC/cpotf2.f | a7bf200debad3107a1ff2cb541794e70ba81eb66b5f20f59f0b898f9f0f8a7c5 |
| SRC/zpotf2.f | cb9da562fa5b1397b8da7da1287453cebc46c69a1e3dcab25801e341d352200f |
| BLAS/SRC/cher.f | 6292fba77fbb7d26ab8ddd1e275216bf3454c5a8635e9834eaa584d746f46873 |
| BLAS/SRC/zher.f | c7b9a2b3218315d1e9795eee2d84ce07b07279358a93e11317963c208b3ead9a |
| BLAS/SRC/cherk.f | 42cb24e734a4e73ac3df8a163bcf4ac17478acf4a207d65d0f1822fe4701e159 |
| BLAS/SRC/zherk.f | 50f483bb8c74bcdeda1cffe781f56ce2de6339e798b153fa6290cf9a256dfd1c |

## Completed scoped evidence

All existing tests and assertions remain. Pure prefix tests add f=1,
32/33/64/65, kd=0, kd>=n and near INT32/INT64 maxima without fictitious
matrix backing. Direct pinned source fidelity adds all four scalars, both
triangles/layouts and blocked/unblocked failure boundaries with n=136, kd=65
and zero off-diagonals; n exceeds f+kd so untouched imaginary sentinels remain
visible. Existing ignored-diagonal NaNs and raw PBTRS negative/nonreal/NaN
fidelity cases remain unchanged.

The typed fault endpoint inspects actual packed diagonals before mutation
and requires zero imaginary components for row factorization. Compiling the
old frozen adapter with the new tests fails both complex validation tests
for each real ABI: 40 assertions per scalar, CTest 0/2, exit 8. Logs
`test-old-control-lp64-01.log` and `test-old-control-ilp64-01.log`
are intentionally failing negative controls, not successful numerical tests.
Their endpoints never call a foreign routine.

Corrected Debug, Release and ASan/UBSan each pass 14/14 with zero skips for
both actual LP64 and true ILP64. Both strict Clang 18 tidy lanes pass with
the unchanged repository configuration; all enabled warnings remain errors.
These sanitizer targets instrument ASC's band adapter/tests only. Existing
Core/Dense/provider-selection archives, Fortran/BLAS and runtimes are not
instrumented and do not acquire sanitizer-closure credit.

The exact repository `BuildDoxygen.cmake` and `check_doxygen.py` pipeline
passes on the frozen snapshot: 80/80 headers, 1599 documented public members,
HTML plus XML, all configured graphs, zero warnings and no path leakage.
The existing graph capacity is 256; no warnings/checks were disabled.
Format and public/private-header self-containment/no-exception checks pass.
`evidence-summary-candidate-01.json` records the exact pre/post dependency
hashes, provider identities, raw-log hashes, scoped commands and limitations.
Final hash checks pass for all eight amended files in both the worktree and
immutable snapshot, and for all fourteen original files in the old worktree.
Exact provider binaries/ABI, source counts and the foreign static call graph
are unchanged from the prior band slice; no new foreign symbol or allocating
route is introduced. New execution allocation probes remain active.

## Remaining gates and restart

Root must perform independent public-only installed-consumer and current-tree
full integration review. The next implementation task here is to update the
new PBSV packing to use the same reviewed policy, finish the new eight-route
evidence and continue PBCON/PBRFS/PBSVX. No provider patch, mathematical
limitation disposition, native capability, license approval or full-P05 claim
is implied by this amendment.
