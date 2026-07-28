# Milestone 1 core production self-review

Status: production implementation and self-review complete on 2026-07-27

## Scope and provenance

The implementation is clean-room C++20 derived from the frozen Milestone 1
contract and approved ADRs 0004 through 0009. The preserved cumulative commit
was inspected as evidence, then reconciled against the narrower frozen
contract. No MdeCpp source, test structure, literal corpus, generated data, or
provider implementation was used.

The production write set is exactly the eleven approved public headers, six
approved `.cc` files, and this review. No private header, CMake file, test,
provider, later module, compatibility file, or unrelated predecessor evidence
was added or changed.

## Implemented surface

- Stable `ErrorCode`, nodiscard `Status`, move-aware nodiscard `Result<T>`,
  release-active `ASC_CHECK`, and debug-only `ASC_DCHECK`.
- Signed 64-bit logical metadata, checked casts/arithmetic/byte counts, and
  compile-time-rank mixed `Extents`.
- Recursive configuration values, object schemas, defaults, exact types,
  numeric and size bounds, unknown-key rejection, transactional validation,
  per-path provenance, sensitivity/deprecation metadata, JSON Pointer lookup,
  UTF-8 string validation, and diagnostic redaction.
- Partial synchronous byte sources and sinks, exact/all loops, move-only local
  files, bounded text files, and portable little-endian scalar encoding.
- Explicit memory spaces, abstract one-space resources, aligned host
  allocation, move-only buffers, const/mutable non-owning byte views, and
  exactly-once release.
- Backend/device/determinism vocabulary, immutable serial execution contexts,
  move-only already-complete events, and explicit overlap-safe host copies.

Core uses only the C++20 standard library and depends on no ASC target.

## Contract reconciliation

Two later-surface artifacts in the preserved cumulative implementation were
not imported:

- configuration origin exposes only `kDefault` and `kProgrammatic`; command
  line origin belongs to a later utilities parsing boundary; and
- the provider-state abstract classes are defined privately in
  `execution.cc`, avoiding an unapproved seventh/private production file.

One preserved correctness defect was fixed. Exact JSON Pointer origin
overrides beneath list values were previously accepted by
`ValidateConfigurationWithOrigins` but silently inherited the list origin.
Recursive metadata recording now applies exact origins at list elements and
their descendants before the final unused-path audit.

The public `File` header now states its move-assignment precondition: the
destination must already be closed so an earlier close failure cannot be
silently discarded.

## Ownership, lifetime, and invalid states

- `Result<T>` owns either one value or one non-OK status and supports move-only
  values. Failed value access invokes `FatalContract`.
- Configuration values, schemas, and validated configurations own their
  complete trees. Validation builds private output and metadata state and
  publishes neither after a failure.
- `File` owns one native stream. Its destructor performs best-effort close;
  callers use `Flush()` and `Close()` to observe errors. Move assignment has a
  documented release-active closed-destination precondition.
- `Buffer` owns one allocation but not its `MemoryResource`. The resource must
  outlive the buffer. Moves invalidate the source, and reset/destruction
  releases a nonzero allocation at most once.
- Memory views are non-owning. Callers keep their storage alive through all
  operations.
- `ExecutionContext` is immutable and copyable. `CompletionEvent` is
  move-only; querying or waiting on a moved-from event returns
  `kInvalidState`.

## Error and transaction behavior

Recoverable failures return stable error codes before publishing an owner,
configuration, or copy event. Status text is diagnostic rather than stable
ABI. Provider/native detail does not expose a provider SDK type.

Configuration validation checks exact types and bounds, rejects unknown object
keys, validates defaults before inserting them, and applies object size bounds
to the effective post-default object. Numeric alternatives are never
converted. Sensitive rendering always returns `<redacted>`.

`ReadExact` and `WriteAll` necessarily may expose a prefix successfully
accepted by their external source or sink before a later error. Bounded text
input publishes no partial returned string. Buffer allocation validates
alignment, null results, and zero-byte behavior before returning an owner.
`CopyBytes` validates bounds, views, and accessibility before mutation.

## Allocation and execution costs

- Checked arithmetic and `Extents` allocate nothing.
- `Status` allocates only through supplied diagnostic strings.
- Configuration scalars allocate nothing; strings, lists, objects, schemas,
  copying, and metadata use standard containers.
