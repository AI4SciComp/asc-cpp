# Reference LU layout implementation and evidence

This is the incremental-lu-v3 checkpoint, not completed P04 or a full provider.
The registered S/D/C/Z GETRF, GETRF2, GETF2, GETRS, GETRI and GESV routes now
accept both layouts with independently chosen factor/RHS layouts. The existing
GEEQU/GEEQUB routes already accept both layouts: 32 reference operations remain
in progress, distinct from eight native LU implementations and zero routines
with final normalized contract-bound verification credit.

## Checked packing and publication

The private `internal_layout.h` helper accounts for all simultaneous numerical,
integer and packing bytes before execution. Each row-major matrix occupies
exactly rows times columns live caller scalar objects in kLayoutConversion;
column-major matrices remain in place. Actual foreign leading dimensions are
ABI-narrowed plan dimensions. Original ASC leading strides are separate
ASC-sized plan options, preserving empty LP64 matrices with wide original
row strides without fabricating huge backing memory.

GETRF/GETRF2/GETF2 publish packed LU and converted pivots only after acceptable
raw INFO and valid foreign pivots. GESV publishes documented partial factors on
singularity but does not unpack an unsuccessful RHS. GETRS and GETRI unpack
successful results only. Provider-defect outputs remain unusable; row packing
is not published after negative/excess INFO or invalid returned pivots.
Column-major foreign outputs cannot be rolled back and are never represented
as transactional on a provider defect. Original const solve factors and matrix
padding remain unchanged. Workspace mutation is explicit and distinct from
destination publication.

GETRI preserves the actual LWORK=-1 query contract, using valid full descriptors
and raw pivots without a hidden packing buffer. Execution validates the bound
plan and supplied minimum/preferred work rather than requerying. Packing counts
are ASC-sized storage counts, not foreign LWORK. All input/workspace overlap,
alignment, placement, capacity and stale-plan checks remain active.

## Frozen candidates and retained failures

Candidate01 is tree `a7578fc435c86b0377b6d05ddb53083317422f47`, archive SHA256
`8014bbdfd67e91f32ba077b9b8f2b63301fee9d360fc47ca344d38693c70bda5`.
Its provider-free Debug suite passes 259/259, zero skips. Both provider builds
fail because new test assertions referred to snapshots local to a different
lambda. These are build failures, not zero-test passes. The separate sanitizer
compile initially failed on warnings in upstream C-linkage complex headers;
classifying external headers as system includes permits compilation without
disabling ASC warnings. The single-real process passes, but the double process
aborts on an invalid empty-matrix test descriptor (leading dimension zero).
Complex processes after that abort were not executed or credited.

Candidate02 corrects only the two test fixtures, retaining every assertion and
unchanged production bytes. It is tree
`8c53d8d41893191b090a4e2847b8894b5b6a8e62`, archive SHA256
`893f7c56b49ddc656465ffa49b9b14dec6d472e5d8ea7d368454a787b450cb6c`,
based on commit `1739a0bb2d2e7fca2c95632dcf1362dfb2bfc394`. Raw records and
logs reside in external `p04-lu-v3-01` and `p04-lu-v3-02`; these immutable
source archives exclude unregistered Matrix Market, native Cholesky/QR and
advanced provider work.

Candidate02 all-four-scalar standalone layout executables pass with Clang19
ASan/UBSan in both LP64 and true ILP64. The two new adapter translation units,
current foundations, fault injection and test probes are instrumented, linked
to the instrumented baseline Core/Dense archives from p03-p04-d3675f3. The
system Fortran/LAPACK/BLAS/runtime libraries are not instrumented. This is eight
actual scalar/ABI processes, not eight CTests or whole-provider instrumentation.
Strict Clang18 checks pass the two corrected test files; candidate01's unchanged
two adapters and installed consumer also pass strict Clang18. Candidate02
Doxygen checks pass 68/68 headers, 1351 public members and zero warnings.

The integrated LP64 and true-ILP64 full suites each pass 285/285 tests with
zero skips, including the actual installed mixed-layout consumers, component
isolation, BLAS and Random regressions. These records are preserved alongside
the failed candidate01 records in `verification-checkpoints.json`.

## Independent numerical and failure checks

The new test covers all four scalars, all three factor algorithms, both layouts,
rectangular and blocked 67-by-67 factorizations, binary scales -20/0/20, raw
pivot guards and independent sequential-permutation LU reconstruction.
GETRS uses independently formed N/T/C right-hand sides, both factor/RHS
layouts and three RHS. GETRI checks both inverse products using actual minimum
and preferred queried workspace. GESV covers zero/one/three RHS, checking that
zero RHS still factors nonempty A. Singular selected cases verify actual raw
INFO, zero-based diagnostics, partial reconstruction and unchanged failed RHS.

Empty dimensions and wide ASC row strides execute without a foreign call where
the contract specifies one is unnecessary. Pure workspace arithmetic tests
check size_t aggregate overflow without allocating or forging large spans.
Injected provider defects are explicitly distinct from real numerical failures.
ELF malloc/calloc/realloc/aligned-allocation wrappers and C++ allocation probes
observe first, repeated and rejection paths; guards detect padding/workspace
overruns. This exact static runtime scope does not imply shared-provider or
other-platform allocation coverage.

SHA-256 identities:

| Artifact | SHA-256 |
| --- | --- |
| Private layout helper | `138fab5a7f8d9e7782cadf929e5fdfb853d54745ce70f1da7ec9eeefc77c6205` |
| Reference LU | `03f8ed2d3d8093488182c89e0f563b6c84de618da123c5a990d8140f378828c3` |
| Reference LU expert | `a9807ba1c2487b56811e4e87b6d8b2616e1a6b14c7ad5dd44af9fe0575b070d1` |
| Layout test | `273a46798842cf9393246e4409d8f1012fcdc0e90beb77aabb09b2ae8874eb73` |
| Original LU test | `829c2df6557e48c97e5bb2a36d8e661cebb9a08732f5bc81da65723c680ed32e` |
| Installed consumer | `aaab07f20b343eda9bc7bdccb475479f3c65bcb8f3ffcebaea1eb2ff790035d1` |

Pinned source/provider identities remain those in `provider-abi-review.md`.
Coverage mapping SHA256 is
`f6f3bc4a058fe57b7ffda03f6f98f30a442a16e551b6d60f1aefcd0d05de642a`.
The LP64 GEEQUB subnormal mathematical-success gate, complete source-derived
2113-routine denominator, license/notice owner review, shared/platform gates and
the rest of P00–P11 remain unchanged. No such gate is closed by layout support.
