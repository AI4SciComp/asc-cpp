# Milestone 2 independent-foundations ownership ledger

Status: Frozen before delegated implementation

| Role | Writable paths | Read-only paths | Shared files integrated by lead |
| --- | --- | --- | --- |
| Lead architect/integrator | root `CMakeLists.txt`, `CMakePresets.json`, `cmake/**`, all module/test CMake registration files, `.github/workflows/ci.yml`, core origin additions required by the frozen parser contract, `docs/architecture/dependency-policy.md`, architecture manifests/matrix/roadmap, `docs/development/asc-cpp-m2-independent-foundations/{milestone-contract,ownership,dependency-audit,provenance-record}.md`, final integration fixes | entire workspace | all shared build, package, CI, core-origin, manifest, and integration files |
| Production implementation engineer | `include/asc/{utilities.h,expression.h,random.h}`, `include/asc/utilities/**`, `include/asc/expression/**`, `include/asc/random/**`, `src/{utilities,random}/**` excluding all CMake files, `docs/development/asc-cpp-m2-independent-foundations/production-self-review.md` | runbook, frozen contract, approved ADRs, released asc-cmake; primary Philox paper only for random derivation | none |
| Independent verification engineer | `tests/utilities/**`, `tests/expression/**`, `tests/random/**`, `tests/compile/m2_*`, `tests/consumer/{utilities,expression,random}/**`, `docs/development/asc-cpp-m2-independent-foundations/{verification-design,verification-review}.md` | runbook, frozen contract, approved ADRs; production headers only after recording the independent contract-first design; primary Philox paper only for independent vectors | lead owns root and cross-directory test registration |
| Documentation and API reviewer | `README.md`, `CHANGELOG.md`, `docs/README.md`, `docs/api.md`, `docs/modules/{utilities,expression,random}.md`, `docs/development/asc-cpp-m2-independent-foundations/documentation-api-review.md` | runbook, frozen contract, ADRs, public headers, package/consumer evidence | lead resolves API/build claims and shared manifest conflicts |
| Portability/GPU/performance reviewer | `docs/development/asc-cpp-m2-independent-foundations/portability-review.md` | complete integrated diff and validation evidence | lead implements every accepted correction |

## Coordination rules

- Write scopes are disjoint. Specialists do not edit root, package, CI,
  architecture manifest, core, or CMake registration files.
- Specialists do not commit, push, merge, tag, release, switch branches, or
  delete branches.
- The production engineer does not write tests. Verification records its first
  contract-first test design before reading production implementation.
- Production and verification derive Philox independently from the frozen
  primary paper and contract. Neither may inspect MdeCpp, deleted asc-cpp
  random code/tests, Random123 implementation code/tests, or an upstream vector
  corpus.
- Documentation reports API defects instead of documenting around them.
- The portability reviewer changes only its report; accepted source/test/build
  corrections are returned to the owning engineer or lead.
- The lead alone changes shared target/export/component logic and accepts the
  final integrated diff.
- No role restores an unrelated deleted file or modifies asc-cmake or MdeCpp.
- GPU evidence must be `skipped`.
