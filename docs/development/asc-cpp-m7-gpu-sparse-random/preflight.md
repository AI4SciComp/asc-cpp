# Milestone 7 Preflight

Date: 2026-07-27

Milestone: **Milestone 7 — sparse CUDA and random CUDA facets**

Owner corrections: **No corrections**

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

Unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

The working tree intentionally contains the cumulative uncommitted Milestones
0--6 clean restart plus the owner's earlier deletion of the retired
implementation. All predecessor feature branches and M7 start from the same
unchanged baseline commit. No existing change was reset, restored, stashed,
committed, or discarded.

The approved architecture names exactly four M7 facets and their direct
dependency closures. The frozen contract narrows implementation to bounded
sparse CUDA evaluation and CSR SpMV plus clean-room Philox/Uniform01 raw,
dense, and exact-count sparse CUDA generation. It adds no dependency beyond
the already approved CUDA toolkit Runtime and cuSPARSE component.

Publication, history changes, remote writes, release work, Milestone 8, and
unlisted provider capabilities remain prohibited.
