# Milestone 1 documentation and API review

Status: complete on 2026-07-27

## Scope and authority

This independent review covers only the frozen Milestone 1 Core CPU
foundation. It was performed against:

- the Milestone 1 contract and ownership ledger;
- ADRs 0004 through 0009;
- all eleven public Core headers and six compiled Core sources;
- the production self-review;
- the integrated `asc_core` target and ASCCpp component configuration; and
- the build-tree isolated Core consumer.

No utilities, expression, dense, sparse, random, provider, compatibility, or
later-milestone surface was treated as implemented.

## API review result

The reviewed API conforms to the frozen contract. No unresolved API defect
remains.

| Boundary | Review result |
| --- | --- |
| Errors | Stable `ErrorCode`; nodiscard `Status`/`Result<T>`; failed result access is a release-active fatal contract; no exception transport or mutable handler |
| Metadata | Exact signed 64-bit aliases and unsigned 32-bit rank; checked conversion/arithmetic/byte counts; rank-zero, zero-extent, and overflow semantics are explicit |
| Configuration | Exact value alternatives; UTF-8 value factory; opaque object/schema key bytes; recursive exact-type validation; defaults, origins, metadata, bounds, rollback, and redaction are coherent |
| I/O | Partial-transfer interfaces, exact/all loops, bounded text output publication, portable little-endian encoding, and move-only native-file ownership are explicit |
| Allocation | One-space resource contract; aligned host allocation; valid zero-byte owner; exactly-once release; `Buffer` retains only a non-owning resource pointer |
| Lifetime and aliasing | Buffer/resource and view/storage lifetime obligations are visible; mutable views do not synchronize; overlapping host `CopyBytes` uses move semantics |
| Execution | Immutable copyable serial context; serial device ordinal zero only; no auto selection, fallback, hidden allocation/transfer/synchronization, or provider API |
| Events | Move-only; moved-from use returns `kInvalidState`; serial copies return already-complete events |
| Thread safety | Concurrent immutable reads are separated from same-object mutation, move, close, wait, destruction, and caller-owned memory synchronization |
| Package | `asc_core` / `ASC::core` only; C++20; no direct link dependency; explicit `core` request required; future components remain unavailable |

The eleven-header inventory is exact. Public signatures contain no provider
SDK type. Internal header helpers use file-specific namespaces containing
`internal`, and internal types do not occur in supported public signatures.

## Finding and resolution

### D1: `File` move-assignment precondition was not visible

Initial severity: medium.

`File::operator=(File&&)` invokes the release-active fatal-contract path when
the destination still owns an open file. The behavior prevents an implicit
close from discarding an I/O error, but the initial public declaration did not
state the precondition and looked like conventional resource-replacing move
assignment.

Resolution: production added a public-header comment requiring the destination
to be closed before move assignment. The Core guide explains the reason and
the failure behavior. The implementation and header now agree. Finding
closed.

Production also reported and corrected an origin-propagation defect before
this review completed: exact JSON Pointer origin overrides beneath list values
had been accepted but ignored. Read-only inspection confirms recursive
metadata recording now applies exact list-element and descendant overrides,
and rejects origin paths absent from the validated tree.

## Documentation result

The current documentation now:

- identifies Milestone 1 as an unreleased Core-only candidate;
- documents the exact component and direct-dependency boundary;
- uses the implemented header, target, and execution APIs;
- covers ownership, resource and view lifetimes, aliasing, allocation,
  recoverable errors, fatal contracts, execution, event invalidation, and
  thread safety;
- describes programmatic configuration without claiming a parser;
- describes only local byte/text I/O and portable scalar encoding;
- distinguishes current Core documentation from retained five-component
  history; and
- makes no availability claim for a later module or CPU/GPU provider.

GPU evidence for this provider-free CPU milestone is **skipped**.

## Independent documentation validation

Exact commands and results:

```sh
cmake -S . -B build/m1-doc-review -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
```

Result: failed before configuration because Ninja is not installed. This is
an environment limitation, not a product failure.

```sh
cmake -S . -B build/m1-doc-review -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build build/m1-doc-review --parallel 4
```

Result: passed; all six Core sources built and `libasc_core.a` linked.

```sh
cmake -S tests/consumer/core -B build/m1-doc-consumer \
  -G 'Unix Makefiles' \
  -DASCCpp_DIR=/home/yicai/AI4SciComp/asc-cpp/build/m1-doc-review/package/ASCCpp
cmake --build build/m1-doc-consumer --parallel 4
./build/m1-doc-consumer/asc_cpp_core_consumer
```

Result: passed configure, build, package-property checks, link, and runtime.

```sh
printf '%s\n' '#include <asc/core.h>' '' 'int main() {' \
  '  const asc::ExecutionContext context = asc::ExecutionContext::Serial();' \
  '  return context.backend() == asc::Backend::kSerial ? 0 : 1;' '}' |
  g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
    -x c++ - -x none build/m1-doc-review/src/core/libasc_core.a \
    -o build/m1-doc-review/readme-example
./build/m1-doc-review/readme-example
```

Result: passed compile, link, and runtime for the README example.

```sh
rg -n '[[:blank:]]+$' \
  README.md CHANGELOG.md docs/README.md docs/modules/core.md
```

Result: passed; no trailing whitespace.

An earlier combined consumer command attempted the nonexistent executable name
`asc_core_consumer` after the configure and build had passed. That run step
failed as a reviewer command typo; rerunning the generated
`asc_cpp_core_consumer` executable passed as recorded above.

## Remaining risks

- Non-owning resource pointers and memory views require caller-enforced
  lifetime discipline that the type system cannot prove.
- A `File` destructor cannot report a close failure; callers requiring
  observable completion must call `Flush()` and `Close()`.
- `CompletionEvent::Query()` and `Wait()` update event-local completion state
  and are not concurrent operations on the same event object.
- Standard-container allocation failure is limited by the platform's C++
  allocation behavior; functional error paths otherwise use status values.
- MSVC, AppleClang, sanitizer, shared-library, installation, relocation, and
  full test-matrix results remain integration/verification evidence, not
  claims from this documentation review.
