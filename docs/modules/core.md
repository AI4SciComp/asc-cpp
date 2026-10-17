# Core module

`ASC::core` is the provider-free CPU foundation of asc-cpp. It has no direct
ASC or external dependency, and its public headers use only C++20
standard-library facilities. CUDA Core and Dense adds the separately requested
`ASC::core_cuda` provider facet; it does not add CUDA to the base target or
umbrella.

Request the component and include its umbrella:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core)
target_link_libraries(my_target PRIVATE ASC::core)
```

```cpp
#include <asc/core.h>
```

The umbrella exposes the original ten narrow headers under `<asc/core/>`.
The additional `<asc/core/array_format.h>` is included explicitly; it supplies
storage-neutral scalar identities and bounded display options/reports shared
by Dense and Sparse. Core owns neither module's shape traversal or storage
schema. All public declarations are directly in `namespace asc`.

## Errors, results, and contracts

`ErrorCode` is a stable classification for success and the Core failure
domains: invalid arguments, shape, index, overflow, invalid state,
allocation, memory access or transfer, unsupported or unavailable
capabilities, providers, numerical operations, configuration, I/O, EOF,
encoding, versions, and internal failures.

`Status` is a nodiscard value. A successful status has `ErrorCode::kOk`;
failed statuses may also carry diagnostic text, a provider name, and a signed
native provider code. These details help diagnosis but are not stable
machine-readable interfaces. In particular, do not branch on message text.

`Result<T>` is a nodiscard discriminated value containing either a `T` or a
non-OK `Status`. It supports move-only values. Test `ok()` before reading the
value, and inspect `status()` when it is false. Reading the value from a failed
result invokes the release-active fatal-contract path; it does not throw a
recoverable public exception.

The contract macros serve programmer errors:

- `ASC_CHECK(condition)` is active in every build.
- `ASC_DCHECK(condition)` is active only in debug builds.

Each active macro evaluates its condition once. A failed active check calls
`FatalContract`; Core exposes no mutable global failure handler. Use a returned
`Status` or `Result<T>` for failures a caller can recover from, including bad
input data, allocation failure, unavailable execution, and I/O errors.

## Logical metadata and extents

`<asc/core/types.h>` defines:

| Type | Representation | Purpose |
| --- | --- | --- |
| `index_t` | signed 64-bit | logical indices and offsets |
| `extent_t` | signed 64-bit | logical extents and counts |
| `stride_t` | signed 64-bit | logical strides |
| `nnz_t` | signed 64-bit | nonzero counts |
| `rank_t` | unsigned 32-bit | rank values |
| `std::size_t` | implementation standard | byte counts |

`kDynamicExtent` is the distinct signed value `-1`.

Checked integral conversion, addition, multiplication, and element-to-byte
conversion return `Result` values. They reject negative-to-unsigned and
out-of-range conversions and avoid undefined signed overflow. Use them at
public boundaries instead of unchecked casts or arithmetic.

`Extents<...>` has compile-time rank with any mix of static and dynamic
dimensions. Its factory validates every supplied dynamic extent and the
complete logical product before returning an object. Negative extents fail.
Rank zero has logical size one; an extent list containing zero has logical
size zero. Product overflow fails before allocation or publication.

```cpp
using MatrixExtents = asc::Extents<2, asc::kDynamicExtent>;
auto extents = MatrixExtents::Create(5);
if (!extents.ok()) {
  return extents.status();
}
// rank() == 2, dynamic_rank() == 1, logical_size() == 10
```

## Programmatic configuration

`ConfigurationValue` owns one recursively nested value of exactly these
types:

- null;
- `bool`;
- signed or unsigned 64-bit integer;
- `double`;
- validated UTF-8 string bytes;
- list of configuration values; or
- string-keyed object of configuration values.

String values are created with `ConfigurationValue::Utf8String`; the deleted
`const char*` constructor prevents accidental unvalidated strings. Object and
schema keys are opaque string bytes, not values of the UTF-8 alternative.

No parser is part of Core. Command-line arguments, environment variables,
response files, local configuration files, and numerical container types are
outside this API.

`ConfigurationSchema` describes a recursive object. Each field has an exact
type and may specify required, default, deprecated, or sensitive state,
nested fields, and bounds appropriate to numeric or sized values. Validation
rejects unknown keys and does not silently convert or truncate numeric types.
Defaults are subjected to their own declared constraints before insertion.

`ValidateConfiguration` is transactional. It returns a `Configuration` only
after the entire input and every inserted default have passed validation.
Failure publishes no partially validated output.

Each successful path records a `ConfigurationOrigin`: default, explicit
programmatic, or command-line origin, plus an optional source label and source
location. The stable origin values are `kDefault = 0`, `kProgrammatic = 1`,
and `kCommandLine = 2`. Sensitivity also follows the path. Diagnostic
rendering of a sensitive value returns a redaction marker rather than its
contents. Redaction is a diagnostic boundary; it does not encrypt the owned
value.

Lookup and origin maps use JSON Pointer paths. The empty path denotes the
root, `/name` denotes an object field, decimal tokens index lists, and `~0`
and `~1` escape `~` and `/`. An exact origin entry applies to that value and
its descendants unless a deeper entry overrides it. Defaults always retain
default origin.

Schema objects own their field names, constraints, and default values.
Validated `Configuration` objects independently own their value tree and
per-path metadata; validation does not retain a reference to the input or the
schema. References or pointers obtained from a contained value remain subject
to ordinary C++ container invalidation and the lifetime of the owning
configuration object.

## Byte and text I/O

`ByteSource::ReadSome` and `ByteSink::WriteSome` are partial-transfer
interfaces. A successful operation may transfer fewer bytes than requested.
A zero-byte request succeeds without dereferencing a pointer or touching the
underlying resource.

`ReadExact` loops over partial reads and distinguishes full completion from
clean EOF or truncated input. `WriteAll` loops over partial writes and rejects
a successful zero-progress sink so it cannot spin forever. Callers keep the
supplied destination or source storage alive and unmodified by other threads
for the duration of the call.

`File` is a move-only owner of one native local-file resource. It supports
explicit open, read, write, flush, and close operations. Ownership transfers
on move; destruction and repeated close do not throw. A move-assignment
destination must already be closed; violating that precondition invokes the
release-active fatal-contract path because an implicit close could discard a
close error. Once closed or moved from, operations fail rather than use the
former native resource.

Bounded text-file helpers check file sizes and configured limits before
publishing output. A failed or oversized read publishes no partial
destination.

The fixed-width little-endian helpers encode and decode integer values and IEC
60559 floating values by their specified bits. They do not dump native object
layout and therefore do not inherit native byte order or padding. Dense,
sparse, random-state, logging, and device-transfer formats are not defined in
Core.

## Memory spaces and allocation

`MemorySpace` distinguishes host, pinned host, device, and managed address
spaces. The distinction describes accessibility and is not an availability
claim. Base Core implements host allocation only. The optional
`ASC::core_cuda` facet adds pinned-host, device, and managed resources without
changing `ASC::core`.

`MemoryResource` allocates and deallocates bytes in exactly one declared
space. `HostMemoryResource` supplies explicitly aligned host storage.
Zero-byte allocation is a successful no-allocation operation. Invalid
alignment, byte-count overflow, and allocation failure are reported as
failures.

Every successful nonzero allocation must be returned exactly once to the same
resource with its matching byte count and alignment. A custom resource must
preserve those rules and must not claim a space it does not implement.

### `Buffer` ownership and resource lifetime

`Buffer` is a move-only byte owner:

- a buffer is created by the fallible `Buffer::Allocate` factory;
- a successful zero-byte buffer is valid but owns no allocation;
- a successful nonzero allocation owns one pointer, byte count, alignment,
  and memory space;
- copying is disabled;
- moving transfers ownership and empties the source; and
- destruction or replacement releases a held allocation exactly once.

The buffer retains a **non-owning** `MemoryResource*`. The resource must
outlive every buffer allocated from it. Moving a buffer does not extend the
resource lifetime. Destroying the resource first leaves the buffer unable to
perform its required deallocation and is a caller lifetime error.

Core does not provide buffer adoption, resizing, cloning, implicit pointer
conversion, implicit allocation, mirroring, or hidden transfer.

### Views, aliasing, and lifetime

`ConstMemoryView` and `MutableMemoryView` are non-owning byte descriptors.
They carry an address, byte count, and `MemorySpace`; they do not retain a
buffer or resource and never deallocate.

The view's referenced storage must remain alive and valid for the complete
operation using it. Resetting or destroying the owner invalidates its views.
Moving a buffer does not change the allocation address, but the destination
owner must then remain alive; the moved-from buffer no longer controls that
lifetime. Replacing a buffer by move assignment invalidates views of the
destination's former allocation. A mutable view additionally requires
exclusive mutation discipline from the caller. Two views may alias; an API
documents whether that overlap is permitted.

Serial `CopyBytes` explicitly permits overlap for host source and destination
ranges and behaves like an overlap-safe byte move. CUDA `CopyBytes` permits
exact self-copy as a no-op but rejects partial overlap. Bounds, context,
accessibility, devices, and overlap are validated before host mutation or CUDA
enqueue. No other operation may be assumed to tolerate overlap unless it says
so.

```cpp
asc::HostMemoryResource resource;
auto allocation = asc::Buffer::Allocate(resource, 64);
if (!allocation.ok()) {
  return allocation.status();
}

