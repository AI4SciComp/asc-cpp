# Core module

`ASC::core` is the dependency-free base of the six-module ASCCpp package.
The optional `ASC::core_cuda` facet adds one CUDA Runtime implementation in
the unreleased `0.9.0` candidate without changing the provider-free base.

Core owns storage-independent foundations. It does not contain a command-line
or configuration-file parser, numerical container, expression, algebra,
random engine, or dense/sparse operation. CUDA implementation remains isolated
in the core-owned provider facet.

## Build and package contract

```text
build target:     asc_core
build-tree alias: ASC::core
installed target: ASC::core
direct ASC deps:  none
external deps:    none

build target:     asc_core_cuda
build-tree alias: ASC::core_cuda
installed target: ASC::core_cuda
direct ASC deps:  ASC::core
private provider: CUDA::cudart
```

Both compiled targets follow `BUILD_SHARED_LIBS`. They publish `cxx_std_20`,
require standard C++20, and disable compiler extensions. ASCCpp uses only the
verified released ASCCMake `v0.1.0` target helpers.

CUDA is opt-in:

```bash
cmake -S /absolute/path/to/asc-cpp \
  -B "/absolute/path/to/asc-cpp build" \
  -DASCCMake_DIR=/absolute/path/to/asc-cmake-build \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASC_CPP_INSTALL=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="/absolute/path/to/asc-cpp prefix"
cmake --build "/absolute/path/to/asc-cpp build"
cmake --install "/absolute/path/to/asc-cpp build"
```

`ASC_CPP_ENABLE_CUDA` defaults to `OFF`. In that mode ASCCpp neither enables
the CUDA language nor discovers `CUDAToolkit`, creates a provider target, or
adds a CUDA runtime requirement. When enabled, configuration requires CUDA 12
or newer, a working CUDA compiler, `CUDA::cudart`, and `CUDA::cublas`; the
dense provider is built with the core provider. The build honors
`CMAKE_CUDA_ARCHITECTURES` and does not infer an architecture from visible
hardware.

An installed provider-free consumer requests `core` explicitly:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_consumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core)

add_executable(my_consumer main.cc)
target_link_libraries(my_consumer PRIVATE ASC::core)
```

An installed CUDA consumer instead requests the facet:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core_cuda)
target_link_libraries(my_consumer PRIVATE ASC::core_cuda)
```

That closure imports exactly `ASC::core` and `ASC::core_cuda` and then requires
the consumer's separately supplied CUDAToolkit. A provider-free component
request, including a no-component request for provider-free `ASC::cpp`, does
not discover CUDA even when the producer installation contains CUDA facets.
An installation built with CUDA disabled reports an optional `core_cuda`
component unavailable and rejects a required request.

## Minimal host-copy example

This example allocates four host bytes, copies an explicit host view, and
waits on the already-complete serial event:

```cpp
#include <asc/core.h>

#include <array>
#include <cstddef>

int main() {
  asc::HostMemoryResource resource;
  auto destination = asc::Buffer::Allocate(resource, 4);
  if (!destination.ok()) {
    return 1;
  }

  auto destination_view = destination->mutable_view();
  if (!destination_view.ok()) {
    return 1;
  }

  const std::array<std::byte, 4> source = {std::byte{1}, std::byte{2},
                                           std::byte{3}, std::byte{4}};
  const asc::ConstMemoryView source_view(source.data(), source.size(),
                                         asc::MemorySpace::kHost);

  auto event = asc::CopyBytes(asc::ExecutionContext::Serial(),
                              *destination_view, source_view);
  if (!event.ok()) {
    return 1;
  }

  const asc::Status completion = event->Wait();
  return completion.ok() ? 0 : 1;
}
```

`resource` is declared before `destination`, so it outlives the buffer
deallocation. The requested byte transfer is explicit; it performs no
allocation or packing and has no hidden synchronization.

## Explicit CUDA copy example

This example uses separately owned pinned-host and device resources. It links
`ASC::core_cuda`; no CUDA SDK header or native handle appears in the source.

