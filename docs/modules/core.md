# Core module

`ASC::core` provides the lowest-level contracts used by asc-cpp. Core
Milestone 1 (M1) adds a canonical serial foundation without changing the
MdeCpp-derived runtime that legacy arrays still use. Array M1 has since
added a separate canonical `Tensor`/`TensorView` path over `Buffer<T>`, and
Linalg M1 uses those views with an explicit Core context and
status values. Random M1 likewise uses explicit Core execution and status
contracts for deterministic canonical-view filling; its keys and counters
remain caller-owned Random values.

This distinction matters when choosing an API:

| API family | M1 status | Intended use |
| --- | --- | --- |
| Types, status/result, and contracts | Canonical | New code |
| `MemorySpace`, `MemoryResource`, and `Buffer<T>` | Canonical, serial host resource only | New low-level ownership code |
| `ExecutionContext` and `Event` | Canonical, synchronous serial provider only | New explicit execution code |
| Array M1 `Tensor` ownership over `Buffer<T>` | Canonical, synchronous serial host path | New dense ownership |
| Linalg M1 `serial-reference` operations | Canonical consumer of `ExecutionContext`, `Status`, and Array views | New dense linear algebra |
| Random M1 Philox/uniform/fill path | Canonical consumer of `ExecutionContext`, `Status`, fixed-width types, and Array views | New deterministic generation |
| `Memory<T>`, `MemoryManager mm`, and `Device` | Legacy compatibility, unchanged | Existing array implementation and migration only |
| `Read`/`Write`, `UseDevice`, and `forall` | Legacy compatibility, unchanged | Existing call sites only |
| Canonical OpenMP/CUDA resources and contexts | Planned, not implemented in M1 | Do not rely on them yet |

New code should include the canonical umbrella and link the minimal component:

```cpp
#include <asc/core.h>
```

```cmake
find_package(ASCCpp REQUIRED COMPONENTS core)
target_link_libraries(my_target PRIVATE ASC::core)
```

The following serial-host path is compiled and run by the installed
core-component consumer:

```cpp
#include <asc/core.h>

#include <utility>

int main() {
  const asc::ExecutionContext context = asc::ExecutionContext::Serial();

  asc::Result<asc::MemoryResourcePtr> resource =
      context.GetMemoryResource(asc::MemorySpace::kHost);
  if (!resource.ok()) return 1;

  asc::Result<asc::Buffer<double>> allocation =
      asc::Buffer<double>::Allocate(4, resource.value());
  if (!allocation.ok()) return 1;

  asc::Buffer<double> values = std::move(allocation).value();
  asc::Result<double*> data = values.HostData();
  if (!data.ok()) return 1;

  for (asc::extent_t i = 0; i < values.GetSize(); ++i) {
    data.value()[i] = static_cast<double>(i + 1);
  }

  return context.Synchronize().ok() ? 0 : 1;
}
```

Allocation, host access, and synchronization can each fail independently, so
production code should propagate the corresponding status rather than collapse
all failures to an integer as this small consumer does.

The common canonical headers use the C++ standard library and asc-cpp's
generated configuration. They do not expose CUDA, OpenMP, Eigen, MKL, or other
provider SDK types.

## Fundamental types

`<asc/core/types.h>` defines signed 64-bit metadata aliases:

- `index_t` for logical indices and offsets;
- `extent_t` for element counts and extents;
- `stride_t` for strides;
- `nnz_t` for sparse nonzero counts;
- `dynamic_extent`, whose value is `-1`.

`std::size_t` remains the byte-count type. Canonical allocation paths reject
negative counts and check the element-count-to-byte-count conversion before
calling a resource. The existing integer `kDynamicExtent` is retained
separately for legacy arrays in M1.

`real_t` remains the build-selected default floating-point type. It is a
convenience and ABI choice, not a restriction on generic algorithm scalar
types.

## Status and results

`<asc/core/status.h>` provides two value-oriented error types:

- `Status` represents success or a failure code with diagnostic information;
- `Result<T>` contains either a `T` or a failed `Status`.

