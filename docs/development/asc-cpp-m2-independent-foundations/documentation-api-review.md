# Milestone 2 documentation and API review

Status: complete on 2026-07-27

## Scope and authority

This independent review covers only the frozen Milestone 2 independent
foundation modules. It was performed against:

- `main:AGENTS.md`;
- the Milestone 2 contract, ownership ledger, and random provenance record;
- ADRs 0002, 0003, 0005, 0010, 0015, 0017, and 0018;
- the Milestone 1 Publication Checkpoint B and current Core guide;
- all ten new public headers and four compiled sources;
- the production self-review and independent verification design;
- the integrated build/package target definitions; and
- the complete local Debug/static warnings-as-errors test matrix.

No Dense, Sparse, random storage facet, provider, compatibility aggregate,
file parser, general broadcasting, entropy, or later-milestone surface was
treated as implemented.

## Public API and dependency result

The reviewed public inventory is exact:

```text
include/asc/utilities.h
include/asc/utilities/command_line.h
include/asc/utilities/export.h
include/asc/utilities/timer.h

include/asc/expression.h
include/asc/expression/expression.h

include/asc/random.h
include/asc/random/distribution.h
include/asc/random/engine.h
include/asc/random/export.h
```

| Build target | Consumer target | Kind | Direct ASC dependency |
| --- | --- | --- | --- |
| `asc_core` | `ASC::core` | Compiled | None |
| `asc_utilities` | `ASC::utilities` | Compiled | `ASC::core` |
| `asc_expression` | `ASC::expression` | Interface | `ASC::core` |
| `asc_random` | `ASC::random` | Compiled | `ASC::core` |

Utilities, Expression, and Random contain no sibling, Dense, Sparse, provider,
or external dependency edge. Expression is a genuine interface target rather
than an empty compiled library.

## API review result

No unresolved API or documentation defect remains.

### Utilities

- `CommandLineParser::Create` owns its schema and copied option table.
- Names, aliases, JSON Pointer destinations, supported scalar schema leaves,
  duplicates, and boolean-negation collisions are validated before
  publication.
- Parsing is fresh-tree transactional. It publishes an owning configuration
  and positional strings only after full Core schema validation.
- Numeric conversion is locale-independent, complete-token, and range
  checked. UTF-8 strings use the Core validated factory.
- `kCommandLine = 2` preserves the stable Core origin values. Location records
  the zero-based option-token index for both attached and separated values.
- Help is deterministic caller-owned text with no terminal I/O or implicit
  control flow.
- `Timer` exposes exact empty/running/stopped transitions and
  `steady_clock::duration`, with no floating or wall-clock conversion.

The parser is immutable after creation. Concurrent const parsing/help reads
construct independent results; moving or destroying the parser concurrently
is invalid. `Timer` has mutable per-object state and no synchronization
guarantee.

### Expression

- `ExpressionAdapter<T>` is the sole non-intrusive customization point.
- The readable protocol carries exact fixed-rank shape, scalar reads, alias
  identity, operation category, and sparsity effect without a storage or
  provider dependency.
- Arithmetic scalars are captured by value, external lvalues by non-owning
  const reference, and rvalues/nested nodes by value.
- Binary construction checks complete rank and shape before returning a node.
  Rank-zero scalar expansion is the only broadcasting behavior.
- Alias and sparsity metadata propagate conservatively.
- Construction performs no framework result allocation, read, destination
  mutation, transfer, synchronization, or dispatch.

An external adapter owns its shape/index validity contract. An lvalue or
captured non-owning view must keep its referenced object and storage alive
through every node read.

### Random

- Public counter, key, result, stream, subsequence, and offset types match the
  frozen fixed-width mapping.
- `Philox4x32_10` performs the exact ten-round unsigned mapping and is pure
  `noexcept`.
- Direct block and positioned-word generation implement the frozen
  little-word-first stream, subsequence, block, and lane mapping.
- Offset advancement reports `kOverflow` rather than wrapping.
- `Uniform01<float>` and `Uniform01<double>` implement the exact 24-bit and
  53-bit transforms with `(high, low)` double input order.
- The API retains no state, reference, storage, provider, or entropy source.

The raw and transform sequence is a 0.2.x pre-1.0 contract. It is not a
storage-fill, statistical-suitability, provider, GPU, or serialized-state
claim.

## Findings and resolutions

### A1: inconsistent command-line origin location

Initial severity: medium.

The production self-review required a zero-based option-token location.
Initial read-only source inspection found that separated values incremented
the loop index before `ParsedOptionValue` captured it, so `--count=-3` would
record `"0"` while `--count -3` would record `"1"`.

Resolution: production captures `option_token_index` before consuming a
separated value and stores it for every spelling. A focused test and the
production smoke cover the rule. Finding closed.

### D1: Core guide omitted the M2 origin kind

