# Milestone 1 contract: core CPU foundation

Status: Frozen after owner approval on 2026-07-26

Branch: `feature/asc-cpp-m1-core`

## Authority and predecessor

This contract is subordinate to the owner's current instructions, the
all-in-one runbook, and the approved Stage A architecture package and ADRs.
It binds released ASCCMake `v0.1.0` at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

The owner advanced directly from the locally validated Milestone 0
Publication Checkpoint B without authorizing its commit or publication.
Therefore this branch carries the complete uncommitted Milestone 0 diff as an
unchanged predecessor layer. Milestone 1 work must not rewrite its approved
architecture, review evidence, or intentional restart deletions except where
this contract explicitly advances a live package, test, CI, or current
documentation file.

## Objective

Implement the provider-free CPU `core` module as the sole available ASCCpp
component. It supplies checked metadata, errors/results, configuration values
and schema validation, byte/text I/O, host memory ownership, and a serial
execution context. Package version is the unreleased `0.1.0` candidate.

## Exact production target and dependency contract

```text
build target:     asc_core
build-tree alias: ASC::core
installed target: ASC::core
direct ASC deps:  none
external deps:    none
```

`asc_core` is a real C++ library following `BUILD_SHARED_LIBS`. It uses only
the C++20 standard library. It must use the released asc-cmake APIs for public
C++20 requirements and target-local warnings/sanitizers. No source or package
edge to `utilities`, `expression`, `dense`, `sparse`, or `random` is allowed.

## Exact public files

```text
include/asc/core.h
include/asc/core/contracts.h
include/asc/core/execution.h
include/asc/core/export.h
include/asc/core/extents.h
include/asc/core/configuration.h
include/asc/core/io.h
include/asc/core/memory.h
include/asc/core/result.h
include/asc/core/status.h
include/asc/core/types.h
```

All public declarations are directly in `namespace asc`. Headers are
self-contained `.h` files with full-path guards and direct includes.
Provider SDK names, headers, handles, and types are prohibited.

## Public semantic surface

### Status, result, and contracts

- `ErrorCode` has stable values for success, invalid argument, shape, index,
  overflow, invalid state, allocation, memory access/transfer, unsupported,
  unavailable, provider, numerical, configuration, I/O, EOF, encoding,
  version, and internal failures.
- `Status` is a nodiscard value containing an `ErrorCode`, diagnostic text,
  optional provider name, and signed native code. Message text is not stable
  ABI and may be redacted.
- `Result<T>` is nodiscard, supports move-only `T`, contains either a value or
  a non-OK `Status`, and never transports a public exception.
- Reading the value of a failed `Result<T>` invokes release-active
  `FatalContract`; destruction and valid moves remain `noexcept` when their
  contained type permits it.
- `ASC_CHECK` is release-active. `ASC_DCHECK` is debug-only. Contract failure
  has no mutable process-global handler.

### Logical metadata and extents

- `index_t`, `extent_t`, `stride_t`, and `nnz_t` are signed 64-bit;
  `rank_t` is unsigned 32-bit; byte counts use `std::size_t`.
- `kDynamicExtent` is the distinct signed value `-1`.
- Checked integral conversion, addition, multiplication, and byte-count
  operations return `Result`, reject negative-to-unsigned and out-of-range
  conversion, and perform no undefined signed overflow.
- `Extents<...>` has compile-time rank and mixed static/dynamic extents.
  Creation validates all dynamic values and the complete logical product
  before publishing an object.
- Rank zero has logical size one. Any zero extent has logical size zero.
  Negative extents and product overflow fail without allocation.

### Configuration model

- `ConfigurationValue` recursively supports exactly null, bool, signed and
  unsigned 64-bit integers, double, UTF-8 string bytes, list, and
  string-keyed object.
- `ConfigurationSchema` is a recursive object schema with exact type,
  required/default/deprecated/sensitive state, nested fields, and applicable
  numeric or size bounds.
- Unknown keys fail. Defaults are validated before insertion. Numeric types
  are not silently converted or truncated.
- `ConfigurationOrigin` records default or explicit programmatic origin plus
  an optional source label and location.
- `ValidateConfiguration` is transactional and returns a `Configuration` only
  after the entire tree succeeds. It records per-path origin and sensitivity.
- Diagnostic rendering of a sensitive value yields a redaction marker.
- No argv, environment, response-file, or concrete local-file parser exists.
  No dense or sparse type is a value alternative.

### I/O

- `ByteSource` and `ByteSink` define partial `ReadSome`/`WriteSome`
  operations. Zero-byte requests succeed without access.
- `ReadExact` distinguishes clean completion from EOF/short input;
  `WriteAll` handles partial writes and rejects zero-progress sinks.