```cpp
#include <asc/core/providers/cuda.h>

#include <array>
#include <cstddef>

int main() {
  auto count = asc::CudaDeviceCount();
  if (!count.ok() || *count == 0) {
    return 1;
  }

  const asc::Device device = {.backend = asc::Backend::kCuda, .ordinal = 0};
  auto pinned =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kPinnedHost);
  auto device_resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  auto context = asc::CreateCudaExecutionContext(device);
  if (!pinned.ok() || !device_resource.ok() || !context.ok()) {
    return 1;
  }

  auto host_source = asc::Buffer::Allocate(**pinned, 4);
  auto device_buffer = asc::Buffer::Allocate(**device_resource, 4);
  auto host_result = asc::Buffer::Allocate(**pinned, 4);
  if (!host_source.ok() || !device_buffer.ok() || !host_result.ok()) {
    return 1;
  }

  const std::array<std::byte, 4> expected = {std::byte{1}, std::byte{2},
                                             std::byte{3}, std::byte{4}};
  auto* source_bytes = static_cast<std::byte*>(host_source->data());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    source_bytes[index] = expected[index];
  }

  auto source_view = host_source->const_view();
  auto device_view = device_buffer->mutable_view();
  if (!source_view.ok() || !device_view.ok()) {
    return 1;
  }
  auto uploaded = asc::CopyBytes(*context, *device_view, *source_view);
  if (!uploaded.ok() || !uploaded->Wait().ok()) {
    return 1;
  }

  auto device_source = device_buffer->const_view();
  auto result_view = host_result->mutable_view();
  if (!device_source.ok() || !result_view.ok()) {
    return 1;
  }
  auto downloaded = asc::CopyBytes(*context, *result_view, *device_source);
  if (!downloaded.ok() || !downloaded->Wait().ok()) {
    return 1;
  }

  const auto* result_bytes = static_cast<const std::byte*>(host_result->data());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    if (result_bytes[index] != expected[index]) {
      return 1;
    }
  }
  return 0;
}
```

Both copy events are waited before any referenced buffer or resource is
destroyed. A production pipeline may query an event and overlap independent
work instead. Calling `Query()` returning `false` is normal, not an error.

## Public headers

Use `<asc/core.h>` for the provider-neutral surface. Provider headers are
deliberately not included by that umbrella. Every topic and provider header is
self-contained and may be included directly:

| Header | Contract |
| --- | --- |
| `<asc/core/types.h>` | fixed-width logical metadata and checked integral arithmetic |
| `<asc/core/extents.h>` | compile-time-rank mixed static/dynamic extents |
| `<asc/core/status.h>` | error codes and status values |
| `<asc/core/result.h>` | value-or-status transport |
| `<asc/core/contracts.h>` | release-active and debug-only fatal contracts |
| `<asc/core/configuration.h>` | recursive configuration values, schemas, validation, origin, and redaction |
| `<asc/core/io.h>` | partial/exact byte I/O, local-file ownership, bounded text, and portable little-endian encoding |
| `<asc/core/memory.h>` | memory spaces, resources, byte buffers, and byte views |
| `<asc/core/execution.h>` | backend/device vocabulary, serial/CUDA-capable opaque contexts, completion events, and explicit byte copy |
| `<asc/core/export.h>` | shared-library symbol visibility |
| `<asc/core/providers/cuda.h>` | CUDA device query, CUDA memory resource, CUDA execution-context factory, and event recording |
| `<asc/core/providers/cuda_export.h>` | core CUDA facet symbol visibility |

All supported declarations are directly in `namespace asc`. Public headers
contain no CUDA, HIP, SYCL, BLAS/LAPACK, Eigen, MKL, OpenMP, or other provider
SDK header, handle, or type. The provider-specific header names CUDA as the
selected capability but still exposes no CUDA SDK declaration or native
handle.

## Status, result, and contracts

`ErrorCode` gives stable categories for:

- success;
- invalid arguments, shape, and index;
- overflow and invalid state;
- allocation, memory access, and memory transfer;
- unsupported and unavailable capabilities;
- provider and numerical failures;
- configuration, I/O, EOF, encoding, and version failures; and
- internal failures.

`Status` is a `[[nodiscard]]` value containing an `ErrorCode`, diagnostic
message, optional provider name, and signed native provider code. Error-code
values are stable; message and provider text are diagnostics, not stable ABI.
Diagnostics may be redacted. `ErrorCodeName` returns a readable category name;
`Status::ToString` combines the available diagnostic fields for display.

`Result<T>` is `[[nodiscard]]` and contains either a value or a non-OK
`Status`. It supports move-only values. A caller must test the result before
accessing its value. Accessing the value of a failed result invokes the
release-active fatal contract. The transport does not expose a public
production exception API.

