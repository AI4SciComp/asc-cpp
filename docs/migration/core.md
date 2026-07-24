# Core migration

Core is migrating from the MdeCpp-derived global memory/device runtime to the
explicit foundation approved in the architecture blueprint. The migration is
staged because current arrays depend on legacy alias registration, lazy
host/device mirroring, and manual lifetime behavior.

## Milestone status

| Milestone | Status | Scope |
| --- | --- | --- |
| Core M1 | Implemented | Add canonical types, status/contracts, host resource, move-only `Buffer<T>`, serial `ExecutionContext`, and completed `Event` |
| Core M2 | Planned | Characterize and adapt compatibility paths; introduce allocation-local compatibility state where array migration permits |
| Core M3 | Planned | Isolate real OpenMP/CUDA resources, contexts, events, and provider translation units |
| Array M1 | Implemented | Add a separate move-only `Tensor` and typed `TensorView` path over canonical Core without adapting legacy arrays |
| Linalg M1 | Implemented | Add seven serial-reference operations consuming canonical views, explicit contexts, and status/results |
| Random M1 | Implemented | Add explicit key/counter Philox generation, unit-uniform transform, and serial canonical-view fill |
| Later Array migration | Planned | Migrate inherited dense storage inward before global pointer identity and manual deletion can be removed |
| Pre-1.0 cleanup | Planned | Remove obsolete global/manual compatibility APIs after downstream migration |

Core M1 is additive. It does not deprecate or change a legacy interface.

## API mapping

| Legacy API or mechanism | Canonical direction | M1 disposition |
| --- | --- | --- |
| integer extents and `kDynamicExtent` | signed 64-bit metadata aliases and `dynamic_extent` | Canonical aliases added; legacy value retained for arrays |
| `ASC_VERIFY`/`ASC_ASSERT` and mutable `ErrorAction` | release-active contracts plus `Status`/`Result<T>` | Canonical path added; legacy behavior unchanged |
| `MemoryType`/`MemoryClass` | explicit `MemorySpace` properties | Canonical description added; no conversion facade |
| `MemoryManager mm` | allocation-owned `MemoryResource` handle | Host resource added; global manager unchanged for legacy arrays |
| manually deleted, shallow-copyable `Memory<T>` | move-only RAII `Buffer<T>` | Canonical owner added; no implicit conversion or shared registry |
| `Read`/`Write` and implicit mirroring | explicit space-aware copy operations | Planned in Core; Array `Clone` and Linalg vector `Copy` are synchronous host operations, not general transfer APIs |
| `UseDevice` execution preference | explicit `ExecutionContext` argument | Serial context added; legacy flag unchanged |
| process-wide `Device` | independently constructed immutable contexts | Serial context added; Device remains legacy-only |
| `forall` macros and CUDA-compiled consumers | coarse compiled providers plus explicit user-kernel extension | Legacy behavior unchanged; provider isolation planned |
| implicit completion/default stream | explicit `Event` | Completed serial event added; real asynchronous providers planned |
| global diagnostic streams | context/scoped diagnostic sink | Planned; `mout` and `merr` unchanged |

## Guidance for new code

New foundational code should use canonical core APIs when its needs fit M1:

- propagate `Status` and `Result<T>`;
- use `ASC_REQUIRE`/`ASC_ENSURE` for public programmer contracts;
- store metadata in the fixed-width aliases;
- allocate host storage through `MemoryResource` and own it with `Buffer<T>`;
- pass a serial `ExecutionContext` explicitly;
- treat events as provider state, not operand-lifetime ownership.

Do not migrate an existing array call site by wrapping the same pointer in both
`Memory<T>` and `Buffer<T>`. The two owners have unrelated registries and
deallocation contracts, so doing so can create double ownership or stale
legacy alias state.

Code requiring legacy array views, mirrored host/device access, or device
`forall` remains on the compatibility path. Array M1's canonical host-only
owner/view path does not adapt those facilities. New dense host callers may use
it, while legacy device/mirroring callers remain unchanged. Do not emulate a
missing canonical operation with a hidden legacy transfer. See
[Array migration](array.md) for the dense mapping. The Linalg M1
operations use the same explicit serial context and host-access boundary; see
[Linalg migration](linalg.md). Their `Copy` is rank-one arithmetic work, not a
Core memory-space transfer.
Random M1 also takes the serial context explicitly and returns `Status`; its
counter state is caller-owned integer data rather than `ExecutionContext`
state. See [Random migration](random.md).

## Deliberate M1 boundaries

M1 does not provide:

- a conversion between `Memory<T>` and `Buffer<T>`;
- deep copy, clone, resize, mirroring, or external adoption for `Buffer<T>`;
- canonical pinned, device, or managed resources;
- canonical OpenMP or CUDA contexts;
- a mutable default context;
- incomplete/asynchronous provider events;
- provider registration or a public plugin ABI.

The presence of an enum value or legacy build option is not evidence that a
canonical provider is available. Query context creation/capabilities and
handle failure through status values.

## Compatibility window

The architecture permits legacy facades for a bounded pre-1.0 transition.
Removal occurs only after:

1. legacy array owners and views no longer depend on `Memory<T>` alias flags;
2. explicit transfer and execution APIs cover supported use cases;
3. known downstream users migrate;
4. package, backend, sanitizer, and compatibility tests pass;
5. a breaking release and removal table are published.

No specific removal release is declared by Core M1. Users will receive
deprecation guidance only after an equivalent canonical migration path exists.

## Provenance

The legacy files were selected and adapted from MdeCpp as recorded in
[the migration inventory](inventory.md). The canonical M1 interfaces are a new
asc-cpp architecture derived from
[`architecture_blueprint_v1.md`](../design/architecture_blueprint_v1.md) and
[`core_design.md`](../design/core_design.md); they are not a renamed MdeCpp
runtime.
