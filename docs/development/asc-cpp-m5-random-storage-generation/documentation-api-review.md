# Milestone 5 Documentation and API Review

Status: complete on 2026-07-28

## Scope and authority

This independent review covers only the frozen Milestone 5 random storage
generation facets. It was performed against:

- `main:AGENTS.md`;
- the frozen Milestone 5 contract, ownership ledger, and provenance record;
- the six-module architecture blueprint and ADRs 0003, 0004, 0007, 0008,
  0009, 0011, 0012, 0015, 0017, and 0018;
- the already-public Core, Dense, Sparse, and Random APIs;
- both new public facet headers; and
- the current product-target and package-component definitions.

No MdeCpp/deleted asc-cpp implementation, test, literal vector, table, or
prose was used. No Milestone 6 provider or GPU surface was treated as
implemented.

## Public API and dependency result

The new public production inventory is exact:

```text
include/asc/random/dense.h
include/asc/random/sparse.h
```

| Build target | Consumer target | Kind | Direct ASC dependencies |
| --- | --- | --- | --- |
| `asc_random_dense` | `ASC::random_dense` | Functional interface facet | `ASC::random`, `ASC::dense` |
| `asc_random_sparse` | `ASC::random_sparse` | Functional interface facet | `ASC::random`, `ASC::sparse` |
| `asc_cpp` | `ASC::cpp` | Convenience interface aggregate | Six modules and both facets |

The base `<asc/random.h>` umbrella remains storage-neutral and includes
neither facet. A Random-only consumer therefore gains no Dense or Sparse edge.
Each facet is separately usable and imports neither the sibling storage
module nor the sibling facet. The aggregate owns no behavior and is not a
dependency of any narrower target.

## API review result

No unresolved production API or documentation defect remains.

### Dense state and sequence mapping

- `FillDenseUniform01` takes an explicit context, mutable dense view, stream,
  subsequence, and input offset. Its `Result<RandomOffset>` is the first
  unused word address; no input address is mutated.
- The element constraint accepts exactly unqualified `float` or `double`.
  Const destinations, Boolean, integral, cv-qualified, and user-defined
  elements fail at the public template boundary.
- Float consumes one word per logical element. Double consumes two consecutive
  words in the exact high-word/low-word transform order.
- Logical ordinal decoding increments dimension zero first. Physical layout,
  stride, and padding do not change the logical sequence.
- Rank zero writes one scalar. A zero extent writes nothing and consumes no
  word.
- Backend, placement, logical-size conversion, word-count multiplication, and
  offset advance all complete before the first write. The post-preflight loop
  contains no fallible operation.
- The facet borrows the view, allocates no storage or workspace, and performs
  no packing, transfer, synchronization, provider dispatch, or fallback.

The checked preflight makes equivalent whole and explicitly addressed
partitioned fills reproducible without a mutable engine or hidden partition
state.

### Sparse structure and value mapping

- `GenerateSparseUniform01<Element>` takes an explicit context, validated Core
  extents, exact count, host resource, and separate structure and value
  address domains.
- `SparseUniform01Generation<Element, ExtentsType>` owns exactly one move-only
  canonical `CoordinateArray` and returns both first-unused offsets.
- Equal structure and value `(stream, subsequence)` pairs are rejected;
  different offsets do not bypass the independence rule.
- Count is checked against the logical domain. Zero-extent and rank-zero
  behavior matches the frozen Core shape semantics.
- The structure priority is an unsigned 64-bit high-word/low-word value.
  Candidates are ordered by `(priority, ordinal)`, so a priority collision has
  a stable public tie-break.
- Sparse ordinal decoding varies the last dimension fastest. Builder
  finalization then publishes unique coordinates in canonical lexicographic
  order with `DuplicatePolicy::kReject` and
  `ExplicitZeroPolicy::kKeep`.
- For nonzero count, the structure address domain advances by exactly twice
  the full logical size. Count zero advances it by zero.
- Values are generated only after canonical finalization and depend only on
  canonical stored position and the value address. Float consumes one word
  per entry and double consumes two.
- The implementation makes the coordinate builder's two declared allocation
  requests and no computational workspace allocation. Selection is
  `O(exact_count * logical_size)` time with `O(rank)` local computational
  storage, in addition to the builder's documented canonicalization cost.

The API accurately describes a deterministic pseudorandom exact-count
reference sequence. It does not claim statistically perfect uniform sampling
over all coordinate subsets.

### Ownership, failure, and concurrency

- Dense generation never owns or retains its view. Its storage and owner
  outlive the synchronous call.
- Sparse generation returns a move-only owner. Its caller-provided resource
  is non-owning state and must outlive final deallocation.
- Sparse metadata, address-domain, and offset checks complete before the first
  allocation. Builder RAII releases a successful first allocation if the
  second fails, and no failed `Result` publishes an owner.
- Successful calls retain no engine, cursor, registry, entropy source,
  context, view, provider handle, or hidden synchronization state.
