# Verification strategy

Status: Implemented through the Issue 9 local Feature Gate B candidate

## Test ownership

Tests live with the owning module/facet and link the narrowest installed target.
An `ASC::cpp` test never substitutes for a component-isolation test.

## Required layers

1. **Compile contracts**
   - every public `.h` is the only project include in one C++20 TU;
   - direct include-what-you-use and full-path guard checks;
   - positive and negative concepts/overloads;
   - representative multi-TU ODR instantiation;
   - provider-neutral headers compile on a host with no provider SDK.
2. **Architecture**
   - source include scan against `dependency-manifest.yaml`;
   - direct build-target edge audit;
   - installed imported-target graph audit;
   - each module/facet configured and consumed with forbidden siblings absent;
   - no unimplemented component/target export.
3. **Unit and property**
   - empty, zero-rank, zero-extent, boundary, overflow, invalid state;
   - ownership/move/clone/view lifetime and failure rollback;
   - shape/layout/format invariants and randomized property checks;
   - parser state machines and deterministic random vectors.
4. **Numerical**
   - independent exact or high-precision oracles;
   - operation-specific absolute/relative/ULP comparisons;
   - residual/backward error for factorization/solver work;
   - NaN, infinity, signed zero, extreme finite values, degenerate shapes,
     transpose/conjugate, and `beta == 0`.
5. **Runtime safety**
   - ASan/UBSan CPU suites;
   - TSan where runtime-supported, reported as skip otherwise;
   - allocation/resource failure injection;
   - context/event/workspace/owner lifetime permutations;
   - GPU memory/race tools as separately reported hardware jobs.
6. **Packaging**
   - build-tree and install-tree packages;
   - relocation and path containing spaces;
   - base modules and random facets independently;
   - no-component `cpp` behavior;
   - unavailable/unknown required-component failure;
   - provider-disabled CPU-only package;
   - requested provider dependency discovery only;
   - static/shared and single/multi-config when claimed.
7. **Documentation and examples**
   - every public example is compiled and run;
   - links and component names validated;
   - generated capability tables checked against manifests.
8. **Performance**
   - allocation, packing, transfer, synchronization counters;
   - workspace query conformance;
   - compile-time/object-size metrics for template ranks/expressions;
   - benchmarks with recorded environment and robust statistics.

## Module-specific falsification

### Core

- signed/unsigned/provider-width checked conversions;
- rank-zero and zero-extent products;
- short I/O and version/endian failures;
- allocation alignment, partial construction rollback, exactly-once release;
- non-host access rejection;
- independent contexts, unavailable providers, event ordering.

### Utilities

- negative numeric arguments, `--`, positional/unknown/duplicate behavior;
- transaction rollback and precedence/source provenance;
- secret redaction;
- empty/running/stopped/reset timer states.

### Expression

- external type with no ASC inheritance;
- lvalue, rvalue, moved, nested temporary, and stored-node lifetime;
- exact-shape and rank-zero-scalar rules;
- alias and sparsity metadata;
- proof that construction does not allocate, evaluate, transfer, or dispatch.

### Dense

- left/right/padded/noncontiguous unique mappings;
- const propagation, slices, owner/view lifetime, resize invalidation;
- safe exact alias and rejected overlap;
- no hidden materialization/packing;
- reference dense operations and provider parity.

### Sparse

- invalid offsets/indices/base/width;
- unsorted/duplicate/explicit-zero policies;
- empty compressed invariant and COO/CSR/CSC round trips;
- structure immutability and view invalidation;
- no densification under a strict allocation resource;
- reference CSR SpMV with generic vector descriptors.

### Random and facets

- independently derived engine/transform vectors;
- key/counter/stream/subsequence/overflow;
- logical layout/stride/partition equivalence;
- exact sparse count, uniqueness, order, and independent structure/value
  streams;
- CPU/GPU bit equality only for explicitly promised rows;
- statistical smoke failures print the reproducing state and use non-flaky
  bounds.

## GPU evidence levels

| Level | Meaning |
| --- | --- |
| configure-tested | toolkit/provider discovery succeeds |
| compile-tested | provider target compiles and links |
| runtime-tested | an asc-cpp operation executes on named real hardware |
| parity-tested | result meets the declared CPU/reference oracle |

Missing hardware is `skipped`, never passed. Toolkit discovery, CUDA compiler
ABI smoke, and `nvidia-smi` visibility do not satisfy `runtime-tested`.

Milestone 6 records `core_cuda` as configure-tested, compile-tested, and
runtime-tested, and records `dense_cuda` as configure-tested, compile-tested,
runtime-tested, and parity-tested on the exact local RTX 3060 Laptop host.
Milestone 7 applies the same independent labels to `sparse_cuda` and the three
random CUDA facets for their exact tested subsets. Hosted GPU, multi-device
runtime, and trusted device CSC staging evidence are skipped explicitly.
Milestone 8 revalidates those labels without widening their capability scope.
Issue 9 adds runtime and parity evidence for exactly the 30 approved Dense
Level 3 rows on the named RTX 3060 Laptop GPU; forced zero-device cases remain
explicit skips and do not verify a CUDA coverage row.

## MdeCpp test policy

MdeCpp tests are GPL-covered behavioral prompts. They are not copied,
mechanically translated, or used as the sole source of literal expected data.
New tests are written from accepted contracts and independent mathematical or
approved upstream evidence. Differential execution is secondary evidence only.

## Milestone gate

Every milestone must leave:

- a buildable tree;
- an acyclic exact graph;
- focused and full suites green;
- no unexplained skip;
- package relocation green;
- documentation no stronger than evidence;
- a recorded exact command/tool/result summary.