`ASC_CHECK` is release-active and `ASC_DCHECK` is debug-only. Each evaluates
its condition once. A failure invokes `FatalContract`; there is no mutable
process-global handler. Recoverable inputs and operational failures use
`Status` or `Result<T>` rather than a contract.

## Logical metadata and extents

The fixed logical types are:

| Type | Representation | Purpose |
| --- | --- | --- |
| `index_t` | signed 64-bit | logical index and offset |
| `extent_t` | signed 64-bit | logical extent |
| `stride_t` | signed 64-bit | logical stride |
| `nnz_t` | signed 64-bit | sparse nonzero count |
| `rank_t` | unsigned 32-bit | runtime rank |
| `std::size_t` | implementation-defined unsigned | byte count |

`kDynamicExtent` is the distinct signed value `-1`. Checked conversion,
addition, multiplication, and byte-count operations return `Result` and do
not execute undefined signed overflow. Negative-to-unsigned conversion and
out-of-range conversion fail. The exact operations are `CheckedCast`,
`CheckedAdd`, `CheckedMultiply`, and `CheckedByteCount`; their integral
constraint is the `CheckedInteger` concept, which excludes `bool`.

`Extents<...>` has compile-time rank and may mix static and dynamic dimensions.
Creation validates each dynamic value and the complete logical product before
publishing an object. Negative extents and product overflow fail without
allocation. Rank zero has logical size one; any zero extent makes the logical
size zero.

Extents are metadata only. They do not own storage, select a layout, allocate,
or choose an execution backend. The span returned by `values()` borrows the
`Extents` object and must not outlive it.

## Configuration

`ConfigurationValue` recursively supports exactly:

```text
null, bool, signed 64-bit integer, unsigned 64-bit integer, double,
UTF-8 string bytes, list, string-keyed object
```

It contains no dense or sparse value alternative.

`AsString`, `AsList`, and `AsObject` return `Result` values containing
`std::reference_wrapper<const ...>`. The reference is non-owning and is
invalidated when the originating `ConfigurationValue` is assigned, moved, or
destroyed. The same lifetime rule applies to values returned by
`Configuration::Find`. Copy a value when it must outlive its owner.

String values are valid UTF-8 by construction. Create one with the fallible
factory:

```cpp
#include <asc/core/configuration.h>

int main() {
  auto label = asc::ConfigurationValue::Utf8String("simulation");
  if (!label.ok()) return 1;
  return label->type() == asc::ConfigurationValueType::kString ? 0 : 1;
}
```

Malformed UTF-8 returns `ErrorCode::kEncoding`. There is no direct
`std::string` or C-string constructor that bypasses validation. Object keys
are owned `std::string` values; the UTF-8 guarantee in this milestone applies
to `ConfigurationValueType::kString` values. Object and schema keys are opaque
byte strings and are not UTF-8-validated.

`ConfigurationSchema` is a recursive object schema. A field declares an exact
type and may declare required, default, deprecated, or sensitive state,
nested fields, and applicable numeric or size bounds.

Schema construction and mutation use `ConfigurationSchema`,
`SetRequired`, `SetDeprecated`, `SetSensitive`, `SetDefault`,
`SetSignedBounds`, `SetUnsignedBounds`, `SetDoubleBounds`, `SetSizeBounds`,
and `AddField`. Mutators that can reject their arguments return `Status`.

Validation behavior is strict and transactional:

- unknown keys fail;
- numeric types are not silently converted or truncated;
- defaults are validated before insertion;
- the complete tree is validated before a `Configuration` is returned;
- a failed validation publishes no partially validated destination;
- every effective path records default, explicit programmatic, or
  command-line origin;
- optional source labels and locations remain diagnostics; and
- sensitive values render as a redaction marker.

`Configuration::Find`, `Origin`, `IsSensitive`, and `IsDeprecated` use JSON
Pointer paths. The empty path names the root, object tokens escape `~` as
`~0` and `/` as `~1`, and list tokens are decimal indices. The escaping makes
opaque object-key bytes addressable without adding a UTF-8 key invariant.
Diagnostic rendering separately quotes paths and escapes quote, backslash,
control, NUL, and non-ASCII key bytes, so opaque keys cannot inject diagnostic
lines or truncate C-style displays.

