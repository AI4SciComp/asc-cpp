# Milestone 0 ownership ledger

Status: Frozen before delegated implementation

| Role | Writable paths | Read-only paths | Lead-integrated shared files |
| --- | --- | --- | --- |
| Lead architect/integrator | `CMakeLists.txt`, `CMakePresets.json`, `cmake/**`, `.github/workflows/ci.yml`, `docs/development/asc-cpp-m0-foundation/{milestone-contract,ownership}.md`, final integration fixes | entire workspace | all shared root build/package/CI files |
| Foundation implementation engineer | `.clang-format`, `.clang-tidy`, `docs/development/asc-cpp-m0-foundation/foundation-implementation-review.md` | runbook, Stage A, released asc-cmake | none |
| Independent verification engineer | `tests/**`, `docs/development/asc-cpp-m0-foundation/verification-review.md` | runbook, frozen contract, Stage A, implemented build/package files | lead reviews/integrates test registration interactions |
| Documentation and API reviewer | `README.md`, `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `docs/README.md`, the 24 retained historical documents listed in the approved implementation plan, `docs/development/asc-cpp-m0-foundation/documentation-api-review.md` | runbook, frozen contract, Stage A | lead resolves shared claim conflicts |
| Portability/GPU/performance reviewer | `docs/development/asc-cpp-m0-foundation/portability-review.md` | complete integrated diff and validation evidence | lead implements any accepted correction |

## Coordination rules

- Write scopes are disjoint.
- Specialists do not commit, push, merge, tag, release, switch branches, or
  delete branches.
- The lead alone edits root CMake/package/CI files and accepts the final diff.
- A specialist finding outside its scope is reported to the lead, not fixed
  across ownership boundaries.
- GPU evidence for this no-provider milestone can only be environment
  discovery or `skipped`; no runtime/parity claim is permitted.
