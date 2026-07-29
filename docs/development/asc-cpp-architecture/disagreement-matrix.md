# Independent-design disagreement matrix

Status: reconciled lead decision, awaiting Checkpoint A approval

The three reports were completed independently before the lead read any of
them.

## Shared conclusions

All designers independently concluded that:

- the current tree is an intentional, non-buildable clean restart;
- the retained five-component documents are historical, not the new contract;
- the exact six-module graph and random facets are viable and mandatory;
- `array` and `linalg` must not return as modules;
- expression must be storage-neutral and own no evaluator;
- dense and sparse must own separate storage, evaluation, and linear algebra;
- random base must be core-only with explicit state;
- memory, execution, transfer, workspace, and synchronization must be explicit;
- optional SDKs must remain outside common headers and base package closure;
- MdeCpp is behavioral/provenance evidence, not a source tree to copy;
- package and dependency boundaries require mechanical negative tests.

## Resolved differences

| Topic | Internal analyst | MdeCpp/provenance analyst | Independent architect | Lead resolution |
| --- | --- | --- | --- | --- |
| `ASC::cpp` closure | Facets only if ADR chooses | Optional umbrella, never a prerequisite | Six bases plus both random facets | `ASC::cpp` contains the complete provider-free surface: six bases and both random facets. It contains no optional provider. |
| General broadcasting in v1 | Required ADR; historical rules unsafe | Preserve concept, redesign | Exact shape plus rank-zero scalars first; defer implicit broadcasting | v1 expression supports exact shape and rank-zero scalar expansion only. General trailing-axis broadcasting needs a later ADR and sparse-effect proof. |
| Default dense layout | Historical left/column-major was useful | Preserve layout test ideas, redesign | Did not require one default | `LayoutLeft`/column-major is the named default; right and explicit non-negative stride mappings are equal supported semantics. Algorithms never branch on a build-global default. |
| Runtime rank | Identified as open | Historical ranks are evidence only | Compile-time rank with mixed dynamic extents; defer runtime rank | v1 uses compile-time rank and mixed static/dynamic extents. General runtime rank is deferred. |
| Package generation | Do not invent asc-cmake component helpers | Candidate target names; isolate consumers | Standard CMake or a new asc-cmake release for conditional exports | Use released asc-cmake policy helpers plus standard CMake component exports. Do not imitate a helper or modify asc-cmake in this milestone. |
| Optional optimized CPU provider | Historical Eigen/MKL useful but broad | Provider concepts may be re-specified | No provider selected without dependency/CI decision | Serial reference is the only approved initial provider. OpenMP, oneMKL/BLAS/LAPACK, Eigen interoperability, and SYCL remain detected candidates, not capabilities. |
| Error exceptions | Fresh decision needed; historical compatibility unsuitable | Preserve taxonomy, redesign | No public production exceptions | Adopt `Status`/`Result<T>` and fatal internal contracts; no public production exception API. |
| Sparse duplicate/zero defaults | Must be explicit | Public contract required | No silent defaults | Finalization requires explicit duplicate and explicit-zero policies; no API default. |
| CUDA facet relationships | Keep owner-specific facets | CUDA isolated by owner | Random CUDA storage facets need storage descriptors but not algebra-provider facets | Adopt owner-specific `core_cuda`, `dense_cuda`, `sparse_cuda`, `random_cuda`, `random_dense_cuda`, and `random_sparse_cuda`; no provider is part of `ASC::cpp`. |
| Philox and historical vectors | Strong historical concept | MdeCpp source/tests cannot be copied | Recommend Philox4x32-10 | Approve the algorithm contract, but implementation/vectors must be clean-room from the paper or a separately approved authoritative upstream, not copied from MdeCpp or deleted asc-cpp. |
| Configuration file syntax | Split core model/utilities parser | Specify rollback/duplicates | Programmatic/CLI first; choose narrow format or dependency later | Freeze the boundary and precedence now. Milestone 2 may ship CLI first; local-file syntax remains separately gated. |

## Requirements that overrode proposals

- A shared `array`, `linalg`, or “backend” module is rejected even if it could
  reduce code duplication.
- Dense and sparse cannot use utilities for common storage or parsing helpers.
- Random generation adapters remain random-owned facets, not dense/sparse
  subcomponents.
- A successful CUDA configure/compile or visible GPU is not runtime provider
  evidence.
- No compatibility facade for the deleted five-component API is planned
  without a separate owner request.

## Remaining approval choices

The lead has resolved the architectural choices in the ADRs. Owner approval is
still required before they become implementation authority. Concrete optional
CPU providers, a local configuration-file syntax/dependency, Sobol, runtime
rank, broad broadcasting, and provider-native interoperability remain deferred
rather than silently selected.