asc::Buffer buffer = std::move(*allocation);
auto bytes = buffer.mutable_view();
if (!bytes.ok()) {
  return bytes.status();
}
// resource must remain alive until buffer has been reset or destroyed.
```

## Execution contexts and completion events

The backend-neutral vocabulary comprises `Backend`, `Device`, `Determinism`,
`ExecutionContext`, and `CompletionEvent`. Base Core always provides immutable
serial CPU execution on host memory. The optional CUDA factory creates an
immutable, copyable context with provider-owned opaque state; provider-neutral
headers expose no CUDA SDK declaration or type.

There is no process-global or thread-local default context. Pass the context
explicitly to an operation. A request for CUDA, another unavailable backend,
or an unsupported memory space returns `kUnavailable` or `kUnsupported`.
Core never silently selects a provider, falls back, allocates temporary
storage, transfers data, packs input, or synchronizes hidden work.

`CopyBytes` takes an explicit context plus source and destination views. For
the serial context it performs a synchronous overlap-safe host copy and
returns an already-complete move-only `CompletionEvent`.

`CompletionEvent::Query()` reports whether that event has completed without
turning it into a process-wide synchronization. `Wait()` waits only for that
event. An event is move-only. Destroying a CUDA event does not synchronize the
device; destruction therefore does not make it safe to release storage that
an unfinished operation still uses.

## Optional CUDA runtime facet

CUDA is opt-in and is disabled by default. A source build requires
CUDAToolkit 12 or newer and a CUDA compiler:

```sh
cmake -S . -B build-cuda \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86
cmake --build build-cuda
```

Choose architecture codes appropriate for the deployment machines; asc-cpp
does not replace a caller-provided `CMAKE_CUDA_ARCHITECTURES`. Enabling CUDA
builds both `core_cuda` and `dense_cuda`. A missing toolkit, compiler, runtime
target, or cuBLAS target is a configuration error rather than a reason to
disable the provider silently.

An installed consumer requests the facet explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core_cuda)
target_link_libraries(my_target PRIVATE ASC::core_cuda)
```

