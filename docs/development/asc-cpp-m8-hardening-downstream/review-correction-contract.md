# Milestone 8 Post-Checkpoint Review Correction Contract

Status: Frozen

Date: 2026-07-28

Owner authorization: resolve every actionable risk reported by the independent
read-only review of
`feature/asc-cpp-m8-hardening-downstream-r2`.

## Boundary

This is a bounded correction wave within **Milestone 8 — packaging/API/
performance/downstream hardening**. It does not authorize Milestone 9, 1.0
readiness, publication, a new target/component/provider/dependency, or a new
numerical operation.

The approved six-module graph, fifteen-target CUDA-enabled surface, nine-target
provider-free surface, direct dependency manifest, C++20 policy, and M7
`MutableMemoryView + word_count` Random CUDA API remain frozen.

Production edits are permitted only where required to correct a review-proven
defect. No public API is intentionally added, removed, or renamed.

## Required corrections

1. Drain the affected CUDA stream before returning any failure that occurs
   after work may have been enqueued, including Core copies, Dense custom
   kernels, cuBLAS calls, and device-backed Dense clone failure paths. Preserve
   the original provider failure unless cleanup is the only failure.
2. Include every sparse structure span read by Coordinate and Compressed
   terminals in conservative alias analysis for sparse evaluation and SpMV.
3. Reject unsafe test workspace deletion targets before any recursive removal,
   including roots, source/build/workspace roots, parents, traversal, and
   symlink escapes.
4. Classify absence of a CUDA device as CTest `SKIP_RETURN_CODE 77` throughout
   Core CUDA and Dense CUDA runtime/performance tests.
5. Keep required provider-free components usable when an optional CUDA
   component cannot discover CUDAToolkit, in build-tree, installed, relocated,
   static, shared, and repeated-lookup package modes.
6. Make the synthetic asc-xde-shaped trial independent of mutable sibling
   checkout state. A real asc-xde repository audit is explicit opt-in and
   requires an explicit expected commit.
7. Enforce checked ELF baselines only on the exact recorded environment;
   otherwise produce a truthful non-enforcing observation or skip.
8. Update `SECURITY.md` to the unreleased 0.9.0 Milestone 8 surface.
9. Add rectangular CPU GEMM coverage for every transpose pair and padded
   mappings.
10. Define and test integral `ReduceSum` overflow as failure before publishing
    a wrapped result.
11. Add irregular, multiway, empty, reordered, and double Random partition
    coverage wherever partition invariance is claimed.
12. Prevent Timer total/count/average arithmetic wrap and align the
    documentation and capability evidence with the implemented checked
    contract.
13. Force sparse Random priority collisions and verify ordinal tie-breaking.
14. Replace stale retained-history banners with milestone-neutral wording and
    a live documentation pointer.

## Final-review amendment

The required independent portability/GPU/performance review found four
additional actionable gaps before the corrected checkpoint could be frozen.
They are accepted into this same bounded Milestone 8 correction wave:

15. Route every recursive fixture deletion, including direct
    `cmake -E rm -rf` registrations, through the dedicated guarded test
    workspace and make the guard regression scan all recursive-deletion
    spellings and registrations.
16. Select a CUDA ELF baseline with CMake's actual detected CUDA host compiler
    identity and version, not the unrelated C++ compiler variables, and prove
    a configure-level unlike-host case is non-enforcing.
17. Classify only successful zero-device enumeration as CTest code-77 skip;
    CUDA enumeration errors must remain failures with a direct regression.
18. Give every reported CPU/CUDA performance row an operation-specific
    correctness oracle and record the complete local environment, flags,
    candidate identity, synchronization, and noise boundary required by the
    approved performance methodology.
19. Preserve the declared CMake 3.25+ CUDA configuration contract: exact CUDA
    host compiler identity may be enforced only when CMake exposes the actual
    detected host ID/version (3.31+); older CMake must ignore identically named
    untrusted cache values, select no enforcing CUDA ELF baseline, and have
    direct unavailable-identity and spoofed-cache regressions.
20. Keep the pre-3.31 spoofed-cache regression valid when NVCC chooses its
    default host compiler: an empty explicit host override must be accepted
    and omitted from the nested configure while the integrated baseline
    remains non-enforcing.

## Required evidence

- Every production defect receives a focused regression that fails against the
  reviewed pre-correction behavior or rigorously exercises its fault boundary.
- Package, deletion-safety, downstream opt-in, baseline selection, and
  no-device classification receive direct CMake/CTest coverage.
- Clean CPU compiler/configuration/linkage, sanitizer-safe, package,
  relocation, and isolated-consumer validation is rerun.
- Available real-device CUDA runtime/parity and Compute Sanitizer evidence is
  rerun after CUDA-lifetime changes.
- Formatting, dependency/target/header manifests, provenance, and
  documentation consistency are rechecked.
- Independent verification, documentation/API review, and portability/GPU/
  performance review run after lead integration.

## Stop condition

Stop at a corrected Publication Checkpoint B. Do not commit, push, mutate a
pull request, merge, tag, release, or delete a branch.