`ConfigurationOrigin` records origin kind, optional source label, and optional
location. `ConfigurationMetadata` is the corresponding origin/sensitive/
deprecated record. `ValidateConfiguration` applies one explicit origin to an
input tree. `ConfigurationOrigins` and `ValidateConfigurationWithOrigins`
apply exact JSON Pointer origins transactionally; an exact entry overrides the
inherited origin for that value and descendants, schema defaults remain
`kDefault`, and an unknown origin path fails. `RenderConfigurationValue`
renders one value or returns the redaction marker when its `sensitive`
argument is true.

Core still parses no argv, environment variable, response file, or concrete
local-file syntax. Milestone 2 `ASC::utilities` owns the command-line parser
and uses the core origin API; other parsing and precedence waves remain
separately gated.

## Byte and text I/O

`ByteSource` and `ByteSink` are polymorphic I/O contracts. Operations receive
them by non-owning reference. `ReadSome` and `WriteSome` may make partial
progress. Zero-byte requests succeed without touching the supplied address.

The higher-level operations provide transactional progress rules:

- `ReadExact` distinguishes success from clean EOF and short input; an error
  may leave a prefix of the caller's destination modified;
- `WriteAll` repeats partial writes and rejects a sink that reports successful
  zero progress; an error cannot roll back bytes already accepted by a sink;
- bounded text-file reading rejects size or count overflow and publishes no
  partial destination; and
- fixed-width integral and IEC 60559 floating-point helpers encode and decode
  little-endian bytes without serializing native object layout.

The portable-scalar constraint is `LittleEndianScalar`; the operations are
`EncodeLittleEndian` and `DecodeLittleEndian`. A source or destination span
whose size differs from `sizeof(T)` returns `kEncoding`.

`File` is a move-only owner of one native local-file resource. Open, read,
write, flush, and close failures are explicit status values. Moving transfers
the handle and leaves the source without ownership. Closing is explicit and
destruction does not throw. Move-assignment requires the destination to be
closed; assigning over an open handle is a fatal programmer-contract
violation, preventing an unreported close error.

`Close()` is idempotent and `noexcept`. It consumes the native handle even
when the underlying close reports an error, because the handle state is then
not safely retryable. That failure is `kIo` with the signed native error code;
callers that need close-error evidence must inspect the returned status.

Opening for writing truncates an existing file. `WriteTextFile` is therefore
not an atomic-replacement operation: a write, flush, or close error may leave a
partial file. Callers needing transactional replacement must implement their
own temporary-file and rename policy.

The text helpers preserve bytes. They do not validate UTF-8, normalize line
endings, or add a terminator; configuration string validation is a separate
contract.

Core does not define dense, sparse, random-state, logging, or device-transfer
formats. Numerical modules own their format metadata and schemas.

## Core CUDA provider API

Include `<asc/core/providers/cuda.h>` and link `ASC::core_cuda` for:

```text
CudaDeviceCount() -> Result<std::int32_t>

CudaMemoryResource::Create(device, memory_space)
    -> Result<std::unique_ptr<CudaMemoryResource>>

CreateCudaExecutionContext(
    device, determinism = Determinism::kDeterministic)
    -> Result<ExecutionContext>

RecordCudaEvent(context) -> Result<CompletionEvent>
```

No function accepts or returns a CUDA SDK handle. The provider factory pattern
keeps device validation and allocation/provider failure out of constructors.
The signed native provider code and provider name are preserved in a failed
`Status`; message text remains diagnostic rather than stable API.

`CudaDeviceCount` queries the active CUDA Runtime installation and may
initialize runtime state. A successful zero count is distinct from provider
failure. It does not select a default device.

`CreateCudaExecutionContext` creates one owned nonblocking stream for the
explicit device and requested determinism mode. Copies of the returned context
share immutable provider execution state. `RecordCudaEvent` records all work
already submitted to that context's stream; later submissions are not added to
the recorded event.

## Host memory

`MemorySpace` distinguishes host, pinned host, device, and managed memory.
Those enum values remain provider-neutral vocabulary. Availability depends on
the selected resource:

| Space | Project resource | Host dereference contract |
| --- | ---: | ---: |
| host | `HostMemoryResource` | yes |
| pinned host | CUDA-created `CudaMemoryResource` | underlying allocation is host accessible; an operation still follows its public view-access contract |
| device | CUDA-created `CudaMemoryResource` | no |
| managed | CUDA-created `CudaMemoryResource` | no implicit host migration is authorized |

