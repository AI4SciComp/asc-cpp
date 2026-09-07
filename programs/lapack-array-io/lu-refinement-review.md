# Reference GERFS bounded implementation evidence

This slice implements actual S/D/C/Z GERFS calls, not completed P04 or full
provider coverage. The candidate sources are frozen for numerical and strict
checks below; integration and installed-profile verification remain separate.
No owner/license approval or full mathematical capability is inferred.

## Source and implementation scope

The exact 3.12.1 source/build/ABI identities in `provider-abi-review.md` apply.
All four complete GERFS argument sections and executable bodies and all four
authoritative `lapack.h` prototypes were read before binding. The implementation
is in new `providers/lapack_lu_refinement.h` and
`src/dense/lapack/reference_lu_refinement.cc`; it does not change the earlier
frozen equilibration/GECON or root-owned layout work.

GERFS accepts original immutable A, corresponding raw immutable LU/pivots,
original immutable B, mutable approximate X and separate contiguous
underlying-real FERR/BERR. The raw-factor caller guarantees common provenance;
the API does not fabricate a successful-factor certificate. Both layouts are
independent for each of A/AF/B/X; caller packing concatenates only row-major
matrices in that order and unpacks X after successful provider execution.
N/T/C complex modes are kept distinct. Row-major source leading strides are
ASC-sized metadata in the plan options; actual packed foreign LDA values are
the narrowed dimension fields. A real empty descriptor with source strides
above INT32_MAX proves LP64 acceptance and stale-stride plan rejection without
fake huge backing. The previous conservative implementation fails that test
in `logs/p04-lu-refinement-stride-old-test-01.log` (build log with the same stem).

Workspace formula queries make no foreign call. Real WORK is 3*n scalar
entries; complex WORK is 2*n scalar entries plus n real RWORK. The kInteger
region contains n converted foreign pivots first, followed by a disjoint
n-entry foreign IWORK subregion for real routines; complex uses only the
pivots. This preserves the common validator: kPivotConversion is ASC index_t
width, not foreign LP64 width, so it is not used for foreign conversion here.
The first LP64 diagnostic run exposed and retained that configuration error;
the correction does not change common workspace semantics.

All seven operands and live workspace spans are disjoint. Dimensions, leading
dimensions, pivot values/family, output lengths/strides, selected integer ABI,
packing products and aggregate byte requirements are checked before writes.
The LACN2 3*n and GERFS n+1 intermediate bounds are checked. Foreign integer
lifetimes are established in caller byte storage without allocation.

Empty n or nrhs follows the source: X is unchanged, FERR/BERR become zero and
no foreign call occurs. Otherwise exact-zero U is rejected before conversion
or packing, returning ASC-detected kNumerical/kSingular with unchanged
X/FERR/BERR and absent native INFO. No tolerance is introduced. Negative or
unexpected positive upstream INFO is a provider defect, preserving raw INFO.

## Preserved finite-input error-estimate limitation

For scalar n=nrhs=1, A=AF=B=minimum_normal/1024, raw pivot 1 and initial X=.75,
the same pinned routines in both ABIs refine X to 1 and produce the correct
guarded BERR, while returning raw INFO=0 and nonfinite FERR:

- S/D: FERR=+Inf for N/T/C.
- C/Z: FERR=NaN for N and +Inf for T/C.

The mathematical weighted estimate for this fixture is finite, approximately
2048. The source's estimation path applies unscaled GETRS before multiplication
by its small error weights, exposing an intermediate inverse overflow.
Finite-estimate mathematical success for this case remains an unmet gate.
A separate A=AF=B=2*minimum_normal fixture exercises the same SAFE1/SAFE2
guarded branch with finite FERR approximately 1, and BERR approximately 1/3.
No upstream patch or changed source identity is assumed.

Raw failed finite-expectation logs are retained:
`logs/p04-lu-refinement-ilp64-test-s-01.log` and
`logs/p04-lu-refinement-{lp64,ilp64}-test-s-02.log`.
The standalone `lu-refinement-direct-probe.cc` calls the pinned S/D/C/Z symbols
without ASC on exactly those two scalar fixtures and all three transpose
modes. Both ABI direct probes passed their distinct NaN/Inf/finite expectations:
`logs/p04-refinement-direct-{lp64,ilp64}-{compile,run}-02.log`.
The first direct LP64 probe had expected Inf for complex N too and failed;
its actual NaN evidence is retained in log revision 01.

