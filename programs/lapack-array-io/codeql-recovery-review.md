# Recovered CodeQL alert gate

Status: **open verification gate**, recovered read-only on 2026-09-12.
The original recovery read found PR 47 at feature commit
`663c7a4c4b1388c3a2c78093e61f2e6e2f94f9f4`, with analyzed merge
`a09ef2393dc6ca5eda02ccde8e6f8ddd184559bc`. The continuation below records
the later pushed checkpoint separately.

The [CodeQL analysis workflow](https://github.com/AI4SciComp/asc-cpp/actions/runs/34647493615)
succeeded, but the separate
[CodeQL alert check](https://github.com/AI4SciComp/asc-cpp/runs/103426990987)
failed. The check reports 248 new alerts: four critical security findings,
four high findings and 240 notes. The branch alert API contains 380 open
findings, including those eight security findings and three other high findings.
A successful analysis job must not be reported as a passing alert gate.
The recovered workflow built the default provider-disabled configuration; it
did not enable `ASC_CPP_ENABLE_LAPACK`. The additive `lapack_cpp` job now
configures the real LP64 and ILP64 providers and compiles the complete
first-party `asc_dense_lapack` production target under CodeQL extraction. The
provider is built and attested before extraction. Existing default analysis
remains unchanged; the new analyses use distinct categories as described by
[GitHub's workflow documentation](https://docs.github.com/en/code-security/reference/code-scanning/workflow-configuration-options#analysis-category).
Actual hosted execution and findings remain required; adding a workflow is
not a passing analysis result.

## Source review

All six affected source files remain byte-identical to feature commit 663c7a4.
The raw alert data, source hashes and review are external under
`master-continuation-20260910-01/continuation-20260912-01/codeql-alert-recovery-01`.

| Alerts | Actual source behavior | Current disposition |
| --- | --- | --- |
| 247–250, critical | The Linux-only huge-preview test maps 8,000,000,000 bytes, computes the count and byte extent with `size_t`, begins the array lifetimes, and touches indices 0, 1, count−2 and count−1. | The reported negative 32-bit offsets do not match the audited 64-bit source arithmetic. Preserve the frozen test and evidence; model/platform disposition remains open. |
| 366, 251, 225, high | Deliberate byte offsets exercise rejected alignment and aliasing. The arithmetic uses `std::byte*`; misaligned typed pointers are not dereferenced by the rejection tests. | Preserve the rejection and zero-write assertions. Alert disposition remains open. |
| 18, 17, high | Sparse test adapters use byte pointers and `memcpy` with explicit element-to-byte conversion. | The byte scaling is intentional. Alert disposition remains open. |
| 365, high | The public local-file API opens the caller's explicit filesystem path. It is not a restricted-directory API. | Imposing path containment would change the frozen API. Calling applications own path authorization; the alert remains open. |
| 15, high | The private GEMV benchmark oracle multiplies doubles before accumulating into long double. Its only caller sets extent 128, matrix coefficients at most 7/16 and vector coefficients at most 11/8. | Products are at most 77/128 for the actual fixture. Preserve the source and alert pending disposition; no performance claim follows. |

No query was suppressed, alert dismissed, production or test source changed,
or frozen native/array-I/O/robust PPSVX acceptance rerun. This review records
source evidence and the remaining gate; it does not claim CodeQL approval.

## Other recovered hosted results

The old GERFS checkpoint's CI workflow completed successfully. Its four actual
LAPACK profiles retain 37 LP64 or 33 ILP64 failures: 227/264 and 231/264 pass,
respectively, for both static and shared linkage, with zero skips. Each provider
suite passes 111/111. All four GERFS required mathematical tests remain failed.
The downloaded original artifacts are preserved in `gerfs-hosted-recovery-01`;
no hosted workflow was rerun to obtain them.

Continue independent LAPACK work while retaining this gate. Full programme
status remains **FULL_PROGRAM_INCOMPLETE**.

## Optional backend analysis executed

Feature checkpoint `736fa6c243c4310ed35675f0cb47a701321e7344` was pushed to
the existing branch. Its [CodeQL workflow](https://github.com/AI4SciComp/asc-cpp/actions/runs/34673119204)
completed successfully, including the default job and both actual LP64/ILP64
optional-backend analysis jobs. Raw job outcomes are preserved in
`continuation-20260912-01/continued-hosted-read-03`.

The branch alert read now contains 2,626 open findings, including twelve
security findings. New high alert 2612 identifies the offset used to begin
the second native integer array in `internal_tridiagonal.h`. That expression
intentionally advances a `std::byte*` by `order*sizeof(lapack_int)` inside
checked caller workspace. The source remains unchanged. Exact alert payload,
source hash and bounded review are in `continued-codeql-alerts-01/review.json`.
The alert gate and complete note review remain open; no alert was dismissed
and no query was suppressed. These analyses cover checkpoint 736fa6c, not
the subsequent local indefinite INFO correction.
