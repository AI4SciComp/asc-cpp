# BLAS completion audit

Status: unreleased `0.9.0` Issue 11 Feature Gate B candidate

Date: 2026-08-01

Issue 11 independently audits the Dense and Sparse BLAS work completed by
Issues 6–10. It adds no operation, scalar type, backend, module, dependency
edge, or compatibility alias.

## Audited inventory

The machine-readable [coverage manifest][manifest] contains the frozen 150-row
Dense inventory and 79-row Sparse inventory. Its 229 rows resolve to 182
verified rows and 47 standards-based not-applicable rows, with no planned,
implemented, or blocked row. The 49-family dense-to-sparse crosswalk resolves
to 11 verified analogues and 38 standards-based not-applicable mappings.

The generator freezes separate identities for:

- official routine, module, level, operation, and scalar inventory;
- API, public header, CPU/CUDA implementation, backend state, conformance,
  CPU/GPU, invalid-input, and aggregate status evidence; and
- dense-to-sparse family, analogue, and completion status.

Every linked file must exist in its approved repository location. Public API
names must occur in the linked public header, and headers, implementation
files, and tests must occur in their owning CMake registration. The generated
[BLAS coverage report](blas-coverage.md) must exactly match the YAML source.

## One backend semantic contract

Dense and Sparse use the same explicit boundary on both declared backends.
The portable CPU operation is synchronous. The CUDA operation accepts only
CUDA-resident operands and returns explicit completion. Neither path silently
allocates, transfers, packs, converts, synchronizes, selects another provider,
or falls back to another backend. A CUDA row is verified only by a real-device
test; forced-no-device skips are negative behavior evidence, not verification.

The executable completion audit also rejects retired `linalg` text in product
headers, sources, benchmarks, or build logic. Historical migration prose and
negative compatibility checks remain intentionally available.

## Reproducing the audit

Configure and run the architecture checks with the real released ASCCMake
package:

```sh
cmake -S . -B build/issue-11-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASCCMake_DIR=/absolute/path/to/ASCCMake-0.1.0
cmake --build build/issue-11-debug --parallel
ctest --test-dir build/issue-11-debug -L architecture --output-on-failure
```

For CUDA evidence, also enable `ASC_CPP_ENABLE_CUDA`, provide a CUDA compiler,
and set the caller-selected `CMAKE_CUDA_ARCHITECTURES`. The full registered
suite—not only the architecture label—contains the conformance, invalid-input,
edge-case, packaging, relocation, downstream, and correctness-guarded
benchmark checks inventoried by the completion audit.

Public Dense and Sparse usage examples remain in the [Dense](modules/dense.md)
and [Sparse](modules/sparse.md) module guides. Issue 11 changes none of their
spelling or semantics.

Exact local suite counts, linkage modes, sanitizer selection, CUDA device, and
package/relocation timings are recorded in the [support matrix][support].

[manifest]: development/asc-cpp-architecture/blas-coverage.yaml
[support]: support-matrix.md