`MemoryResource` allocates and deallocates bytes in exactly one declared
space. `HostMemoryResource` implements aligned host allocation. A zero-byte
allocation succeeds without allocating. A zero or non-power-of-two alignment,
and allocation failure, return status before publishing an owner.

`CudaMemoryResource::Create` accepts an explicit CUDA `Device` and exactly
`kPinnedHost`, `kDevice`, or `kManaged`. It rejects ordinary `kHost`, invalid
devices, and unavailable capabilities. The resource is noncopyable and
nonmovable so its address remains stable for every buffer that borrows it.
It must outlive all such buffers.

CUDA allocations use the matching runtime allocation/free family. A zero-byte
request returns null without invoking a provider allocator. A provider failure
uses a stable ASC error category plus provider name and signed native code.
Device and managed deallocation uses the CUDA Runtime's ordinary free
operation, which may synchronize according to the provider contract; ASCCpp
does not represent it as stream-ordered deallocation. Every access using an
allocation must therefore complete before its buffer is reset or destroyed.

`MemorySpaceName` renders the enum spelling. `ConstMemoryView` and
`MutableMemoryView` expose `data`, `size`, `space`, and `valid` accessors; a
mutable view converts to a const view.

`Buffer` is a move-only byte owner:

- it retains a non-owning pointer to the resource that created its allocation;
- that resource must outlive the buffer;
- a move transfers ownership and leaves the source empty;
- destruction releases a nonempty allocation exactly once;
- empty destruction performs no deallocation; and
- there is no implicit copy, resize, mirror, device fallback, or typed object
  construction.

A successful zero-byte allocation is a valid buffer with a null address and
no resource allocation. `Reset()` is idempotent and leaves a buffer invalid;
queries that require its resource then return `kInvalidState`.

`data()` exposes the allocation address but does not make a non-host space
host-dereferenceable. Check `space()` or use a view with an execution context
before access. `HostMemoryResource` is always available; CUDA resources exist
only through the optional provider facet. Custom resources remain responsible
for truthfully implementing their declared `MemorySpace`.

`ConstMemoryView` and `MutableMemoryView` are non-owning byte descriptors. They
carry an address, byte size, and `MemorySpace`; they do not extend storage
lifetime. A mutable view converts to a const view, never the reverse. The
storage owner and resource must outlive every view use and operation that
refers to it, including asynchronous work represented by an incomplete event.

## Serial and CUDA execution

`Backend`, `Device`, `Determinism`, `ExecutionContext`, and
`CompletionEvent` are provider-neutral public vocabulary. Common headers carry
no provider SDK state in a public signature.

`BackendName` renders the backend spelling. `Device::Serial` and
`ExecutionContext::Serial` construct the always-available path;
`ExecutionContext::Create` validates an explicit backend, device, and
determinism request but does not discover or instantiate a provider.
`CreateCudaExecutionContext` is the only M6 CUDA factory. It validates an
explicit CUDA device and creates one owned nonblocking stream; there is no
default device, global current ASC context, provider registry, native-stream
adoption, or fallback.

Execution contexts are copyable immutable handles. Each CUDA completion event
retains the provider execution state needed to keep its stream alive. A
provider call restores CUDA current-device state that it temporarily changes.
`CanAccess` reports whether the explicit context can use a memory space.

`CompletionEvent` is move-only. `Query()` returns whether the recorded work
has completed without waiting; `Wait()` waits only for that event and is
idempotent after successful completion. Both reject a moved-from event.
Destroying an event does not synchronize the device. The event retains
provider event/stream state, not the caller's arrays or views.

`CopyBytes` requires an explicit context and source/destination byte views. It
validates range, nullability, placement, device access, and overlap before
accessing either pointer. Serial host copy is overlap-safe and returns an
already-complete event. CUDA copy uses the context's stream and returns a
recorded completion event. An exact self-copy is a no-op event; partial CUDA
overlap is rejected. There is no device-wide synchronization, transfer
fallback, packing, or hidden context selection.

The provider creates completion state before enqueue. If recording that event
fails after a successful enqueue, the failure path waits only the affected
stream before returning the provider error; this prevents untracked live work
from escaping without a completion handle. Ordinary success performs no such
wait.

