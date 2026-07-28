# Milestone 3 Dense CPU ownership ledger

Status: Frozen before delegated implementation

Date: 2026-07-28

Branch: `feature/asc-cpp-m3-dense-cpu`

All scopes are disjoint. Agents preserve cumulative Milestones 0--2 and the
user's prior deletions. No agent may commit, push, merge, tag, release, delete
a branch, add a dependency, or implement a later milestone.

| Role | Exclusive writable paths | Read-only authority | Lead-owned integration |
| --- | --- | --- | --- |
| Lead architect/integrator (`/root`) | root `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml`, `cmake/**`, all `src/**/CMakeLists.txt` and `tests/**/CMakeLists.txt`, `README.md`, `CHANGELOG.md`, `docs/{README,api}.md`, architecture manifests/matrix, `docs/development/asc-cpp-m3-dense-cpu/{milestone-contract,ownership,dependency-audit,provenance-record,publication-checkpoint-b}.md`, final integration corrections | entire repository, runbook, approved architecture/ADRs, M2 checkpoint, released ASCCMake | all root, package, component export, CI, manifest, cross-directory registration, reconciliation, and final evidence files |
| Production implementation engineer (reused `/root/m2_production`) | `include/asc/dense.h`, `include/asc/dense/**`, `src/dense/*.cc`, `docs/development/asc-cpp-m3-dense-cpu/production-self-review.md` | frozen contract, approved ADRs, current Core/Expression APIs, released ASCCMake; no MdeCpp or later implementation | none |
| Independent verification engineer (reused `/root/m0_verification`) | `tests/dense/**` excluding CMake files, `tests/compile/m3_*.cc`, `tests/consumer/dense/**` excluding CMake files, `benchmarks/dense/**` excluding CMake files, `docs/development/asc-cpp-m3-dense-cpu/{verification-design,verification-review}.md` | frozen contract and accepted ADRs; production headers only after recording contract-first test design; no MdeCpp or later test material | lead owns every CMake registration and shared test/package fixture |
| Documentation and API reviewer (`/root/m3_documentation`) | `docs/modules/dense.md`, `docs/development/asc-cpp-m3-dense-cpu/documentation-api-review.md` | frozen contract, ADRs, public headers, package/consumer evidence | proposed edits outside scope are findings for the lead |
| Portability/GPU/performance reviewer (`/root/m3_portability`, started after an earlier role completes) | `docs/development/asc-cpp-m3-dense-cpu/portability-review.md` | complete integrated M3 diff and validation evidence | review-only; lead implements every accepted correction |

## Coordination rules

- Production does not write tests; verification does not write production.
- Verification freezes its independent test/oracle design before inspecting
  production headers or sources.
- No agent reads, copies, translates, or uses MdeCpp, deleted asc-cpp, or the
  completed hardening branch's M3 production/test implementation.
- Documentation reports API defects rather than documenting around them.
- Portability changes only its report and classifies GPU evidence exactly as
  **skipped**.
- Only the lead changes shared target/export/component logic, package tests,
  manifests, or CI.
- Work outside a scope is reported with the exact file/symbol and rationale;
  only the lead may reassign it.