```cpp
#include <asc/core/providers/cuda.h>
```

The required component closure is exactly `core;core_cuda`. The package finds
CUDAToolkit only because that requested closure contains a CUDA component.
Requesting `core`, any other provider-free component, or the provider-free
`cpp` aggregate does not discover CUDA.

The public provider API is deliberately SDK-neutral:

- `CudaDeviceCount()` reports the number of currently available CUDA devices;
- `CudaMemoryResource::Create(device_ordinal, space)` creates one stable
  resource for pinned-host, device, or managed allocations;
- `CreateCudaExecutionContext(device_ordinal, determinism)` creates one
  nonblocking stream owned by an immutable execution context; and
- `RecordCudaEvent(context)` records completion in that explicit stream.

```cpp
constexpr std::int32_t kDeviceOrdinal = 0;
auto context = asc::CreateCudaExecutionContext(kDeviceOrdinal);
if (!context.ok()) {
  return context.status();
}
auto marker = asc::RecordCudaEvent(*context);
if (!marker.ok()) {
  return marker.status();
}
auto ready = marker->Query();
if (!ready.ok()) {
  return ready.status();
}
if (!*ready) {
  asc::Status waited = marker->Wait();
  if (!waited.ok()) {
    return waited;
  }
}
```

Ordinary host allocation remains `HostMemoryResource` work.
`CudaMemoryResource` is noncopyable and nonmovable because every `Buffer`
allocated from it retains a non-owning resource address. Keep the resource
alive until every such buffer has been reset or destroyed. A CUDA context owns
its execution state and stream, so copied contexts refer to the same immutable
state; there is no default device, global current context, provider registry,
or native-stream adoption.