The returned event defines operation completion, not a universal guarantee
that the submitting host call cannot block. In particular, CUDA Runtime copy
synchronization behavior depends on the memory kinds and may stage pageable
host memory. Use pinned host storage when host-asynchronous transfer behavior
is required, and keep both storage owners, their memory resources, and the
context alive until the event completes.

`RecordCudaEvent` records the CUDA context's current stream position without
retaining user storage. Requests for an unavailable provider return
`kUnavailable`; an unsupported backend or operation returns `kUnsupported`.
There is no `AUTO` backend.

## Ownership, failure, cost, and concurrency summary

| Surface | Ownership and lifetime | Failure and mutation | Cost and synchronization | Thread safety |
| --- | --- | --- | --- | --- |
| `Status` / `Result<T>` | values own their diagnostic/value state | failed-result value access is fatal | message/value construction may allocate | separate values are independent; concurrent mutation of one value is unsupported |
| `Extents<...>` | value type | invalid metadata fails before publication | rank-linear validation; no storage allocation | immutable values may be shared |
| Configuration values/schema | values recursively own their data | validation is transactional | tree-linear traversal plus owned string/container allocation | concurrent const access is allowed; callers serialize mutation |
| `File` | move-only native resource owner | I/O returns explicit status; partial progress is not rolled back | system-call and byte-linear work; synchronous | separate files are independent; same handle requires caller serialization |
| `MemoryResource` | implementation lifetime is external to `Buffer` | allocation fails before owner publication | allocation/deallocation only; CUDA ordinary free may synchronize | separate resources may be used concurrently; callers serialize unsafe shared provider state |
| `Buffer` | move-only bytes; non-owning resource pointer | move is destructive to source; release occurs once | allocation/deallocation only | distinct buffers are independent; shared bytes require caller synchronization |
| Memory views | non-owning; storage must outlive use | invalid/inaccessible ranges fail before copy mutation | descriptor construction is constant-time | follows the referenced storage |
| Context/event | copyable context owns immutable serial or CUDA stream state; move-only event owns completion state | unavailable/unsupported requests return status | serial is synchronous; CUDA query is nonblocking and wait is event-local | independent contexts may be submitted concurrently; caller synchronizes shared storage |
| `CopyBytes` | event retains provider execution state, not views; storage/resources outlive completion | validates before pointer access or enqueue | linear in bytes; no allocation/packing/fallback; pageable-host CUDA staging may block submission | concurrent overlap is a caller data race |

The table states library-level behavior; it does not make unsynchronized
access to shared mutable standard-library state or byte storage safe.

## Provider, sanitizer, and performance limits

The provider-neutral `ASC::core` target has no provider edge or discovery.
`ASC::core_cuda` is the only M6 core provider facet and privately uses the CUDA
Runtime. Requesting the base component does not import or discover that
provider.

GPU evidence is reported separately as **configure-tested**,
**compile-tested**, **runtime-tested**, **parity-tested**, or **skipped**.
Toolkit or device inventory, successful configuration, and a host-only header
parse are not runtime or parity evidence. The M6 Publication Checkpoint B is
authoritative for exact compiler, toolkit, driver, GPU, architecture, transfer
coverage, and skipped cases.

Core is correctness infrastructure rather than a numerical provider. Relevant
costs are documented above: validation is proportional to inspected metadata
or tree content, exact I/O and copy are linear in bytes, and allocation is
delegated to the selected resource. Project-owned CUDA transfer benchmarks may
separate allocation and transfer, warm up the provider, preserve checksums, and
record the environment; smoke timings are not a general performance guarantee
or an unstable CI gate.

Sanitizer, package, relocation, and isolated-consumer evidence belongs to the
Publication Checkpoint B report. Enabling a sanitizer option requests
target-local instrumentation from released ASCCMake; the option name alone is
not evidence that a sanitizer runtime test passed.

## Scope and provenance

The cumulative public surface follows the frozen
[Milestone 6 contract](../development/asc-cpp-m6-gpu-core-dense/milestone-contract.md)
and the approved error, configuration, I/O, metadata, memory/execution,
CPU/GPU, packaging, and ownership ADRs. It contains no copied or mechanically
translated MdeCpp source, tests, literal corpus, or generated data. MdeCpp
remains behavior and test-category evidence only under
[ADR 0017](../development/asc-cpp-architecture/decisions/0017-third-party-provenance.md).
The CUDA implementation is project-owned; CUDA Runtime is an optional,
separately supplied provider library rather than vendored or redistributed
project material.
