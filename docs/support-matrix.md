# ASCCpp 0.9.0 support matrix

ASCCpp 0.9.0 is a provider-free C++20 reference release. This page
distinguishes the release contract from evidence. A row is supported
only after its job passes on the exact release commit; configuration files are
not evidence by themselves.

## Release contract

| Area | 0.9.0 status | Boundary |
| --- | --- | --- |
| C++ language | supported | C++20, extensions disabled |
| Core | supported | provider-free CPU status/configuration/I/O/memory/execution |
| Utilities | supported | command-line and monotonic timer contracts |
| Expression | supported | storage-neutral read/write/alias/access customization |
| Dense | supported reference | owners/views/evaluation and BLAS Levels 1--3 |
| Sparse | supported reference | coordinate/CSR/CSC/evaluation and Sparse BLAS |
| Random | supported | deterministic engines/distributions/QMC and storage adapters |
| CPU BLAS performance | not claimed | serial correctness reference, not optimized provider |
| CUDA components | experimental | source/package retained; no support without full GPU gate |
| ABI | `0.9.x` patch line | SONAME `0.9`; no cross-minor `0.x` promise |

## Platforms requiring exact-commit evidence

| Platform | Required compiler/configurations |
| --- | --- |
| Linux x86-64 | GCC minimum Debug/Release; GCC current Release static/shared; Clang current Debug/Release static/shared |
| Windows x64 | VS 2022 Debug/Release static/shared |
| macOS arm64 | AppleClang Debug/Release static/shared |

All supported rows also require header self-containment/no-exception checks,
complete CTest, package install/relocation/isolation, strict documentation,
installed examples, and architecture/BLAS/Random/provenance drift checks.

## Sanitizers and analysis

ASan+UBSan, standalone LSan, bounded TSan, format, full provider-free tidy, and
CodeQL (or an approved equivalent) are release gates. Tool absence at Gate B is
reported as missing evidence, never as a pass.

## CUDA experimental boundary

Upgrading CUDA to supported requires a trusted NVIDIA runner to pass static and
shared builds; all six CUDA component/package/relocation consumers; numerical
CPU/GPU and Random bit/address parity; BLAS/Sparse BLAS coverage; stream/event
lifetime and concurrency; no-device/provider-failure paths; Compute Sanitizer;
and exact toolkit, driver, device, and architecture reporting.

Historical local CUDA observations remain useful development evidence but do
not satisfy this release gate.

## Scientific coverage contracts

- BLAS: 182 verified rows and 47 standards-based not-applicable rows in
  [`contracts/blas-coverage.yaml`](contracts/blas-coverage.yaml).
- Random: 22 accepted and 11 explicitly rejected decisions in
  [`contracts/random-crosswalk.yaml`](contracts/random-crosswalk.yaml).
- Joe--Kuo data/license checks and MdeCpp disposition are required provenance
  gates.

See [performance](performance.md), [API compatibility](api-compatibility.md),
and the Gate B/hosted validation records under [`release/`](../release/).