- Independent operations may execute concurrently when destination,
  allocation-resource, and object lifetimes are independently safe under the
  C++ memory model. Overlapping writes and concurrent use of a
  non-thread-safe resource remain caller responsibilities.
- Reusing an explicit address intentionally reproduces a sequence. The caller
  assigns disjoint address ranges where statistical independence is required.

### Provider, package, and compatibility boundary

Both facets accept only the serial backend and host memory/resource placement.
Unsupported execution or placement fails rather than selecting a provider or
transferring storage. No external package or optional SDK is exposed.

The raw Philox mapping and scalar transforms retain their exact 0.2.x
pre-1.0 sequence contract. Milestone 5 adds the documented dense
logical-coordinate mapping and sparse structure/value mapping on the 0.5.x
line. ABI compatibility, statistical suitability, serialized state, and
CPU/GPU parity are separate claims and are not implied.

The implementation and documentation are project-owned clean-room work. The
facet headers add no copied source, table, vector, generated data, or
third-party dependency.

## Findings and resolutions

### D1: dense documentation example lacked the owner header

Initial severity: low; documentation only.

The first exact guide example included `<asc/random/dense.h>` and then used
`DenseArray`. The facet header correctly includes the view contract it needs,
not the complete Dense owner surface, so strict example compilation reported
an incomplete `DenseArray` type.

Resolution: the guide now directly includes `<asc/dense.h>` for the owner it
uses while retaining the explicit facet include. The corrected example
compiles with strict GCC and Clang and runs successfully. Closed.

## Documentation result

`docs/modules/random.md` now:

- gives the exact base, facet, and aggregate consumption boundaries;
- preserves the raw Philox lane, stream, subsequence, offset, and
  `Uniform01` transform rules;
- documents the dense logical traversal, per-element word consumption,
  padding behavior, partition mapping, state advance, and zero-size cases;
- documents sparse priority ordering, canonical ordinal and stored order,
  exact count, stream separation, value independence, and explicit-zero
  retention;
- explains owner/resource/view lifetimes, failure transactions, allocation
  count, computational complexity, and concurrency responsibilities;
- provides checked dense-owner and sparse-owner examples with exact returned
  offsets;
- states the serial/host provider boundary and absence of transfer,
  synchronization, fallback, or optional dependencies; and
- identifies the deferred distributions, sparse modes, providers, GPU work,
  serialization, and compatibility surfaces.

## Independent validation

### Strict standalone public headers

```sh
for compiler in g++ clang++-19; do
  for flag in '' '-fno-exceptions'; do
    for header in asc/random/dense.h asc/random/sparse.h; do
      printf '#include <%s>\nint main() { return 0; }\n' "$header" |
        "$compiler" -std=c++20 -Wall -Wextra -Werror -pedantic $flag \
          -Iinclude -x c++ -fsyntax-only -
    done
  done
done
```

Result: pass, 8/8 strict standalone header configurations.

### Production-only warnings-as-errors build

```sh
cmake -S . -B build/m5-doc-review -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build build/m5-doc-review --parallel 4
```

Result: pass. Core, Utilities, Dense, Sparse, and Random compiled as static
libraries; Expression and both random facets remained functional interface
targets.

### Exact documentation examples

The combined guide example was compiled from standard input with both facet
headers, both storage umbrellas, strict warnings, and the production-only
static libraries:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -Iinclude \
  -x c++ - -x none \
  build/m5-doc-review/src/random/libasc_random.a \
  build/m5-doc-review/src/dense/libasc_dense.a \
  build/m5-doc-review/src/sparse/libasc_sparse.a \
  build/m5-doc-review/src/core/libasc_core.a \
  -o build/m5-doc-review/documentation-example
./build/m5-doc-review/documentation-example
```

Result: pass. The executable created a right-layout dense owner, filled six
doubles, observed next offset 20 from offset 8, generated three canonical
sparse doubles over shape 4-by-5, observed structure/value offsets 44 and 15,
and checked all sampled values were in `[0, 1)`.

```sh
git diff --check -- \
  docs/modules/random.md \
  docs/development/asc-cpp-m5-random-storage-generation/documentation-api-review.md
```

Result: pass.

Full test, sanitizer, package, relocation, isolated-consumer, compiler matrix,
and performance evidence remains lead-owned integration evidence rather than
a claim of this documentation review.

## Remaining risks

- The 0.5 API is pre-1.0 and may change in a later minor release.
- The sparse repeated-scan selection and builder canonicalization are
  intentionally reference algorithms; large shape/count use can be slow.
- Deterministic exact-count generation is not a proof of statistically uniform
  subset sampling or fitness for a particular scientific application.
- Raw-pointer views cannot validate allocation capacity or lifetime, and the
  types cannot detect an address-range collision chosen by the caller.
- A caller must preserve storage, owner, resource, and concurrency lifetimes;
  no facet adds synchronization.
- Hosted MSVC/AppleClang, shared/DLL, sanitizer, package/relocation,
  isolated-consumer, and full runtime matrices remain final integration
  evidence.
- No GPU facet or provider exists in Milestone 5. GPU evidence is exactly
  **skipped**.