The stable status codes cover invalid arguments and ranges, failed
preconditions, overflow, allocation failure, unavailable or unsupported
capabilities, backend errors, numerical failure, and internal errors. A
provider name, native provider code, and message may add diagnostics. Do not
parse message or provider text as a stable interface.

Always inspect a status before continuing and a result before accessing its
value. `Result<T>` supports move-only values, including `Buffer<T>`. Accessing
the missing value of a failed result is a contract violation.

Status-returning interfaces have the same signatures whether legacy exception
translation is enabled or disabled. Recoverable allocation and provider
failures are represented by status values rather than by changing the API at
configuration time.

## Public contracts

`<asc/core/contracts.h>` separates programmer errors from recoverable runtime
failures:

- `ASC_REQUIRE` checks a public precondition;
- `ASC_ENSURE` checks a public postcondition;
- `ASC_DCHECK` checks an internal invariant in debug builds.

`ASC_REQUIRE` and `ASC_ENSURE` remain active in release builds and evaluate
their condition once. In an exception-enabled build, failure is translated to
the canonical contract exception. Otherwise the library emits a minimal
diagnostic and aborts. This canonical path has no mutable global error action.

Use a returned `Status` for failures a caller can handle, such as an unavailable
provider or failed allocation. Use a contract for violated API rules that
indicate a programming error.

The older `ASC_VERIFY`, `ASC_ASSERT`, `ASC_ABORT`, and `ErrorAction` interfaces
retain their existing behavior for compatibility; they are not aliases for the
new contract model in M1.

## Memory spaces

`<asc/core/memory_space.h>` distinguishes four address spaces:

| Space | Host accessible | Device accessible | M1 resource |
| --- | ---: | ---: | --- |
| Host | Yes | No | Yes |
| Pinned host | Yes | No | No |
| Device | No | Yes | No |
| Managed/unified | Yes | Yes | No |

The accessibility entries describe the space contract, not provider
availability. M1 supplies only a host resource. It does not emulate pinned,
device, or managed memory with ordinary host allocation.

External ownership is not a memory space. External adoption and non-owning
views require separate lifetime contracts and are deferred to later
milestones.

## Memory resources

`<asc/core/memory_resource.h>` defines the allocation boundary.
`MemoryResource` reports its space and diagnostic name, allocates bytes with an
explicit alignment, deallocates without throwing, and supports resource
equality. Resource handles are shared so an allocation can retain the resource
needed to release it.

M1 ships one immutable, thread-safe host resource. Zero-byte allocation is a
successful no-allocation operation. Invalid alignment and allocation failure
are returned as status failures.

A custom resource must obey the same allocation/deallocation pairing: the
pointer, byte count, and alignment passed to deallocation correspond to the
successful allocation. A resource must not claim a memory space whose
allocation and accessibility rules it does not implement.

## `Buffer<T>` ownership

`<asc/core/buffer.h>` provides the canonical low-level owner.

- A buffer owns one allocation in one explicit memory space.
- Default construction creates a valid empty buffer.
- Allocation is performed by a factory returning `Result<Buffer<T>>`.
- Copy construction and copy assignment are disabled.
- Move construction and assignment transfer ownership and leave the source
  empty.
- Destruction releases exactly once and does not throw.
- The buffer retains the resource that must deallocate its storage.
- Host access is checked and succeeds only for a host-accessible space.
- There is no implicit pointer conversion, manual deletion, resize, alias,
  mirror, execution preference, or public raw device-pointer accessor.

Non-trivial objects in host-accessible storage are constructed and destroyed
as objects. A partially completed construction is rolled back before an
allocation failure is returned.

Deep copy and clone operations are intentionally absent in M1. Their final
interfaces must include an execution context and explicit source, destination,
and synchronization semantics. Do not use the legacy mirrored-memory API to
make a canonical `Buffer<T>` appear copyable.

## Execution contexts

`<asc/core/execution_context.h>` makes execution choice explicit.
`ExecutionContext` is a cheap, copyable handle to immutable provider state and
construction options. Those options identify:

- backend kind and device identifier;
- fallback policy;
- determinism policy.