CUDA `CopyBytes` supports the placement routes accepted by the explicit CUDA
context. It checks byte count, nullability, accessibility, device identity,
and overlap before enqueueing work on that context's stream. An exact
self-copy returns a no-op event, while partial overlap fails. The operation
does not select another device, stage through hidden memory, synchronize the
device, or fall back to the serial implementation.

The returned event retains the provider execution state, not the source or
destination buffers. Until `Query()` reports completion or `Wait()` succeeds,
the context, resources, memory owners, and referenced views must remain alive,
and the referenced bytes must not be released or incompatibly accessed.
Pageable-host CUDA copies are not promised to return without host-side
blocking; the API's asynchronous contract concerns completion represented by
the event and prohibits a hidden device-wide synchronization.

Factories, allocation, copies, event recording, query, and wait report
failures through `Status` or `Result`. Provider failures retain a stable ASC
`ErrorCode`, the provider name, and a signed native code. Diagnostic text and
native codes are not portable branching interfaces. No production exception
API is added.

## Thread safety

Core does not add synchronization to caller-owned state:

- independent immutable values, statuses, extents, schemas, configurations,
  origins, and serial contexts may be read concurrently;
- concurrent access to one object is safe only when every access is const and
  the object is not concurrently moved, assigned, or destroyed;
- a `Buffer`, `File`, `Result<T>`, or `CompletionEvent` must not be moved,
  assigned, closed, waited on, or destroyed concurrently with another access
  to that same object;
- views do not synchronize the referenced bytes; the caller must prevent data
  races, including races through aliased views;
- a byte source or sink determines its own concurrency properties; Core's
  transfer helpers do not serialize calls; and
- separate host allocations and separate serial contexts do not create shared
  execution state; and
- independent CUDA contexts and storage may execute concurrently, while
  callers still serialize mutation, move, reset, destruction, and event access
  for any one ASC object.

These rules describe library object access. They do not make concurrent
mutation of referenced memory safe under the C++ memory model.

## Package and scope boundary

The build target is `asc_core`; the build-tree and installed target is
`ASC::core`. It follows `BUILD_SHARED_LIBS` and exports a strict C++20
requirement with extensions disabled. No warning or sanitizer flags are
propagated to consumers.

Build-tree, installed, relocated, static, shared, and isolated consumers use
the same component name and headers. A component-free
`find_package(ASCCpp 0.9 CONFIG REQUIRED)` request selects the provider-free
`cpp` aggregate. It does not select `core_cuda` or discover CUDAToolkit.

CUDA Core and Dense Core deliberately excludes:

- a provider edge from `ASC::core` or `ASC::cpp`;
- automatic device choice, mutable global policy, provider registries, or a
  default context;
- native CUDA stream/type exposure, stream adoption, peer copies, graph
  capture, prefetch, or a pool allocator;
- implicit transfer, staging, synchronization, allocation, or fallback;
- numerical containers or kernels owned by Dense, Sparse, or Random;
- Sparse CUDA, Random CUDA, HIP, SYCL, or an optimized CPU provider;
- command-line, environment, response-file, or concrete configuration-file
  parsers; and
- compatibility APIs from the deleted implementation.

The [frozen CUDA Core and Dense contract][contract] is authoritative for the CUDA
facet. The [Core contract][core-contract] remains the base Core
authority.

[contract]: ../architecture/decisions/0008-memory-and-execution.md
[core-contract]: ../architecture/dependency-policy.md