- Move-only `File` owns one native local-file resource and provides explicit
  read, write, flush, and close behavior without throwing.
- Bounded text-file helpers reject size overflow and publish no partial
  destination.
- Fixed-width integral and IEC 60559 float little-endian encode/decode helpers
  never serialize native object layout.
- This milestone does not define dense, sparse, random-state, logging, or
  device-transfer formats.

### Host memory and serial execution

- `MemorySpace` distinguishes host, pinned host, device, and managed spaces.
- `MemoryResource` allocates/deallocates exactly one declared space.
  `HostMemoryResource` supplies aligned host allocation.
- `Buffer` is a move-only byte owner retaining a non-owning resource pointer;
  that resource must outlive the buffer. It releases exactly once. Zero-byte
  allocation succeeds without allocating.
- `ConstMemoryView` and `MutableMemoryView` are non-owning byte descriptors
  carrying address, size, and memory space.
- `Backend`, `Device`, `Determinism`, `ExecutionContext`, and
  `CompletionEvent` are backend-neutral public vocabulary. The only available
  context is immutable serial CPU execution on host memory.
- `CopyBytes` takes an explicit context and views, validates bounds and
  accessibility before mutation, performs overlap-safe host copying, and
  returns an already-complete move-only event.
- Requests for CUDA or another unavailable backend/space return
  `kUnavailable` or `kUnsupported`; there is no auto selection, fallback,
  allocation, transfer, packing, or hidden synchronization.

## Exact compiled production files

```text
src/core/configuration.cc
src/core/contracts.cc
src/core/execution.cc
src/core/io.cc
src/core/memory.cc
src/core/status.cc
```

Templates and small checked operations required by consumers remain in their
owning public headers. No `*_impl.h`, provider source, compatibility source,
or explicit instantiation outside this list is approved.

## Build and package contract

- Minimum CMake 3.25; strict public C++20 with extensions off.
- ASCCMake 0.1.0 is found exactly.
- `ASC_CPP_BUILD_TESTING` and `ASC_CPP_INSTALL` retain Milestone 0 defaults.
- `ASC_CPP_WARNINGS_AS_ERRORS` and four explicit sanitizer options are
  top-level development controls, defaulting off and never exported.
- Standard CMake owns the conditional ASCCpp component export.
- Build and install configs expose only `core`; all future components remain
  known but unavailable.
- `find_package(ASCCpp 0.1 CONFIG REQUIRED COMPONENTS core)` succeeds and
  creates exactly `ASC::core`.
- No-component lookup remains an implicit required `cpp` request and fails.
- Unknown or unavailable required components fail. Optional unavailable
  components report their component `_FOUND` false without invalidating an
  otherwise successful required `core` request.
- Build-tree, installed, relocated, path-with-spaces, static, shared, and
  isolated core consumers are required. No user package-registry write is
  permitted.

## Verification contract

- Compile every public header alone under C++20 and with exceptions disabled
  where supported; compile representative multi-TU use.
- Mechanically audit public/source includes and build/install direct
  dependencies.
- Exercise all error domains, `Result<T>` move-only behavior, fatal misuse,
  checked arithmetic/casts, rank zero, zero/negative extents, and overflow.
- Exercise configuration types, recursive validation, defaults, unknown
  fields, bounds, origin, rollback, and redaction.
- Exercise short/zero-progress I/O, EOF, truncation, endianness, floating bit
  round trips, file move/close, and size limits.
- Exercise alignment, zero bytes, failure injection, move, exactly-once
  release, inaccessible memory, overlap-safe copy, independent serial
  contexts, unavailable backends, and event state.
- Run Debug and Release, warnings-as-errors, ASan/UBSan, package relocation,
  static/shared, subproject, and isolated consumers where locally supported.
- Hosted GCC/Clang/MSVC/AppleClang remains CI evidence, not a local claim.

## Prohibited and deferred

- No other module, facet, provider, numerical container, expression, random
  engine, parser, logging framework, optimized CPU backend, or GPU code.
- No public production exception API, mutable global policy/registry/default
  context, hidden transfer, provider fallback, or provider header.
- No copied or mechanically translated MdeCpp source, test, literal corpus, or
  generated data.
- No unapproved dependency, C++23 feature, C++20 module, `#pragma once`,
  `asc::detail`, or restored compatibility API.
- No release, push, PR, merge, tag, or branch deletion.

## Stop and rollback

Stop at Publication Checkpoint B. Before a commit, rollback removes only the
Milestone 1 additions and reverses its explicit live package/CI/current-doc
changes; the complete Milestone 0 predecessor diff remains. After a separately
approved commit, rollback is a reviewed `git revert`, never reset, force-push,
or broad deletion.
