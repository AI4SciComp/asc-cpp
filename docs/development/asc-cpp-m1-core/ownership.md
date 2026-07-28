# Milestone 1 core ownership ledger

Status: Frozen before delegated implementation

| Role | Writable paths | Read-only paths | Shared files integrated by lead |
| --- | --- | --- | --- |
| Lead architect/integrator | root `CMakeLists.txt`, `CMakePresets.json`, `cmake/**`, all module/test CMake registration files when integration requires it, `.github/workflows/ci.yml`, `docs/architecture/dependency-policy.md`, `docs/development/asc-cpp-m1-core/{milestone-contract,ownership,dependency-audit,publication-checkpoint-b}.md`, manifest/backend-matrix status updates, final integration fixes | entire workspace | all shared build, package, CI, manifest, and integration files |
| Core production engineer | `include/asc/core.h`, `include/asc/core/**`, `src/core/**` excluding CMake files, `docs/development/asc-cpp-m1-core/production-self-review.md` | runbook, frozen contract, ADRs, released asc-cmake, MdeCpp only as prohibited-source provenance context | none |
| Independent verification engineer | `tests/core/**`, `tests/compile/**`, `tests/consumer/core/**`, `docs/development/asc-cpp-m1-core/verification-review.md` | runbook, frozen contract, ADRs, M0 tests, implemented public API only after an independent contract-first test design | lead owns root and cross-directory test registration |
| Documentation and API reviewer | `README.md`, `CHANGELOG.md`, `docs/README.md`, `docs/modules/core.md`, `docs/development/asc-cpp-m1-core/documentation-api-review.md` | runbook, frozen contract, ADRs, public headers, package/consumer evidence | lead resolves API/build claims and shared manifest conflicts |
| Portability/GPU/performance reviewer | `docs/development/asc-cpp-m1-core/portability-review.md` | complete integrated diff and validation evidence | lead implements every accepted correction |

## Coordination rules

- Write scopes are disjoint. Specialists do not edit root/package/CI files.
- Specialists do not commit, push, merge, tag, release, switch branches, or
  delete branches.
- The production engineer does not write tests. Verification derives its first
  test design from the contract before reading production implementation.
- Documentation reports API defects instead of documenting around them.
- The lead alone changes shared target/export/component logic and accepts the
  final diff.
- No role restores an unrelated deleted file or modifies the MdeCpp checkout.
- GPU evidence for this CPU-only milestone must be `skipped`; toolkit/hardware
  inventory is not provider evidence.