The serial context is always available and deterministic. It provides the host
resource and synchronous execution capability. Creating an OpenMP or CUDA
canonical context in M1 returns an unavailable status; enabling a legacy build
option does not change that fact.

Fallback is disabled by default. A context never performs an implicit transfer
to satisfy an operation. A resource query for an unsupported space fails before
a pointer is acquired.

Contexts contain no mutable setters or global provider registry. Independent
contexts hold independent options and state. Copies of an immutable serial
context may be inspected and used concurrently. M1 deliberately has no mutable
global or thread-local default context: canonical operations require the
context explicitly.

The Linalg M1 operations follow this rule: all seven take a
mandatory context first, query a compiled capability boundary, and return
`Status` or `Result<T>`. They do not make Core OpenMP/CUDA contexts available,
and legacy backend switches do not alter their serial context.

The legacy `Device` singleton remains the execution default for legacy arrays
and loops only. Configuring it does not configure or mutate a canonical
`ExecutionContext`.

## Events and lifetime

`<asc/core/event.h>` defines a copyable shared-state `Event` handle.

- A default event is completed successfully.
- `IsReady()` is a non-blocking readiness query.
- `Wait()` is idempotent and returns the provider status.
- The event identifies its backend kind.
- Event destruction does not silently wait.

M1 serial work completes synchronously, so M1 events are completed events. The
event retains provider event state, not input or output buffers. Future
asynchronous operations will require the caller to keep every referenced
buffer and view alive until the event completes.

## What is still legacy

The following narrow headers remain available because legacy array, linalg,
and random code depends on their behavior:

- `<asc/core/memory.h>` and `<asc/core/memory_impl.h>`;
- `<asc/core/device.h>` and `<asc/core/forall.h>`;
- `<asc/core/cuda.h>`;
- `<asc/core/error.h>` and `<asc/core/globals.h>`;
- the device-coupled operation functors in `<asc/core/operators.h>`.

These interfaces still provide manual `Memory<T>::Delete()`, global
`MemoryManager mm`, the process-wide `Device`, lazy mirrored-memory transfers,
and build-dependent loop dispatch. Core M1 does not change or deprecate them,
because the inherited array implementation has not migrated to typed owners
and views. Canonical Array M1 instead adds a separate move-only dense owner and
element-typed view path; it does not adapt these compatibility types.

Do not mix the two ownership models. In particular, a canonical `Buffer<T>` is
not registered with `mm`, and M1 provides no conversion or hidden transfer
between `Buffer<T>` and `Memory<T>`.

## Provider roadmap

The following behavior is planned, not part of M1:

- explicit copy/fill operations and asynchronous events;
- pinned-host, managed-memory, OpenMP, and CUDA resources/providers;
- provider translation units isolated from ordinary C++ consumers;
- allocation-local compatibility mirroring;
- context-scoped diagnostic sinks;
- an explicit CUDA extension path for user-authored kernels;
- eventual removal of global/manual compatibility APIs before 1.0.

Provider availability will be exposed through context capabilities and status
values. Optional providers will not add or remove declarations from the common
canonical API.

## Migration guidance

For new foundational code:

1. include `<asc/core.h>` or the narrow canonical header;
2. pass `ExecutionContext` explicitly;
3. allocate through a `MemoryResource` and own storage with `Buffer<T>`;
4. propagate `Status`/`Result<T>` failures;
5. use release-active contracts for programmer preconditions;
6. do not introduce a dependency on `Device`, `mm`, or implicit mirroring.

Existing array-facing code should remain on the characterized compatibility
path when it needs inherited expressions, sparse storage, transforms, or
device/mirroring behavior. New dense host code may adopt Array M1 when it can
use the new ownership, lifetime, and constness rules coherently. See
[Array migration](../migration/array.md) for that mapping and
[Core migration](../migration/core.md) for the staged runtime mapping. New
dense linear algebra should follow the
[Linalg module](linalg.md) and [Linalg migration](../migration/linalg.md)
contracts rather than routing a canonical view through the legacy runtime.
New unit-uniform generation should follow the
[Random module](random.md) and [Random migration](../migration/random.md)
contracts; entropy, mutable engines, inherited samplers, and device generation
are not Core M1 capabilities.
