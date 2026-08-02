# API map

All supported public declarations are directly in `namespace asc`. Prefer a
module umbrella for application code and a narrow owning header for reusable
libraries.

## Component map

| Component | Build target | Consumer target | Kind | Direct ASC dependency |
| --- | --- | --- | --- | --- |
| Core | `asc_core` | `ASC::core` | Compiled | None |
| Utilities | `asc_utilities` | `ASC::utilities` | Compiled | `ASC::core` |
| Expression | `asc_expression` | `ASC::expression` | Interface | `ASC::core` |
| Dense | `asc_dense` | `ASC::dense` | Compiled | `ASC::core`, `ASC::expression` |
| Sparse | `asc_sparse` | `ASC::sparse` | Compiled | `ASC::core`, `ASC::expression` |
| Random | `asc_random` | `ASC::random` | Compiled | `ASC::core` |
| Random Dense | `asc_random_dense` | `ASC::random_dense` | Interface | `ASC::random`, `ASC::dense` |
| Random Sparse | `asc_random_sparse` | `ASC::random_sparse` | Interface | `ASC::random`, `ASC::sparse` |
| Aggregate | `asc_cpp` | `ASC::cpp` | Interface | All provider-free targets |
| Core CUDA | `asc_core_cuda` | `ASC::core_cuda` | Compiled, optional | `ASC::core`; private `CUDA::cudart` |
| Dense CUDA | `asc_dense_cuda` | `ASC::dense_cuda` | Compiled, optional | `ASC::dense`, `ASC::core_cuda`; private `CUDA::cublas` |
| Sparse CUDA | `asc_sparse_cuda` | `ASC::sparse_cuda` | Compiled, optional | `ASC::sparse`, `ASC::core_cuda`; private `CUDA::cusparse` |
| Random CUDA | `asc_random_cuda` | `ASC::random_cuda` | Compiled, optional | `ASC::random`, `ASC::core_cuda` |
| Random Dense CUDA | `asc_random_dense_cuda` | `ASC::random_dense_cuda` | Compiled, optional | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` |
| Random Sparse CUDA | `asc_random_sparse_cuda` | `ASC::random_sparse_cuda` | Compiled, optional | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` |

Dense and Sparse load Core and Expression but do not load one another.
Utilities, Expression, and base Random neither include nor link one another.
Storage modules do not import Random; the Random-owned facets provide the
approved integration points.

## Core

Umbrella: `<asc/core.h>`.

- `<asc/core/status.h>` and `<asc/core/result.h>`: stable error categories and
  value-or-error transport.
- `<asc/core/contracts.h>`: release-active and debug-only programmer
  contracts.
- `<asc/core/types.h>` and `<asc/core/extents.h>`: checked signed 64-bit
  metadata, byte counts, and compile-time-rank extents.
- `<asc/core/configuration.h>`: recursive programmatic configuration values,
  object schemas, validation, origins, metadata, and redaction.
- `<asc/core/io.h>`: partial byte interfaces, exact/all transfer helpers,
  move-only local files, bounded text files, and little-endian scalars.
- `<asc/core/memory.h>`: memory spaces, host allocation, move-only byte
  ownership, and non-owning byte views.
- `<asc/core/execution.h>`: backend-neutral vocabulary, immutable serial
  execution, overlap-safe host copies, and completed move-only events.
- `<asc/core/export.h>`: Core shared/static visibility macros.
- `<asc/core/providers/cuda.h>`: optional explicit CUDA device discovery,
  pinned/device/managed resources, execution contexts, copies, and events.
- `<asc/core/providers/cuda_export.h>`: Core CUDA shared/static visibility
  macros.

See the [Core module guide](modules/core.md).

## Utilities

Umbrella: `<asc/utilities.h>`.

- `<asc/utilities/command_line.h>`: a schema-directed, transactional
  command-line parser, positional results, command-line origins, and
  deterministic caller-owned help text.
- `<asc/utilities/timer.h>`: an empty/running/stopped monotonic timer with
  completed interval, total, last, and average queries.
- `<asc/utilities/export.h>`: Utilities shared/static visibility macros.

Utilities parses command-line tokens only. It does not parse local files,
environment variables, response files, list repetition, or implicit
programmatic merges.

See the [Utilities module guide](modules/utilities.md).

## Expression

Umbrella: `<asc/expression.h>`.

- `<asc/expression/expression.h>`: `ExpressionAdapter<T>`,
  `ReadableExpression`, alias and sparsity metadata, scalar terminals, safe
  lvalue/rvalue holders, optional recursive access validation, and pointwise
  negate/add/subtract/multiply nodes.
- `<asc/expression/writable.h>`: opt-in placed-readable and writable
  customization, memory placement, conservative byte-span aliases, and
  coordinate writes for existing caller-owned destinations.

Expression defines no storage, evaluator, allocation, reduction, provider, or
result materialization. Ranked operands require exact shape. The only
pointwise expansion is rank-zero scalar against one ranked operand.

See the [Expression module guide](modules/expression.md).

## Dense

Umbrella: `<asc/dense.h>`.

- `<asc/dense/layout.h>`: checked left-, right-, and explicit-stride mappings.
- `<asc/dense/view.h>`: typed non-owning dense views and rank-preserving
  subviews.
- `<asc/dense/array.h>`: move-only typed dense ownership backed by Core
  buffers and memory resources.
- `<asc/dense/evaluate.h>`: destination evaluation and scalar reductions.
- `<asc/dense/blas.h>`: allocation-free serial CPU BLAS, including the
  complete classic Level 1 real/complex surface over checked signed-stride
  vector descriptors, all 66 applicable classic Level 2 real/complex rows,
  and all 30 applicable classic Level 3 rows over checked matrix descriptors.
