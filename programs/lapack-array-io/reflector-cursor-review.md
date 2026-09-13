# Reference reflector terminal INTEGER cursor correction

This is an implementation self-review, not owner or license approval. The
earlier v5 full-suite pass does not cover the newly discovered integer limit.
Pinned source and actual LP64/true ILP64 providers remain unchanged.

## Source-conditioned bound

The exact 3.12.1 GEQR2/ORG2R/ORM2R routes use LARF1F, not an assumed historical
LARF implementation. Real left LARF1F calls AXPY with a matrix-row increment
LDC; its LASTV=1 route calls SCAL. Complex LASTV=1 can have a nonzero phase
reflector and calls complex SCAL. All four LARFB left routes COPY a row using
increment LDC, including the compact-WY QR/LQ application paths.

Reference COPY/AXPY advances the INTEGER cursor after the final accessed
entry; SCAL computes N*INCX and its final loop cursor. Thus checking only
(length-1)*LDC+1 is insufficient: length*LDC+1 must fit the selected INTEGER.
This is a foreign-expression bound, not a blanket restriction on ASC strides.

QR factorization's largest left trailing width is N-1; blocked updates are
smaller. Q generation similarly uses N-1. Left Q application uses all target
columns; right application has unit-stride matrix columns. Real order-one
LARFG has zero tau and its factorization is exempt, whereas complex order-one
LARFG can produce a nontrivial phase. Raw generation/application reflectors
must not inherit a real factorization-only zero-tau assumption. Empty and
zero-reflector paths preserve their existing semantics.

For GELS/GELST/GETSLS, tall A uses the effective foreign LDA and N-1 bound;
the prior source-specific wide-row reflector guard remains. B is explicitly
packed with max(M,N) leading dimension, so left application additionally bounds
NRHS*max(M,N)+1. Real scalar GELS has zero tau; compact-WY LARFB still copies
regardless of tau, so GELST/GETSLS retain the cursor guard. Existing nested
LWORK, floating metadata, loop and layout-size bounds are not replaced.

## Exact amended artifacts

| Artifact | SHA256 |
| --- | --- |
| `src/dense/lapack/internal_qr_counts.h` | `ea5f8c75a0cdd66a8c5e24ce7be50ca96f83e46e5bebf95f7bd94f295f96e60e` |
| `src/dense/lapack/reference_qr.cc` | `b6e5f8b9d0756e1f503350b32548535e2eef387a5102abee46b214c5365fe008` |
| `src/dense/lapack/internal_least_squares_counts.h` | `a8553e17c867dd751df9769d9fa25978166fd935b414a2c815dedd9b681a89da` |
| `tests/dense_lapack/qr_counts_test.cc` | `f6c337ec77471f6275b83114e9435bb95f5377b178c70a832c52adc5de04ee61` |
| `tests/dense_lapack/least_squares_counts_test.cc` | `a4f21e9d645fbbbe3bd23067405ceee4b0b37bfad4c8e5d498d7ae0c3a64cb0a` |

The amended least-squares helper/test are distinct from the frozen owner's
original artifacts. No frozen old source or log was overwritten.

## Actual diagnostic evidence and remaining verification

External root: `../asc-cpp-evidence/lapack-array-io`. Run command:

```sh
bash reflector-cursor-diagnostic.sh 01
```

Both count test executables pass, including allocation controls and explicit
INT32_MAX/INT64_MAX arithmetic. No huge nonempty view is fabricated. Both TUs
pass strict Clang18. `reflector-cursor-probe.cc` is compiled unchanged once
against the frozen old least-squares helper and once against the correction.
The old executable exits1 with twelve unsafe query admissions; corrected exits0
with all rejected and adjacent safe cases still accepted. Both build exits0.
Raw logs are `logs/reflector-*-01.log`; the old failure is not a passing math
test or a CTest count. These are pure arithmetic checks, not actual foreign
calls with huge buffers or a simulated provider ABI.

Public-only expert/least-squares diagnostic attempt03 also passes all four
family/ABI runtime processes and both consumer strict checks after the helper
amendment. It links existing v4 Core/Dense archives and newly compiled expert
adapters; this is not relocated installed-package or full integration evidence.
Fresh v6 full, affected-family sanitizer and installed verification remain
required. Normalized complete routine/mode evidence remains separately open.
