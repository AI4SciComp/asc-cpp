# Remaining owner decisions

This packet separates approval-controlled actions from ongoing ordinary
engineering authorized by the master continuation. PR #47 remains a draft.
No owner decision below is inferred from a successful build or provider-fidelity
comparison. Independent ready LAPACK work continues.

| Decision | Exact current scope | Required decision and retained evidence |
| --- | --- | --- |
| Notice/metadata amendment | Manifest `380a8792f8c23263439ac79e8c02bb1959089b674c1221c6397e9bc26585992c`; proposed notice `9ebeaf30a78e77f5fa4a4ba94ed66c68bdb92acd1dfdc8b1456e0c114ffbed41`. PR47 reread during this continuation still has no new owner decision. | Review that packet before its approval-controlled publication/adoption. This does not block authorized first-party feature commits or confer permission to bundle provider/runtime files. |
| Pinned Reference PPSVX numerical strategy | Four unregistered Reference rows; five overlapping causes over 44 unique scalar/mode/value cases. The separately named robust capability is already integrated under its own experimental decision. | Reference numerical acceptance still requires a strategy addressing scaled inverse condition estimation, weighted estimation, reciprocal/norm evaluation, residual/weight evaluation and safe equilibration. [Exact existing disposition](packed-cholesky-expert-disposition.md). Robust success provides no Reference credit and does not authorize changing the provider. |
| Pinned Reference PTTRS scalar arithmetic | Four partial solve rows, n=1, A=B=a at denorm_min and twice denorm_min for S/D/C/Z. Exact X=1; the pinned scalar reciprocal path produces Inf or (Inf,NaN), INFO=0. Eight mathematical cases, 16 layout executions, 48 assertions and four failed processes per matching profile share one cause. | Select an explicitly named permitted numerical strategy if remediation is desired. Preserve the Reference failing gate; no silent substitution, dependency patch or requirement waiver. [Exact PT disposition and direct-call evidence](positive-tridiagonal-review.md). PT delivery and DSGESV/ZCGESV remain independent. |
| Pinned Reference PTCON scalar condition arithmetic | S/D/C/Z, N=1, D=ANORM=a, empty E, both orientations. At denorm_min, twice denorm_min and min()/8 the inverse overflows and RCOND=0; at max() the rounded subnormal inverse's reciprocal overflows and RCOND=Inf. Exact RCOND=1 in every case; direct calls match INFO0. Per ABI: four failed processes, 32 assertions, 16 scalar-labeled cases/eight underlying-real fixtures, two arithmetic causes. | Preserve condition-one and finite-diagnostic gates. Numerical acceptance needs an explicitly approved scaled inverse and reciprocal/norm strategy; no provider patch, silent substitution or waiver. These categories overlap existing condition-estimator limitations. [Exact PTCON disposition](positive-tridiagonal-condition-review.md#numerical-disposition). Safety, installed-use and independent families continue. |
| Pinned Reference PTRFS correction and residual-weight arithmetic | S/D/C/Z, N=1, A=DF=B=a, initial X=1, both factor orientations. Three tiny values produce X/FERR/BERR NaN through zero-residual correction with an overflowing reciprocal; maxfinite keeps X=1 but overflows the residual denominator and returns FERR Inf. Direct typed calls match INFO0. Per ABI: four failed processes, 80 assertions, 16 scalar-labeled cases/eight underlying-real fixtures, two active causes. | Preserve exact solution and finite safeguarded-error requirements. Numerical acceptance needs explicit strategies for scaled correction and residual/error evaluation; no provider patch, waiver or hidden substitution. [Exact PTRFS derivation and raw evidence](positive-tridiagonal-refinement-review.md#mathematical-fixtures-and-derivation). Independent safety/package/family work continues. |
| Pinned Reference PTSV scalar solve arithmetic | S/D/C/Z, N=NRHS=1, A=B=a, empty E, both RHS layouts; a is denorm-min, twice denorm-min or min-normal/8. Exact X=1 becomes Inf or (Inf,NaN), INFO0, matching direct typed PTSV. Per profile: four failed processes, 24 assertions, 12 scalar/value cases and 24 layout executions. This reaches the same PTTRS reciprocal-overflow cause above, not a new independent algorithm defect. Max-finite remains a passing control. | Retain the exact solution gate and expose raw numerical warnings. An explicitly approved scaled PT solve strategy or provider decision is needed for numerical acceptance; PPSVX-only authorization does not extend to this route. [Exact PTSV disposition](positive-tridiagonal-driver-review.md#required-numerical-disposition). Independent engineering and family work continues. |
| Pinned Reference PTSVX composed arithmetic | S/D/C/Z, FACT N/F, N=NRHS=1, A=B=a, empty E, both layouts. Three tiny values yield X Inf/(Inf,NaN),RCOND0,FERR/BERR NaN,INFO2; maxfinite yields RCOND/FERR Inf,BERR0,INFO0. Direct calls match. Four processes224assertions16scalar/valuecases32modecases64layout executions per profile. | Preserve all solution/condition/finite-error gates. Four inherited causes require a separately approved PT solve, scaled condition/reciprocal, and residual-weight strategy or provider decision; PPSVX-only authority does not extend here. [Typed review and exact cause table](positive-tridiagonal-expert-review.md). Local safety/package integration and unrelated families continue. |
| Pinned SGEDMDQ QR-factored modes | JOBZ=Q is documented but omitted from the source condition selecting nested GEDMD vector computation. Both actual ABIs return INFO0/K2 with incorrect Q*Z mode vectors for the ordinary m5,n4 diagonal trajectory;8 single-real variants fail per ABI, one source cause. D/C/Z and other selected modes pass the bounded direct probe. | Preserve the required eigenvector gate. A provider correction or separately named first-party composition would need its explicit numerical strategy authority; neither is implemented by silent JOBZ remapping. [Exact values and raw comparison](dmd-qr-review.md#required-numerical-failure-and-decision). This does not block checked GEDMD or other independent families. |
| Required extra-precision provider profile | All 130 absent definitions have pinned source. The maintained [dependency closure](xblas-dependency-closure.json) separates 54 rows that reach 28 external XBLAS helpers from 76 rows whose declared/called closure needs only already-present provider definitions. Both current actual-ABI providers selected USE_XBLAS=OFF. | Approve a separately named external extra-precision preparation and its exact dependency/ABI strategy before changing build selection or adding XBLAS. The existing providers remain unchanged. See the concrete candidate and evaluation scope below. This is a build-selection/dependency absence, not 130 missing upstream source files or 130 ASC binding defects. |
| Pinned Reference GECON guarded inverse estimation | S/D/C/Z, N=1, LU=ANORM=minnormal/1024, both norm flags/layouts. Exact RCOND=1 within the unchanged32epsilon scalar bound; ASC/direct pinned GECON return RCOND0,INFO0. Four failed processes16assertions per ABI, four failing scalar/value cases16norm/layout executions, one cause already recorded through GESVX. The2minnormal safeguard passes. | Retain the scalar condition gate. A separately authorized scale-invariant inverse-estimation strategy or provider decision is required; no hidden scaling, fallback, clamp or waiver. [Exact GECON continuation and source expression](lu-condition-review.md#master-continuation-normalized-gecon-modes). Concurrency/observer/installed work and independent families continue. |

The 78 historical root mathematical failing processes retain their family
reviews and raw records. Their complete cross-family cause consolidation is
still an engineering task; they are not 78 automatically distinct provider bugs
and are not a blanket request to waive requirements. Unavailable native
Windows/macOS or other required runners will be identified by their actual
missing resources when the existing admission queue reaches them. No platform
is marked verified from another platform's result.

One concrete oracle discrepancy has been isolated in that consolidation:
[GBRFS tiny diagnostic assertions](general-band-refinement-oracle-review.md)
omitted the documented small-denominator safeguards. The reviewed correction
retains the required finite-FERR failure and every existing input; the separate
provider inverse-before-weight overflow still needs a numerical strategy.
This test correction is ordinary authorized engineering, not an owner waiver
or a new dependency/provider algorithm decision.

## Extra-precision candidate: decision-ready scope

Read-only review obtained the authoritative [Netlib XBLAS distribution](https://www.netlib.org/xblas/)
version 1.0.248, archive SHA-256
`b5fe7c71c2da1ed9bcdc5784a12c5fa9fb417577513fe8a38de5de0007f7aaa1`
(2,087,424 bytes). The archive's `LICENSE` SHA-256 is
`d8fbddb866858a6a3a97bd00b7ef7758822fff72c635be67ad64b8c8ef71ca43`.
It contains Berkeley copyright and source/binary notice conditions; exact text
is retained externally for owner review. This is neither notice approval nor an
adopted dependency. No XBLAS code, archive or runtime is imported or distributed.

`tools/lapack/map_extra_precision_dependencies.py` regenerates/checks the
130-row map from the retained actual-provider symbol/build audit, current pinned
source, unchanged inventory and that exact archive. Each row binds its source
hash, build guards, direct call lines, declared function dependencies, transitive
missing sources and external helper set. Every existing-provider boundary has a
recorded definition in both actual ABIs. Each of the 28 candidate bridges has its
own source hash. The four independent generator tests include cycles, source-only
closures, fixed-form continuation and fail-closed checks under Python -O.
This is conservative source/declaration closure, not new link or numerical proof.

The proposed finite evaluation, **not executed or authorized as adoption**, is:

1. Preserve the current providers and create a separately named external
   USE_XBLAS preparation of the same LAPACK commit. For the 76 source-only rows,
   review a separately identified source-selection companion if their earlier
   admission is useful; do not silently alter the accepted archives.
2. Inspect/configure XBLAS with the admitted GNU C/Fortran toolchain, explicit
   compile options and a reviewed `make.inc`; its actual targets are `make lib`
   and `make tests`. Do not run a guessed preparation-script flag. Bind generated
   bridge objects, compiler settings and all source hashes before linking.
3. Resolve the ABI obstacle explicitly. Distributed `*-f2c.c` bridges and C
   numerical entry points use `int` dimensions/strides; `f2c-bridge.h` changes
   symbol spelling, not integer width. A Fortran integer typedef switch cannot
   admit ILP64. A proposed original checked 64-to-C-int bridge must have explicit
   bounds and preserve the required input domain, or a separately approved true
   64-bit dependency implementation is needed. Do not reinterpret 64-bit pointers
   as C-int pointers, silently truncate, or imply full-domain ILP64 support.
4. Review `blas_fpu.h` before any numerical evaluation: its x86 branch changes
   x87 precision and restores it. No floating-point environment change is
   authorized here. Establish an admissible configuration/equivalent algorithm
   and inspect generated code; do not hide that branch with an unreviewed define.
5. Run all 28 helper symbol/signature/control tests and upstream extra-precision
   tests; preserve their error ratios and failures. Add independent cancellation
   cases that distinguish extra precision from ordinary BLAS, extended-vector
   head/tail checks, range/exceptional cases and both actual-ABI guards. Then run
   the exact affected LAPACK expert tests and checked ASC wrapper gates. Source
   DGERFSX explicitly chooses `ILAPREC('E')`; ordinary double BLAS is insufficient.

Decision requested in this maintained packet: authorize the exact isolated
candidate evaluation plus an explicitly bounded ABI approach, or identify an
approved equivalent. Dependency adoption/import, any source adaptation, numerical
profile changes and eventual notice distribution retain their separate recorded
approval requirements. None is inferred from ordinary feature-write authority.
The raw review, archive and initial source-map attempt are retained under
`master-continuation-20260910-01/xblas-review-01` and `xblas-closure-01.json`.