- `<asc/dense/export.h>`: Dense shared/static visibility macros.
- `<asc/dense/providers/cuda.h>`: optional move-only CUDA Dense context,
  bounded pointwise evaluation, complete classic Level 1 real/complex BLAS,
  all applicable classic Level 2 and Level 3 real/complex BLAS.
- `<asc/dense/providers/cuda_export.h>`: Dense CUDA shared/static visibility
  macros.

Successful Dense BLAS and evaluation calls do not dispatch to an optional
provider, transfer memory, synchronize, allocate temporaries, or expose a
sparse or random-storage facet. CUDA scalar results remain in caller-owned
device storage; CUDA Iamax also takes caller-owned device workspace for
provider-index conversion. A CUDA failure after possible enqueue follows the
documented stream-drain recovery rule before returning its error.

See the [Dense module guide](modules/dense.md).

## Sparse

Umbrella: `<asc/sparse.h>`.

- `<asc/sparse/coordinate.h>`: explicit-capacity coordinate builders,
  duplicate/zero finalization policies, and canonical coordinate owners/views.
- `<asc/sparse/compressed.h>`: canonical rank-two CSR/CSC owners/views and
  named coordinate/compressed conversions.
- `<asc/sparse/evaluate.h>`: allocation-free, structure-preserving evaluation
  into an existing Sparse structure.
- `<asc/sparse/blas.h>`: allocation-free serial CSR SpMV for `float` and
  `double` over storage-neutral placed vector operands.
- `<asc/sparse/export.h>`: Sparse shared/static visibility macros.
- `<asc/sparse/providers/cuda.h>`: optional move-only CUDA Sparse context,
  canonical host-CSR cloning, CSR SpMV, and bounded structure-preserving
  evaluation.
- `<asc/sparse/providers/cuda_export.h>`: Sparse CUDA shared/static visibility
  macros.

Sparse never imports Dense, densifies, grows a finalized structure, hides a
conversion/workspace, or dispatches to an optional provider.

See the [Sparse module guide](modules/sparse.md).

## Random

Umbrella: `<asc/random.h>`.

- `<asc/random/engine.h>`: versioned SplitMix64, PCG32, xoroshiro64*, and
  xoroshiro128+ value engines plus fixed-width Philox4x32-10 counter/key
  blocks, explicit addressing, and checked offset advancement.
- `<asc/random/distribution.h>`: unbiased closed uniform integers, half-open
  uniform `float`/`double`, scalar Box-Muller normal, and exact raw-word
  `Uniform01` transforms.
- `<asc/random/generator.h>`: nonvirtual engine/distribution value composition
  and transparent uniform/normal aliases.
- `<asc/random/quasi.h>`: caller-span prime/radical-inverse/permutation helpers,
  Latin hypercube, indexed Halton/Hammersley/Sobol, explicit Sobol direction
  initialization, and a checked sequential Sobol index value.
- `<asc/random/seed.h>`: explicit `Result<uint64_t>` acquisition from one
  caller-owned full-width `std::random_device`.
- `<asc/random/dense.h>`: deterministic logical-order uniform and generic
  pseudo fills, prepared multivariate-normal and unit-sphere sampling, and
  Dense Latin/Halton/Hammersley/Sobol adapters over explicit caller views and
  workspaces.
- `<asc/random/sparse.h>`: structure-preserving coordinate/CSR/CSC value
  fills, exact-count structure-only ordinal selection into caller workspace,
  and combined canonical coordinate-owner generation with separate
  structure/value address domains.
- `<asc/random/export.h>`: Random shared/static visibility macros.
- `<asc/random/providers/cuda.h>`: asynchronous
  `CudaRandomWordGeneration` over the Core `ExecutionContext`, with
  `CudaFillPhilox4x32` accepting a capacity-carrying `MutableMemoryView`,
  explicit word count, and explicit Random address.
- `<asc/random/providers/dense_cuda.h>`: CUDA logical-order uniform filling of
  caller-provided Dense views.
- `<asc/random/providers/sparse_cuda.h>`: CUDA exact-count canonical Sparse
  generation with separate structure and value addresses.
- `<asc/random/providers/cuda_export.h>`,
  `<asc/random/providers/dense_cuda_export.h>`, and
  `<asc/random/providers/sparse_cuda_export.h>`: Random CUDA facet
  shared/static visibility macros.

Base Random owns no storage and includes neither facet header. Its mutable
engines are explicit caller-owned values, and its nondeterministic source is
never invoked implicitly. The facets adapt caller-owned Dense and Sparse
contracts without introducing a default engine, hidden allocation/workspace,
provider, factorization package, or new GPU code. All Issue 15 additions
reject non-serial contexts before access or mutation.

See the [Random module guide](modules/random.md) and
[frozen provenance record][random-provenance].

## Aggregate and optional providers

`ASC::cpp` aggregates all six provider-free modules and both Random storage
facets. It owns no production behavior and is never a dependency of a narrower
target.

The six `*_cuda` components are available only in packages built with
`ASC_CPP_ENABLE_CUDA=ON`. Provider-free or no-component package lookups do not
load them or discover CUDAToolkit. Each provider component loads its exact
declared closure; requesting `random_dense_cuda`, for example, does not load
Sparse or Random Sparse. Public provider headers expose no native CUDA,
cuBLAS, or cuSPARSE type.

[random-provenance]: development/asc-cpp-m2-independent-foundations/provenance-record.md