Initial severity: low; outside the documentation reviewer's writable scope.

The live Core guide still described only default and programmatic origins
after M2 appended `kCommandLine = 2`.

Resolution: the lead updated the guide with all three stable values while
retaining the accurate statement that Milestone 1 itself contained no parser.
Finding closed.

### I1: integrated capability and Core-consumer fixtures were stale

Initial severity: integration blocker; lead-owned.

The first complete CTest run had 76/79 passes. The architecture checker still
expected the three implemented M2 capabilities to be `proposed`, and two Core
consumer cases requested optional Utilities while asserting it was
unavailable.

Resolution: the lead aligned the capability checks with runtime-tested M2
status and changed the unavailable optional fixture to Dense. The repeated
matrix passed 79/79. Finding closed.

### D2: documentation example omitted a required argument

Initial severity: low.

The first compile of the Utilities example used `SetRequired()` rather than
the implemented `SetRequired(true)`.

Resolution: the example was corrected and the combined exact documentation
example then compiled, linked, and ran under warnings as errors. Finding
closed.

## Documentation result

The assigned live documentation now:

- identifies the unreleased 0.2.0 candidate and four available components;
- gives exact build/imported targets, target kinds, and direct dependencies;
- replaces the deleted five-component API map with the current header map;
- documents command-line grammar, transactions, origins, ownership, help,
  timing state, and concurrency without claiming file parsing;
- documents expression customization, capture lifetimes, exact shapes,
  scalar-only expansion, aliasing, sparsity, and absence of evaluation or
  writable storage;
- documents Philox lane/address mapping, sequence compatibility, exact scalar
  transforms, purity, provenance, and absence of fills or entropy; and
- marks every provider and GPU claim absent.

GPU evidence is exactly **skipped**.

## Independent validation

### Production-only build

```sh
cmake -S . -B build/m2-doc-review -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build build/m2-doc-review --parallel 4
```

Result: passed. Core, Utilities, and Random compiled and linked as static
libraries; Expression remained interface-only.

### Documentation API examples

A single strict C++20 translation unit at
`/tmp/asc-cpp-m2-documentation-example.cc` combined the Utilities schema,
parser and timer snippets, the external Expression adapter and
scalar-expansion snippet, and the Random block/address/transform snippets.

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
  /tmp/asc-cpp-m2-documentation-example.cc \
  build/m2-doc-matrix/src/utilities/libasc_utilities.a \
  build/m2-doc-matrix/src/random/libasc_random.a \
  build/m2-doc-matrix/src/core/libasc_core.a \
  -o build/m2-doc-matrix/documentation-examples
./build/m2-doc-matrix/documentation-examples
```

Result: passed compile, link, parser/configuration behavior, timer transition,
external adapter and scalar expansion, all-zero Philox word, offset advance,
and unit-transform range checks.

### Integrated warnings-as-errors matrix

```sh
cmake -S . -B build/m2-doc-matrix -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build build/m2-doc-matrix --parallel 4
ctest --test-dir build/m2-doc-matrix --output-on-failure
```

Result: final pass, 79/79 tests. This includes every Core and M2 public header
with and without exceptions, dependency scans, expression compile
contracts/multi-TU use, all runtime suites, subproject use, static build-tree
and relocated installed consumers for each component, component package
isolation, and package-registry preservation.

The first test-enabled configure failed while lead-owned
`tests/expression/CMakeLists.txt` registration was still being integrated.
After registration, the first CTest run exposed the three I1 failures
described above; the final repeated matrix is clean.

### Documentation hygiene

```sh
rg -n '[[:blank:]]+$' \
  README.md CHANGELOG.md docs/README.md docs/api.md \
  docs/modules/utilities.md docs/modules/expression.md \
  docs/modules/random.md
git diff --check -- \
  README.md CHANGELOG.md docs/README.md docs/api.md \
  docs/modules/utilities.md docs/modules/expression.md \
  docs/modules/random.md
```

Result: passed; no trailing whitespace or patch-format error.

## Remaining risks

- The 0.2 API is pre-1.0 and may change at a later minor release.
- Expression lvalue holders and captured non-owning views rely on
  caller-enforced object/storage lifetime and synchronization.
- External adapters are responsible for valid extents and index access.
- A `Timer` object is not safe for concurrent mutation/query without caller
  synchronization.
- Parsing and node capture can invoke ordinary C++ standard-container or
  operand copy/move allocation behavior; the no-result-allocation claim does
  not override those operations.
- Hosted MSVC and AppleClang, shared-library matrices, sanitizers, and
  minimum-CMake validation remain integration/portability evidence rather
  than claims of this documentation review.
- No provider or GPU implementation exists, so configure-tested,
  compile-tested, runtime-tested, and parity-tested GPU evidence are all
  inapplicable; the classification remains **skipped**.
