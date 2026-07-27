# Milestone 1 documentation and API review

Status: complete with no open blocking findings on 2026-07-26

## Scope and authority

This review covers the frozen **Milestone 1: core CPU foundation** contract,
the complete public `ASC::core` header surface, its six compiled sources, the
lead-owned target/package contract, and the current user documentation.

The review applied:

- the owner-approved Stage A architecture package and all 18 accepted ADRs;
- released ASCCMake `v0.1.0` at
  `8a7dcbad3a97267cce59810aff24de800a3497a7`;
- C++20, the flat `asc` namespace, self-contained `.h` headers, and `.cc`
  compiled sources;
- the current
  [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
  consulted on 2026-07-26; and
- the clean-room MdeCpp provenance boundary in ADR 0017.

The review did not authorize or inspect a later module as implemented. It made
no production, test, CMake, package, CI, or provider edit.

## Reviewed public and package surface

```text
package candidate:  ASCCpp 0.1.0
build target:       asc_core
build-tree target:  ASC::core
installed target:   ASC::core
direct ASC deps:    none
external deps:      none
language contract:  C++20, extensions off
available component: core only
```

The reviewed public headers are:

```text
include/asc/core.h
include/asc/core/configuration.h
include/asc/core/contracts.h
include/asc/core/execution.h
include/asc/core/export.h
include/asc/core/extents.h
include/asc/core/io.h
include/asc/core/memory.h
include/asc/core/result.h
include/asc/core/status.h
include/asc/core/types.h
```

All supported public declarations are directly in `namespace asc`. Internal
implementation namespaces include `internal` in their names. The surface has
no C++23 construct, C++ module, `#pragma once`, public exception transport,
provider header, provider handle, numerical storage, expression, algebra,
random, parser, or compatibility API.

## API decisions confirmed

- `ErrorCode` values are stable machine categories; diagnostic text is not
  stable ABI. `Status` and `Result<T>` are nodiscard value transports.
  Failed-result value access and release-active checks terminate through
  `FatalContract`.
- Logical indices, extents, strides, and nonzero counts are signed 64-bit;
  runtime rank is unsigned 32-bit; bytes use `std::size_t`. Checked operations
  reject narrowing, negative-to-unsigned conversion, and overflow.
- `Extents` is allocation-free metadata with rank-zero size one, zero-extent
  size zero, and validation before publication.
- A configuration string value is valid UTF-8 and can be created only through
  the fallible `ConfigurationValue::Utf8String` factory. Object and schema
  keys remain opaque owned byte strings as required by the frozen
  string-keyed-object contract.
- Configuration validation is exact-type, recursive, and transactional.
  Canonical metadata and lookup paths use reversible JSON Pointer `~0`/`~1`
  escaping. Diagnostic paths quote and escape opaque bytes, and sensitive
  values are redacted before inspection.
- Byte I/O is synchronous and may make partial progress. A zero read means
  end-of-stream; exact reads distinguish EOF, while all-writes reject
  successful zero progress. Portable encoding supports 1/2/4/8-byte integral
  types and IEC 60559 4/8-byte floating-point types.
- `File` is move-only. `Close()` is idempotent and `noexcept`, consumes the
  handle even when native close reports an error, and returns `kIo` with the
  native code. Move assignment has a release-active precondition that its
  destination is already closed, so a close failure is not silently lost.
- Memory space and backend enums are vocabulary, not availability claims.
  `HostMemoryResource` is the only project-supplied allocator;
  `ExecutionContext::Serial` is the only available context. Buffers own bytes
  but borrow their resource; views borrow storage.
- `CopyBytes` requires an explicit context and views, validates before
  mutation, performs synchronous overlap-safe host copying, and returns an
  already-complete event. It performs no allocation, transfer, packing,
  fallback, or hidden synchronization.

## Findings and resolutions

| ID | Finding | Resolution |
| --- | --- | --- |
| DAPI-M1-001 | Live root documentation and preset language still described Milestone 0 after the core target became real. | The lead advanced the live CMake, preset, test, and documentation wording to Milestone 1 and named all seven current presets. |
| DAPI-M1-002 | Initial status visibility placement did not compile as a self-contained GCC header, and `Status::Ok` briefly lacked export visibility. | Production moved visibility to supported member declarations, exported every out-of-line public symbol, and revalidated GCC and Clang header-alone compilation. |
| DAPI-M1-003 | `Result<T>` move assignment initially declared `noexcept` without also requiring nothrow move construction. | Its condition now requires both nothrow move construction and assignment. |
| DAPI-M1-004 | Initial `Extents::Create` argument conversion could silently narrow broad integral inputs. | Construction now uses the checked integer/cast path before storing any dynamic extent. |
| DAPI-M1-005 | Initial configuration ownership and overloads admitted moved-from misuse, ambiguous integers, C-string-to-bool conversion, and unchecked floating narrowing. | The value now owns an inline variant, uses exact bool/double constraints, maps bounded integral alternatives deliberately, deletes the C-string constructor, and validates string values through `Utf8String`. |
| DAPI-M1-006 | The UTF-8 guarantee needed an exact boundary, and opaque control-byte keys could make an unescaped validation path unsafe for display. | UTF-8 applies to `kString` values, while keys remain opaque bytes. Canonical paths stay reversible; diagnostic paths and rendered values now escape quotes, backslashes, controls, NUL, and relevant raw bytes. |
| DAPI-M1-007 | Initial scalar encoding omitted 1- and 2-byte integral mappings, and the source zero-progress comment conflicted with exact-read semantics. | The scalar mapping now covers 1/2/4/8-byte integers; synchronous zero read is explicitly end-of-stream. |
| DAPI-M1-008 | File move assignment could discard an unobservable close error, while `Close()` had a conflicting allocation/noexcept design. | Move assignment now requires a closed destination. `Close()` remains `noexcept` and reports allocation-free `kIo` plus native code; explicit close is required to observe errors. |
| DAPI-M1-009 | Buffer view queries were marked `noexcept` despite constructing allocating error diagnostics on invalid state. | The incorrect `noexcept` specifications were removed. |
| DAPI-M1-010 | Several headers and sources initially relied on transitive status/result/contract includes. | Every public header now directly includes the status/result declarations it names, and sources directly include contract/status declarations they use. |
| DAPI-M1-011 | Rendered valid UTF-8 strings could expose quote, control, and NUL bytes without diagnostic escaping. | Rendering now applies redaction first and deterministic string escaping second. |
| DAPI-M1-012 | Class-wide Windows export of recursive configuration classes could force DLL-interface treatment onto private standard-library storage and template instantiations. | Configuration value, schema, origin, and validated-configuration types now export only their out-of-line public operations; inline/template operations and private storage remain undecorated. |
| DAPI-M1-013 | The private friend declarations of the configuration factory and byte-copy operation were their functions' first declarations and needed to agree with the later exported declarations. | Both friend declarations now carry `ASC_CORE_EXPORT`, matching their namespace-scope declarations without changing friendship or public signatures. |
| DAPI-M1-014 | Scoped backend and determinism values can still be constructed from unrecognized underlying values, but context creation initially classified them only through later availability logic. | `ExecutionContext::Create` now rejects unrecognized `Backend` and `Determinism` enumerators with `kInvalidArgument` before device matching or availability checks; both cases have regressions. |
| DAPI-M1-015 | Package and isolated-consumer tests launch nested builds that do not inherit the parent target's sanitizer instrumentation, so including them in the sanitizer preset would blur instrumented and uninstrumented evidence. | The ASan/UBSan test preset excludes every test labeled `package` or `consumer`. Those six registered tests remain in the non-sanitizer package matrix and are not reported as sanitizer-tested. |
| DAPI-M1-016 | The generic little-endian encoder performed an unnecessary eight-bit right shift for a one-byte scalar and therefore relied on integral-promotion behavior that a one-byte encoding does not need. | The shift is now compiled only for scalar widths greater than one byte; signed and unsigned 8-bit round trips remain explicit regressions. |

No finding was documented around. Each accepted semantic correction was made
in its owning production or lead integration scope before this review closed.
There is no open blocking documentation or public-API finding.

## Documentation disposition

This role replaced the live user-facing material in:

```text
README.md
CHANGELOG.md
docs/README.md
docs/modules/core.md
```

The result documents the exact target/component contract, ASCCMake binding,
configure/test/install/consumer commands, all development options, public
headers, ownership and lifetime, failure and partial-mutation rules, costs,
thread safety, provider limitations, and provenance.

The retained `docs/api.md`, `docs/architecture.md`, `docs/build-system.md`,
`docs/optional-backends.md`, `docs/testing.md`, `docs/design/**`,
`docs/migration/**`, and non-core `docs/modules/**` remain explicitly
superseded historical audit evidence. They are not current API guidance.

## Reviewer validation

The documentation/API role ran the following checks from the repository root
on `feature/asc-cpp-m1-core`:

```text
clang-format-19 --dry-run --Werror \
  include/asc/core.h include/asc/core/*.h src/core/*.cc
PASS

git diff --check -- \
  README.md CHANGELOG.md docs/README.md docs/modules/core.md
PASS

GCC 11 and Clang 19, each with:
  -std=c++20 -pedantic-errors -Wall -Wextra -Werror -fno-exceptions
  syntax-compile the documented <asc/core.h> host-copy example
  syntax-compile the documented Utf8String example
PASS

Local-link existence check:
  README.md docs/README.md docs/modules/core.md
PASS
```

Production separately reported strict GCC 11 and Clang 19 header-alone and
source compilation with exceptions disabled. The lead's fresh GCC Debug
static warnings-as-errors integration run configured, built, and passed all
44 registered tests, including package, relocation, and isolated-consumer
cases. Those are production/lead results, not relabeled as this role's
independent execution.

After the portability-driven corrections, this role also ran a fresh focused
GCC 11 Debug shared warnings-as-errors build:

```text
cmake -S . -B /tmp/asc-cpp-doc-rereview.<id>/build \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build /tmp/asc-cpp-doc-rereview.<id>/build --parallel 2 \
  --target asc_core_configuration_test asc_core_io_test \
           asc_core_memory_execution_test

ctest --test-dir /tmp/asc-cpp-doc-rereview.<id>/build \
  --output-on-failure \
  -R '^asc_cpp\.core\.(configuration|io|memory_execution)_test$'
PASS: 3/3
```

The corrected configuration, execution, and I/O headers also passed
header-alone GCC 11 and Clang 19 syntax compilation under strict C++20,
warnings-as-errors, and exceptions-disabled flags. Clang-format 19 and
`git diff --check` passed on the bounded correction/review set.

A CTest selection audit found 44 registered tests: the sanitizer preset's
`package|consumer` label exclusion selects 38 and excludes all six package or
consumer tests. This validates the selection wording only; it is not relabeled
as sanitizer runtime evidence.

## Evidence boundary and remaining risks

CUDA and every other GPU provider are **skipped**. This milestone contains no
provider target, provider discovery, SDK compilation, runtime provider path,
parity comparison, numerical kernel, or benchmark. It makes no performance
claim beyond documented operation costs.

Hosted MSVC, AppleClang, and published-branch CI evidence remain pending.
In particular, the selective DLL-interface and matching friend annotations
are structurally consistent and pass the available GNU/Clang shared build, but
their Windows shared-library compile/link result remains hosted MSVC evidence.
Destructor-time file close errors are inherently unobservable; durability
callers must explicitly flush and close. Non-owning configuration references,
memory views, and buffer resource pointers require the documented caller
lifetime. These are disclosed contract risks, not unresolved API defects.

Final Publication Checkpoint B acceptance still depends on the independent
verification and portability reviews and the lead's complete validation
matrix. Subject to those independent gates, the documentation and public API
review accepts Milestone 1.
