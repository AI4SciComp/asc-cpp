# Milestone 1 core production self-review

Status: production implementation and self-review complete on 2026-07-26

## Scope and provenance

The implementation is original clean-room C++20 written from the frozen
Milestone 1 contract and accepted ADRs. No MdeCpp source, test structure,
literal corpus, generated data, or optional-provider code was consulted as an
implementation source or copied.

The production write set is exactly the eleven approved public headers and six
approved `.cc` files. The lead owns `src/core/CMakeLists.txt` and all root,
package, test-registration, and CI integration.

## Implemented contracts

- `ErrorCode`, nodiscard `Status`, nodiscard `Result<T>`, release-active
  `ASC_CHECK`, and debug-only `ASC_DCHECK`.
- Signed 64-bit logical metadata, checked integral casts/arithmetic/byte
  counts, and compile-time-rank mixed `Extents`.
- Recursive configuration values, schema validation, defaults, bounds,
  unknown-key rejection, origin/sensitivity metadata, transactional
  publication, JSON Pointer lookup, and diagnostic redaction/escaping.
- Partial synchronous byte sources/sinks, exact read/all-write loops, move-only
  local files, bounded text files, and portable 1/2/4/8-byte integer plus
  IEC 60559 float/double little-endian conversion.
- Explicit memory spaces, abstract one-space resources, aligned host
  allocation, move-only byte buffers, non-owning byte views, and exactly-once
  release.
- Backend/device/determinism vocabulary, immutable serial execution contexts,
  move-only already-complete events, and explicit overlap-safe host copies.

No other module, provider, parser, numerical storage, expression, random,
logging, compatibility, or device behavior is present.

## Ownership, lifetime, and invalid states

- `Result<T>` owns either one value or one non-OK status. It supports move-only
  values. Accessing a failed value reaches `FatalContract` without first
  allocating a rendered diagnostic.
- `ConfigurationValue`, schemas, and validated configurations own their trees.
  Validation copies into private temporary state and returns no configuration
  after any failure.
- `File` owns one native stream. Its destructor performs best-effort close;
  callers must call `Close()` to observe close errors. Move assignment has a
  release-active precondition that the destination is already closed, avoiding
  silent loss of a close failure.
- `Buffer` owns one resource allocation but not the `MemoryResource`; the
  resource must outlive the buffer. Moves clear the source. `Reset()` and
  destruction deallocate a nonzero allocation at most once.
- Memory views never own and cannot prove that an external pointer remains
  alive. The caller keeps viewed storage alive through `CopyBytes`.
- `CompletionEvent` owns only completion state. Moving invalidates the source;
  waiting on that source returns `kInvalidState`.

## Errors and transactional behavior

Recoverable public failures return stable error codes. Status diagnostic text
is deliberately not stable ABI. Provider/native detail is preserved without a
provider enum or SDK type. `Status` construction is body-noexcept because it
moves already-constructed string arguments; argument creation may still fail
under ordinary standard-library allocation rules.

Configuration validation checks type and bounds before publishing, rejects all
unknown object keys, validates defaults before insertion, and applies object
size bounds to the effective post-default object. Numeric alternatives are
never converted. `ConfigurationValue::Utf8String` is the sole string-value
factory and rejects malformed, truncated, overlong, surrogate, and
above-U+10FFFF sequences with `kEncoding`. Object and schema keys are the
contract's opaque `std::string` byte keys, not `kString` values.

Bounded text input, buffer creation, and `CopyBytes` validate their complete
operation before publishing a destination or owner. `ReadExact` may modify the
successfully read destination prefix before a later EOF/error, and `WriteAll`
cannot roll back a prefix already accepted by a sink.
`File::Close()` is genuinely `noexcept`; its failure status carries `kIo` and
the native code without allocating a diagnostic.

## Allocation and execution costs

- Checked arithmetic and `Extents` allocate nothing.
- A `Status` allocates only as required by its supplied strings.
- `ConfigurationValue` scalars allocate nothing; strings, lists, objects,
  schemas, metadata, copying, and validation allocate through standard
  containers. Validation is linear in the traversed tree plus ordered-map
  costs and builds one complete output tree and metadata map.
- `ReadExact` and `WriteAll` allocate nothing themselves. `ReadTextFile`
  allocates at most its bounded returned string; `WriteTextFile` adds no
  content-sized temporary.
- `HostMemoryResource` performs exactly one aligned allocation for a nonzero
  request. `Buffer` and views add no allocation. `CopyBytes` uses synchronous
  `memmove`, performs no allocation, packing, transfer, fallback, or hidden
  synchronization, and returns an already-complete event.

## Concurrency

There is no mutable global handler, registry, context, resource, diagnostic
sink, or provider state. Separate objects may be used concurrently.
`HostMemoryResource` is stateless and delegates to thread-safe allocation
primitives. Immutable `ExecutionContext` instances and const validated
configuration reads may be shared. Concurrent mutation of the same
configuration builder, file, buffer owner, result, or event is not supported.
Callers synchronize overlapping memory operations.

## Portability and provider boundary

Common headers contain only C++20 standard-library types. Windows symbol
visibility distinguishes `ASC_CORE_BUILDING_LIBRARY` and
`ASC_CORE_STATIC_DEFINE`; the lead-owned target supplies those definitions.
Native file opening uses wide paths on Windows and native byte paths on POSIX.
All provider names in `Backend` are vocabulary only: CUDA returns
`kUnavailable`; HIP/SYCL return `kUnsupported`. There is no provider discovery,
SDK header, target, allocation, compilation, or runtime claim. GPU evidence is
therefore `skipped`.

## Production validation performed

The production files passed:

```text
GCC 11.4:
  -std=c++20 -pedantic-errors -Wall -Wextra -Werror -fno-exceptions
  every public header alone and every source independently

Clang 19:
  -std=c++20 -pedantic-errors -Wall -Wextra -Werror -fno-exceptions
  every public header alone and every source independently

CMake 4.1.2 / GCC 11.4:
  Debug static, warnings-as-errors: configured and built
  Release shared, warnings-as-errors: configured and built
```

`clang-format-19` was applied using the checked-in configuration. No local
`clang-tidy` executable was available to this role. Full tests, sanitizers,
package relocation, and consumer validation remain lead/verification evidence,
not claims of this self-review.

## Remaining review risks

- MSVC and AppleClang compilation require hosted evidence.
- File-destructor close errors are necessarily unobservable; callers requiring
  durability must call `Flush()` and `Close()`.
- Opaque object/schema key bytes are deliberately distinct from validated
  UTF-8 `kString` values and must be rendered through escaped diagnostics.
- Public non-owning views and resource pointers rely on documented caller
  lifetime; sanitizers and failure resources must continue to falsify those
  boundaries.
- No sanitizer or performance conclusion should be accepted until the lead's
  fresh integrated validation and independent review complete.