The implementation preserves INFO=0 without inventing a foreign failure.
After unpacking completed X, ASC scans the estimates without allocation.
Nonfinite FERR/BERR returns kNumerical/LapackOutcome::kAccuracyWarning;
negative estimates take precedence and return kProvider/kPartialResult.
Both retain all raw estimates and X, report called_provider=true,
kDocumentedPartial validity and the first offending RHS index. Individual
finite nonnegative estimates retain their routine-specific validity; X has
completed refinement, not gained a new universal accuracy/finiteness promise.
Separate injected NaN FERR, Inf BERR, negative FERR and negative BERR checks
verify the exact report, preserved solution versus an uninjected real call,
unchanged other estimates and no allocations.

## Available diagnostic verification and exact next work

External scripts `build-lu-refinement.sh` and `build-lu-refinement-asan.sh`
compile the immutable snapshot `lu-refinement-frozen-28p9o8nQ`, built upon
`lu-condition-frozen-whvbQ45C`, not root's live layout edits. Optimized
LP64/true ILP64 and all four scalar processes passed
`logs/p04-lu-refinement-{lp64,ilp64}-compile-06.log` and
`logs/p04-lu-refinement-{lp64,ilp64}-test-{s,d,c,z}-06.log`.
Both-ABI ASan/UBSan compilation and all four scalar processes passed
`logs/p04-lu-refinement-{lp64,ilp64}-asan-compile-02.log` and
`logs/p04-lu-refinement-{lp64,ilp64}-asan-test-{s,d,c,z}-02.log`.
Only adapters, foundation and tests are instrumented; baseline Core/Dense,
Fortran, BLAS and runtime libraries are not. This is not a full instrumented
provider claim. Earlier snapshot `lu-refinement-frozen-ZOxNGka5` preserves the
output-quality correction before the leading-stride fix, with passing
optimized revision 05 and sanitizer revision 01 and its own identities.

Strict header compilation, clang-format and HTML/XML Doxygen passed
`logs/p04-lu-refinement-{header,format,doxygen}-frozen-02.log`.
Clang-tidy on the final frozen sources passed
`logs/p04-lu-refinement-tidy-frozen-02.log`; the earlier frozen snapshot also
passed revision 01.

Exact final candidate SHA-256 identities:

| File | SHA-256 |
| --- | --- |
| `include/asc/dense/providers/lapack_lu_refinement.h` | `c38c5b7a41a6fd3016a31234cd219b767a61f5cc58af324c5eab0490eca68826` |
| `src/dense/lapack/reference_lu_refinement.cc` | `df9b2b62d0ff0b2e72c6388062084605a65754a9c5bb7f838f1c43b0d1c1010a` |
| `tests/dense_lapack/lu_refinement_test.cc` | `150784bcec74d161255b79d9cf3b966b1c911aa25b9c2ab8958eed321ae295b5` |
| `tests/dense_lapack/lu_refinement_test_support.h` | `b771b2e9c4a77bef804a3eb35313d89947d828730afa1992d6ca419954f6ea6c` |
| `tests/dense_lapack/lu_refinement_faults.h` | `b1cbf9db073b3a86b3cb080dbb1470c6533ffa9274a0b1e38efd30e1106a80e0` |
| `tests/dense_lapack/lu_refinement_faults.cc` | `4418072bbc0fed1a7c37119d494423dd890999a8542694cdaa87d4c47bc996e1` |

The independent archive traversal `audit-lu-refinement-closure.py` starts at
exact s/d/c/zgerfs symbols in each pinned static LAPACK/BLAS pair and passed
`logs/p04-lu-refinement-{lp64,ilp64}-static-closure-01.json`. With the checked
argument-error XERBLA branch excluded, only cabs/cabsf/memcpy/memset are external
leaves. C++ new and malloc/calloc/realloc/aligned allocation wrappers observe
zero allocations in first, repeated and failure paths. Neither static closure
nor those probes imply shared-library dynamic-runtime interposition coverage.

Tests cover 1/3/8 order, 1/3 RHS, N/T/C, all sixteen A/AF/B/X layout
combinations, binary scales -60/0/60, repeated refinement and preserved
original/factor/pivots. Independent long-double residual and componentwise
backward-error equations check the solution; FERR is an estimate, not an
invented exact bound. Tests also check exact workspace formulas, role units,
redzones, unused buffers, first/repeated/error allocation probes, empty and
exact-zero-U behavior, output alias/shape/stride/device rejection, stale plans,
bad pivots, and injected negative/unexpected positive INFO. A passing fidelity
test for nonfinite FERR does not satisfy the unmet finite-estimate gate.

Next: integrate the frozen registration and identities. Installed component
isolation and full-profile
integration remain separate required gates. Continue GESVX afterward: all
four complete source documentation/bodies and pinned lapack.h prototypes have
now been read; typed mode-specific binding proposal was sent to the integrator
but no GESVX capability is claimed yet. GECON's separate leading-stride
correction is frozen and verified in `lu-condition-review.md`.
The rest of P04 and all later required packages remain in scope.