- Configuration validation is linear in the traversed value tree plus ordered
  map costs and constructs one complete output tree and metadata map.
- `ReadExact` and `WriteAll` allocate nothing themselves. Bounded text input
  allocates only its result string.
- Nonzero host allocation performs one aligned allocation. Buffer and views
  add no allocation.
- Serial `CopyBytes` uses synchronous `memmove`, allocates no storage, performs
  no transfer or fallback, and returns an already-complete event.

## Concurrency

There is no mutable global handler, registry, default context, resource,
diagnostic sink, or provider state. Separate objects may be used concurrently.
`HostMemoryResource` is stateless. Immutable contexts and const validated
configuration reads may be shared.

Concurrent mutation of the same schema, file, buffer owner, result, event, or
destination memory is not supported. Callers synchronize shared and
overlapping memory operations.

## Portability and provider boundary

Public headers contain only C++20 standard-library types. Windows visibility
distinguishes shared-library build/use and static linkage. Native file opening
uses wide paths on Windows and native byte paths on POSIX.

Backend enumerators are vocabulary only. Serial ordinal zero is the only
available execution device. CUDA returns `kUnavailable`; HIP and SYCL return
`kUnsupported`. There is no provider discovery, SDK header, handle, target,
allocation, compilation, or runtime claim. GPU evidence is `skipped`.

## Production validation

All eleven public headers compiled alone with each compiler:

```sh
printf '#include "%s"\nint main() { return 0; }\n' "$include_name" |
  g++ -x c++ -std=c++20 -pedantic-errors -Wall -Wextra -Werror \
    -fno-exceptions -Iinclude -fsyntax-only -

printf '#include "%s"\nint main() { return 0; }\n' "$include_name" |
  clang++-19 -x c++ -std=c++20 -pedantic-errors -Wall -Wextra -Werror \
    -fno-exceptions -Iinclude -fsyntax-only -
```

All six sources compiled independently:

```sh
g++ -std=c++20 -pedantic-errors -Wall -Wextra -Werror \
  -fno-exceptions -fPIC -Iinclude -c "$source_file" -o "$object_file"

clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra -Werror \
  -fno-exceptions -fPIC -Iinclude -c "$source_file" -o "$object_file"
```

Integrated target-only builds used:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m1-cmake-production.vObEY4/debug-static \
  -DASCCMake_DIR=/tmp/asc-cpp-m1-cmake-production.vObEY4/asc-cmake \
  -DBUILD_TESTING=OFF -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build \
  /tmp/asc-cpp-m1-cmake-production.vObEY4/debug-static --parallel 2

cmake -S . \
  -B /tmp/asc-cpp-m1-cmake-production.vObEY4/release-shared \
  -DASCCMake_DIR=/tmp/asc-cpp-m1-cmake-production.vObEY4/asc-cmake \
  -DBUILD_TESTING=OFF -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=OFF -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build \
  /tmp/asc-cpp-m1-cmake-production.vObEY4/release-shared --parallel 2
```

Results:

- GCC 11.4: 11/11 headers and 6/6 sources passed;
- Clang 19.0.0: 11/11 headers and 6/6 sources passed;
- CMake 4.1.2/GCC Debug static, warnings-as-errors: passed;
- CMake 4.1.2/GCC Release shared, warnings-as-errors: passed;
- checked-in clang-format 19 dry run: passed; and
- a temporary linked GCC runtime smoke: passed.

The runtime smoke exercised checked mixed extents, a move-only result, an exact
list-element origin override, host buffer allocation, overlapping host copy,
completion-event state, and signed little-endian round trip.

The production scope also passed exact file inventory, header-guard,
extension, provider-include, namespace, trailing-whitespace, and 80-column
checks. No local clang-tidy executable is available.

## Remaining risks

- Full independent functional, failure, death, sanitizer, package,
  relocation, static/shared, subproject, and isolated-consumer tests remain
  verification/integration evidence rather than claims of this review.
- MSVC and AppleClang compilation require hosted evidence.
- Native file close failures are unobservable from destruction; callers
  requiring durability must call `Flush()` and `Close()`.
- Non-owning views and resource pointers rely on caller lifetime discipline.
- A future separately approved provider milestone must design its private
  state access without adding provider behavior to this bounded M1 surface.
