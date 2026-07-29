# Milestone 4 Sparse CPU ownership ledger

Status: Frozen before delegated implementation

Date: 2026-07-28

Branch: `feature/asc-cpp-m4-sparse-cpu`

All scopes are disjoint. Every role preserves cumulative Milestones 0--3 and
the user's prior deletions. No role may commit, push, merge, tag, release,
delete a branch, add a dependency, or implement a later milestone.

| Role | Exclusive writable paths | Read-only authority | Lead-owned integration |
| --- | --- | --- | --- |
| Lead architect/integrator (`/root`) | root `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml`, `cmake/**`, all `src/**/CMakeLists.txt` and `tests/**/CMakeLists.txt`, `tests/architecture/**`, shared compile/package/consumer scripts, `README.md`, `CHANGELOG.md`, `docs/{README,api}.md`, architecture manifests/matrix, `docs/development/asc-cpp-m4-sparse-cpu/{milestone-contract,ownership,dependency-audit,provenance-record,publication-checkpoint-b}.md`, final integration corrections | entire repository, supplied runbook, approved architecture/ADRs, M3 checkpoint, released ASCCMake | root/package/component/CI/manifests, cross-directory registration, reconciliation, clean validation, checkpoint |
| Production implementation engineer (`/root/m4_production`) | `include/asc/sparse.h`, `include/asc/sparse/**`, `src/sparse/*.cc`, `include/asc/expression/{writable,expression}.h`, `include/asc/expression.h`, `include/asc/dense/view.h`, `docs/development/asc-cpp-m4-sparse-cpu/production-self-review.md` | frozen contract, approved ADRs, current Core/Expression/Dense APIs, released ASCCMake; no MdeCpp or later implementation | no CMake, tests, packages, manifests, CI, general docs, or providers |
| Independent verification engineer (`/root/m4_verification`) | `tests/sparse/**` excluding CMake files, `tests/compile/m4_*.cc`, `tests/consumer/sparse/**` excluding CMake files, `benchmarks/sparse/**` excluding CMake files, `docs/development/asc-cpp-m4-sparse-cpu/{verification-design,verification-review}.md` | contract and ADRs; production only after contract-first oracles are frozen; no MdeCpp/deleted/later tests | lead owns every CMake registration and shared fixture |
| Documentation/API reviewer (`/root/m4_documentation`) | `docs/modules/sparse.md`, `docs/modules/expression.md`, `docs/development/asc-cpp-m4-sparse-cpu/documentation-api-review.md` | contract, ADRs, public headers, package evidence | reports out-of-scope defects; lead reassigns corrections |
| Portability/GPU/performance reviewer (`/root/m4_portability`, started after another role completes) | `docs/development/asc-cpp-m4-sparse-cpu/portability-review.md` | complete integrated M4 diff and evidence | review-only; lead implements or explicitly reassigns accepted corrections |

## Coordination rules

- Production does not write tests; verification does not write production.
- Verification freezes independent expected structures/numerics before reading
  production.
- No role reads, copies, translates, or uses MdeCpp, deleted asc-cpp, or later
  milestone production/test material.
- Documentation reports API defects rather than documenting around them.
- Portability changes only its report and classifies GPU evidence exactly as
  **skipped**.
- Only the lead changes shared target/export/component/package logic,
  manifests, or CI.
- Work outside scope is reported with the exact file/symbol and rationale;
  only the lead may reassign it.
