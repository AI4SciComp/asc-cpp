# Milestone and release roadmap

Status: Approved architecture; Milestone 8 reached local Publication
Checkpoint B

## Version policy

- Milestone completion and release publication are separate decisions.
- Pre-1.0 minor versions may break source/ABI with migration notes.
- Pre-1.0 patch versions preserve the documented public contract.
- ASCCpp 0.x package version files use `SameMinorVersion`.
- Serialized random state and file formats have their own schema compatibility
  and are not implied by library ABI.
- No unimplemented component or provider is exported.

## Roadmap

| Milestone | Scope | Earliest release line | Exit gate |
| --- | --- | --- | --- |
| 0 | Stage A, repository/build/package/test foundation, stale-doc disposition | no library release | green foundation tests; no production target |
| 1 | core CPU types/config/error/I/O/host memory/serial execution | 0.1.x | isolated `ASC::core` build/install/consume and safety tests |
| 2 | independent utilities, expression, random base waves | 0.2.x | three isolated components; random provenance gate resolved |
| 3 | dense CPU storage/evaluation/reference linalg | 0.3.x | dense contract/numerical/allocation/package evidence |
| 4 | sparse CPU coordinate/CSR/CSC/evaluation/reference SpMV | 0.4.x | sparse independent of dense; no densification evidence |
| 5 | random dense and sparse generation facets | 0.5.x | two independently consumable deterministic facets |
| 6 | core CUDA and dense CUDA | 0.6.x | real-hardware runtime and CPU parity |
| 7 | sparse CUDA and random CUDA facets | 0.7.x | real-hardware structural/numerical/bit evidence |
| 8 | packaging/API/performance/downstream hardening | 0.9.x candidate | full matrix and asc-xde trial |
| 1.0 readiness | stable reviewed six-module surface | 1.0.0 only after approval | compatibility/support/security/provenance/release review |

Milestone 7 reached its local Publication Checkpoint B candidate with the
complete fifteen-target surface and its recorded CUDA evidence. Milestone 8
hardens that unchanged surface through package/version/header/symbol checks,
the full locally available matrix, performance observations, documentation,
and an isolated asc-xde-shaped downstream trial, and reached its local
Publication Checkpoint B on 2026-07-27. This is not a release. A
hosted GPU runner, multi-device hardware evidence, publication approval, and
1.0 readiness remain outstanding.

## Provider sequencing

Serial reference behavior lands first. Detected OpenMP, Eigen, oneMKL,
BLAS/LAPACK, TBB, and SYCL do not enter a release merely because available on
the current host. Each needs an owner, exact target, operations, license,
capability rows, package isolation, and CI matrix.

CUDA is the first implemented GPU backend and is split by owner and evidence:
`core_cuda`, `dense_cuda`, `sparse_cuda`, `random_cuda`,
`random_dense_cuda`, and `random_sparse_cuda`. All default off together.
Provider facets can be omitted from a base release without changing the
six-module graph or making the provider-free package discover CUDA.

## Release procedure and rollback

Before publication:

1. review the complete diff and provenance manifest;
2. run the clean CPU/provider/package matrix on the exact commit;
3. obtain explicit approval to commit/push/open a PR;
4. inspect hosted results and all skips;
5. obtain separate merge and tag/release authorization.

Never move or recreate a published tag. Before release, rollback uses a
reviewed revert. After release, publish a new patch after full validation.
Branch deletion is a separate post-release operation after proving all unique
work is merged or archived.

## Explicitly blocked or deferred

- direct MdeCpp source/test copying;
- historical Sobol source/tables and Lebedev data;
- general broadcasting and runtime-rank storage;
- hidden compatibility with deleted `array`/`linalg` APIs;
- broad factorization/solver surfaces before numerical/provider contracts;
- provider claims without real runtime evidence.
