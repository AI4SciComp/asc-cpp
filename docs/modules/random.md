# Random module and storage-generation facets

`ASC::random` is the explicit-state, storage-neutral random-bit component in
the unreleased ASCCpp `0.9.0` candidate. It supplies a pure Philox4x32-10
raw-bit engine, explicit stream/subsequence/word-offset addressing, checked
offset advancement, and exact scalar unit-uniform transforms.

Milestone 5 adds two interface facets without changing that base boundary:

- `ASC::random_dense` fills an existing mutable dense view; and
- `ASC::random_sparse` constructs an exact-count canonical coordinate array.

Milestone 7 adds three separately consumable CUDA provider facets:

- `ASC::random_cuda` fills caller-owned device storage with raw Philox words;
- `ASC::random_dense_cuda` fills an existing device dense view; and
- `ASC::random_sparse_cuda` constructs an exact-count device coordinate
  owner.

The dense and sparse facets are independent. Neither storage module depends on
random, the base random umbrella includes neither facet, and the facets do not
depend on one another. CUDA remains opt-in and none of the provider facets is
added to `ASC::cpp`.

## Build and dependency contract

| Component | Build target | Installed target | Kind | Direct ASC dependencies |
| --- | --- | --- | --- | --- |
| random base | `asc_random` | `ASC::random` | compiled static/shared library | `ASC::core` |
| dense facet | `asc_random_dense` | `ASC::random_dense` | interface library | `ASC::random`, `ASC::dense` |
| sparse facet | `asc_random_sparse` | `ASC::random_sparse` | interface library | `ASC::random`, `ASC::sparse` |
| aggregate | `asc_cpp` | `ASC::cpp` | interface library | all six modules and both facets |

All four components require C++20 and have no external dependency.

The M7 providers are compiled libraries available only with
`ASC_CPP_ENABLE_CUDA=ON`:

| Provider component | Build/installed target | Exact direct ASC dependencies | Provider dependency |
| --- | --- | --- | --- |
| raw CUDA | `asc_random_cuda` / `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | CUDA Runtime through `core_cuda` |
| dense CUDA | `asc_random_dense_cuda` / `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | CUDA Runtime through `core_cuda` |
| sparse CUDA | `asc_random_sparse_cuda` / `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | CUDA Runtime through `core_cuda` |

`ASC::random_dense_cuda` does not import sparse, and
`ASC::random_sparse_cuda` does not import dense. Provider-free configuration
does not enable the CUDA language or discover CUDAToolkit.

An installed consumer requests and links only the capability it uses:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense)
target_link_libraries(my_dense_target PRIVATE ASC::random_dense)
```

or:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_sparse)
target_link_libraries(my_sparse_target PRIVATE ASC::random_sparse)
```

The `random_dense` package closure contains
`core;expression;random;dense;random_dense`; it imports no sparse target. The
`random_sparse` closure contains
`core;expression;random;sparse;random_sparse`; it imports no dense target.
Requesting `COMPONENTS random` imports only core and the base random target.
With no component list, the package loads the `cpp` aggregate and its complete
six-module/two-facet closure.

## Public headers and API map

| Header | Contract |
| --- | --- |
| `<asc/random.h>` | complete storage-neutral random base; it deliberately excludes both storage facets |
| `<asc/random/engine.h>` | Philox state vocabulary, raw-bit generation, address mapping, and checked offset advancement |
| `<asc/random/distribution.h>` | exact `Uniform01<float>` and `Uniform01<double>` transforms |
| `<asc/random/export.h>` | base compiled-library symbol visibility |
| `<asc/random/dense.h>` | dense generation facet and `FillDenseUniform01` |
| `<asc/random/sparse.h>` | sparse generation facet, `SparseUniform01Generation`, and `GenerateSparseUniform01` |
| `<asc/random/providers/cuda.h>` | raw device Philox generation |
| `<asc/random/providers/cuda_export.h>` | raw CUDA symbol visibility |
| `<asc/random/providers/dense_cuda.h>` | device dense Uniform01 generation |
| `<asc/random/providers/dense_cuda_export.h>` | dense CUDA symbol visibility |
| `<asc/random/providers/sparse_cuda.h>` | exact-count device sparse generation |
| `<asc/random/providers/sparse_cuda_export.h>` | sparse CUDA symbol visibility |

The headers are self-contained. All supported public declarations are directly
in `namespace asc`; implementation-only names contain `internal`. Facet
headers contain no provider SDK header or provider type.

## Explicit state and raw engine

`Philox4x32Counter` contains four public `std::uint32_t` lanes in order
`[c0,c1,c2,c3]`. `Philox4x32Key` contains two public lanes in order `[k0,k1]`.
A direct Philox result contains four words in the same lane order.

`Philox4x32_10` applies exactly ten rounds. Round zero uses the supplied key.
Each later round adds the fixed Philox Weyl schedule modulo 2^32. Products and
key arithmetic use unsigned fixed-width semantics.

For input lanes `[c0,c1,c2,c3]` and key `[k0,k1]`, one round computes:

```text
p0 = 0xD2511F53 * c0
p1 = 0xCD9E8D57 * c2

output = [
  high(p1) xor c1 xor k0,
  low(p1),
  high(p0) xor c3 xor k1,
  low(p0)
]
```

Between rounds the key adds `[0x9E3779B9,0xBB67AE85]` modulo 2^32. Direct block
generation is a pure `noexcept` operation:

```text
result = Philox4x32_10(counter, key)
```

It allocates no memory, reads no hidden state, mutates no argument, and calls
no provider. The same key/counter words produce the same four words on every
conforming implementation.

## Stream, subsequence, and word-offset mapping

The complete public address vocabulary is:

```text
RandomStream       unsigned 64-bit
RandomSubsequence  unsigned 64-bit
RandomOffset       unsigned 64-bit
```

The base mapping is exact:

- stream maps little-word-first into key lanes `[k0,k1]`;
- subsequence maps little-word-first into counter lanes `[c2,c3]`;
- word offset selects block `offset / 4`, mapped little-word-first into
  `[c0,c1]`; and
- `offset % 4` selects the result lane.

`Philox4x32Block(stream, subsequence, block)` performs the address mapping and
returns four words. `Philox4x32Word(stream, subsequence, offset)` maps the word
offset and returns its selected lane. There is no mutable cursor.
`AdvanceRandomOffset` checks addition and returns `ErrorCode::kOverflow`
instead of wrapping.

```cpp
#include <asc/random.h>

#include <cstdint>

int main() {
  constexpr asc::RandomStream kStream = 17;
  constexpr asc::RandomSubsequence kSubsequence = 4;
  constexpr asc::RandomOffset kOffset = 8;

  const std::uint32_t word =
      asc::Philox4x32Word(kStream, kSubsequence, kOffset);
  const float value = asc::Uniform01<float>(word);

  auto next = asc::AdvanceRandomOffset(kOffset, 1);
  return next.ok() && *next == 9 && value >= 0.0F && value < 1.0F ? 0 : 1;
}
```

The algorithm identity, lane order, key schedule, and address mapping remain
the exact pre-1.0 sequence API introduced by ASCCpp `0.2.0`. A future
incompatible sequence must use a separately versioned algorithm or an
explicitly documented breaking release.

There is no mutable engine period or seed-expansion algorithm to describe.
The raw interface accepts the complete 128-bit counter and 64-bit key values;
the convenience address interface exposes a 64-bit stream, 64-bit
subsequence, and a 64-bit block or word offset. Public fixed-width structs are
copied and moved by value. Their native object layout is not a serialization
format.

## Exact `Uniform01` transforms

`Uniform01<float>(word)` consumes one raw 32-bit word:

```text
value = most_significant_24_bits(word) * 2^-24
```

`Uniform01<double>(high_word, low_word)` consumes two words in `(high, low)`
order, concatenates them into one 64-bit bit string, and computes:

```text
value = most_significant_53_bits(concatenated) * 2^-53
```

The binary scales are exactly representable. Results are exactly in `[0,1)`:

- zero input maps to positive zero;
- all-one input maps to `1 - 2^-24` for `float`; and
- all-one input maps to `1 - 2^-53` for `double`.

Discarded low bits do not affect the result. No
`std::uniform_real_distribution` or implementation-defined standard-library
sequence participates.

The compiled component requires IEC 60559 radix-two native representations
with 24 `float` significand bits and 53 `double` significand bits.
Compile-time guards reject a platform that cannot honor the exact transforms.

## Dense generation facet

The dense facet has one operation:

```text
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>
```

Template deduction accepts exactly an unqualified mutable `float` or `double`
`DenseView`. The context, stream, subsequence, and starting offset are always
explicit. The operation supports exactly serial execution and host memory in
Milestone 5.

Logical traversal is independent of physical layout. For shape
`e[0], ..., e[Rank-1]`, dimension zero changes fastest:

```text
ordinal = i[0] + e[0] * (i[1] + e[1] * (...))
```

The word mapping is:

```text
float element i:
  Uniform01<float>(word(offset + i))

double element i:
  Uniform01<double>(word(offset + 2*i), word(offset + 2*i + 1))
```

The first `double` word is the high word. Success returns the first unused
word offset: `offset + logical_size` for `float` or
`offset + 2*logical_size` for `double`.

The same logical coordinates therefore receive the same values in
`LayoutLeft`, `LayoutRight`, and a proven-unique `LayoutStride` mapping.
Only logical elements are written; padding and holes are preserved. Rank zero
contains one logical scalar. A shape with any zero extent consumes no words,
writes nothing, and returns the input offset.

```cpp
#include <asc/dense.h>
#include <asc/random/dense.h>

#include <utility>

int main() {
  using Shape = asc::Extents<2, 3>;
  using Array = asc::DenseArray<double, Shape>;

  auto extents = Shape::Create();
  if (!extents.ok()) {
    return 1;
  }

  asc::HostMemoryResource resource;
  auto array_result = Array::Create(*extents, resource, asc::LayoutRight{});
  if (!array_result.ok()) {
    return 2;
  }
  Array array = std::move(*array_result);

  auto view = array.view();
  if (!view.ok()) {
    return 3;
  }

  auto next = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *view, asc::RandomStream{7},
      asc::RandomSubsequence{3}, asc::RandomOffset{11});
  return next.ok() && *next == 23 ? 0 : 4;
}
```

The resource precedes the owner and therefore outlives its deallocation. The
fill borrows the view synchronously, allocates no computational storage or
workspace, and does not extend the array lifetime.

Partition invariance follows from explicit word addressing, not from a hidden
cursor. A partition must pass each subview the starting offset corresponding
to its first consecutive logical ordinal. Partitioning by physical address,
or partitioning a region that is not a consecutive segment of the documented
logical order, does not establish this equivalence.

## Sparse generation facet

The sparse facet returns:

```text
SparseUniform01Generation<Element, ExtentsType>
  array
  next_structure_offset
  next_value_offset
```

`array` is a move-only `CoordinateArray`. The operation is:

```text
GenerateSparseUniform01<Element>(
    context, extents, exact_count, resource,
    structure_stream, structure_subsequence, structure_offset,
    value_stream, value_subsequence, value_offset)
    -> Result<SparseUniform01Generation<Element, ExtentsType>>
```

`Element` must be specified and is exactly unqualified `float` or `double`.
`ExtentsType` is an unqualified core `Extents<...>` specialization and is
deduced from `extents`. The execution context, host memory resource, structure
address, and value address are explicit. The structure and value
`(stream, subsequence)` pairs must be distinct; changing only their offsets
does not satisfy that domain-separation rule.

`exact_count` must be in `[0, extents.logical_size()]`. Rank zero has one
logical coordinate and permits a count of zero or one. A zero-extent domain
permits only count zero.

### Structure mapping

Sparse candidate ordinals use canonical lexicographic coordinate order, with
the last dimension changing fastest:

```text
ordinal = (...((i[0] * e[1] + i[1]) * e[2] + i[2])...)
```

For each logical ordinal, two structure words form one unsigned 64-bit
priority:

```text
priority =
    uint64(word(structure_offset + 2*ordinal)) << 32
  | uint64(word(structure_offset + 2*ordinal + 1))
```

The first word supplies the high 32 bits. Candidates are ordered by
`(priority, ordinal)`, so an ordinal breaks a priority tie deterministically.
The exact `exact_count` smallest candidates are selected without replacement.
They are then finalized through the sparse coordinate builder with duplicate
rejection and explicit-zero retention. The published structure is unique and
sorted in canonical lexicographic order.

For nonzero count, structure generation consumes two words for every logical
coordinate, independent of how many entries are selected. Its next offset is
`structure_offset + 2*logical_size`. Count zero consumes no structure word and
returns the input structure offset.

### Value mapping

Values are assigned only after canonical structure finalization. Stored
position `p`, not candidate ordinal or selection order, chooses the value
address:

```text
float stored position p:
  Uniform01<float>(word(value_offset + p))

double stored position p:
  Uniform01<double>(word(value_offset + 2*p),
                    word(value_offset + 2*p + 1))
```

Success returns `value_offset + exact_count` for `float` or
`value_offset + 2*exact_count` for `double`. An exact generated zero remains a
stored entry.

Changing only the value address preserves the canonical coordinates. For the
same shape, count, and value address, changing only the structure address
preserves the canonical stored-position value sequence, although those values
can attach to different coordinates. The contract does not promise that two
different addresses produce different bits or structures.

```cpp
#include <asc/random/sparse.h>

int main() {
  using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

  auto extents = Shape::Create(3, 4);
  if (!extents.ok()) {
    return 1;
  }

  asc::HostMemoryResource resource;
  auto generated = asc::GenerateSparseUniform01<double>(
      asc::ExecutionContext::Serial(), *extents, 4, resource,
      asc::RandomStream{7}, asc::RandomSubsequence{3}, asc::RandomOffset{10},
      asc::RandomStream{8}, asc::RandomSubsequence{3}, asc::RandomOffset{40});
  if (!generated.ok()) {
    return 2;
  }

  auto view = generated->array.view();
  if (!view.ok()) {
    return 3;
  }
  return view->nnz() == 4 && generated->next_structure_offset == 34 &&
                 generated->next_value_offset == 48
             ? 0
             : 4;
}
```

The resource precedes the returned owner and must remain alive through its
final deallocation. Views borrowed from `generated->array` do not extend that
owner's lifetime.

## Validation, failures, and transaction boundaries

Dense generation validates execution, placement, logical-size conversion,
word-count multiplication, and returned-offset addition before the first
write. On failure, every logical element and every padding location remains
unchanged.

Sparse generation validates execution, destination placement, count,
structure/value domain separation, word counts, and both returned offsets
before requesting output storage. A validation failure performs no resource
allocation. Allocation failures publish no owner; any earlier successful
builder allocation is released exactly once. A failed call publishes no
advanced state because offsets are input and result values, never mutated
references.

Machine-readable failures include:

- `kUnsupported` for a non-serial backend or non-host placement;
- `kInvalidArgument` for a negative/out-of-domain count or equal
  structure/value domain;
- `kOverflow` for an unrepresentable count, word count, or next offset; and
- a propagated resource status, normally `kAllocation`, when output allocation
  fails.

Diagnostic message text is not a stable interface and may allocate through
core `Status`. No-allocation statements apply to successful computational
storage and workspace, not to construction of an error diagnostic.

## Ownership, cost, and concurrency

Base random address, counter, key, raw-word, and scalar-transform values are
small fixed-width values owned by the caller. Pure base calls retain no
reference and perform no dynamic allocation.

Dense fill is synchronous, borrows its mutable destination, takes
`O(logical_size * Rank)` time, and uses `O(Rank)` automatic coordinate
metadata. It performs no general allocation, transfer, synchronization,
packing, or workspace acquisition.

Sparse selection deliberately uses a bounded-memory serial reference
algorithm:

| Phase | Time | Storage |
| --- | --- | --- |
| metadata validation | `O(1)` after validated extents | none |
| exact priority selection | `O(exact_count * logical_size)` | `O(Rank)` automatic metadata |
| coordinate finalization | existing reference `O(exact_count^2 * Rank)` worst case | reuses output buffers |
| value generation | `O(exact_count)` | none |

Successful sparse generation requests only the coordinate builder's coordinate
and value buffers. It creates no selection workspace, dense mask, temporary
coordinate list, compressed representation, or hidden transfer. The supplied
resource is not owned and must outlive the builder/result allocation.

Pure base calls are safe to execute concurrently. Dense and sparse facet calls
share no hidden random state. Separate calls may run concurrently when
destinations and resources permit it. Concurrent mutation of one dense view,
overlapping destinations, or unsynchronized use of a non-thread-safe shared
resource remains the caller's responsibility.

Applications must partition address domains explicitly. Reusing an address
tuple intentionally reproduces the same word sequence; using overlapping
tuples can intentionally or accidentally reuse words.

## CUDA generation facets

M7 provides project-owned CUDA kernels for the same Philox and Uniform01
contracts. The CUDA public headers contain no CUDA SDK type. They accept an
explicit CUDA `ExecutionContext`, device storage, and the same explicit random
address vocabulary as the provider-free operations. They never use cuRAND,
entropy, a mutable pool, a default engine, implicit transfer, packing,
workspace, or fallback.

An installed raw-provider consumer requests only its component:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_cuda)
target_link_libraries(my_target PRIVATE ASC::random_cuda)
```

The dense and sparse forms use `COMPONENTS random_dense_cuda` and
`COMPONENTS random_sparse_cuda` respectively, then link the matching imported
target. A package request that reaches one of those components discovers
CUDAToolkit through `core_cuda`. Provider-free component requests from the
same installation do not.

### Raw device words

The raw operation and move-only result are:

```text
CudaFillPhilox4x32(
    context, uint32_device_pointer, word_count,
    stream, subsequence, offset)
  -> Result<CudaRandomWordGeneration>

CudaRandomWordGeneration
  completion
  next_offset
```

Logical word `i` is bit-identical to
`Philox4x32Word(stream, subsequence, offset + i)`. The operation validates the
CUDA context and device, returned-offset arithmetic, byte count, alignment,
representable address span, and pointer placement before launch. ASC-owned
storage additionally receives an exact registered-allocation subspan check.
Zero count consumes no word, accepts a null destination,
returns the input offset with an already-complete event, and enqueues no CUDA
work.

CUDA Runtime 12.9 exposes no allocation-range query for arbitrary external
pointers. External and suballocator users must therefore keep the declared
word span inside the actual logical allocation.

```cpp
#include <asc/core/providers/cuda.h>
#include <asc/random/providers/cuda.h>

#include <cstdint>

int main() {
  const asc::Device device{asc::Backend::kCuda, 0};
  auto context = asc::CreateCudaExecutionContext(device);
  auto resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!context.ok() || !resource.ok()) {
    return 1;
  }
  auto words = asc::Buffer::Allocate(**resource, 16 * sizeof(std::uint32_t),
                                     alignof(std::uint32_t));
  if (!words.ok()) {
    return 2;
  }

  auto generated = asc::CudaFillPhilox4x32(
      *context, static_cast<std::uint32_t*>(words->data()), 16,
      asc::RandomStream{7}, asc::RandomSubsequence{3}, asc::RandomOffset{11});
  if (!generated.ok() || generated->next_offset != 27) {
    return 3;
  }
  return generated->completion.Wait().ok() ? 0 : 4;
}
```

The context and resource precede the storage and result. The explicit wait
completes device use before destruction. The result event retains provider
completion state, not the caller's buffer or memory resource.

### Device dense Uniform01

```text
CudaFillDenseUniform01(
    context, mutable_device_dense_view,
    stream, subsequence, offset)
  -> Result<CudaDenseUniform01Generation>

CudaDenseUniform01Generation
  completion
  next_offset
```

The destination element type is exactly mutable `float` or `double`, and rank
is zero through eight. The view must be device-resident and have the
proven-unique nonnegative mapping required by `DenseView`. `LayoutLeft`,
`LayoutRight`, and unique padded `LayoutStride` mappings are supported.
Dimension zero varies fastest independently of physical layout. Float
consumes one word and double consumes two high-word-first adjacent words per
logical coordinate, exactly as in `FillDenseUniform01`. Only logical elements
are written; padding is untouched.

The provider validates logical size and word consumption, the returned
offset, scalar alignment, required physical byte span, CUDA placement, device
identity, and registered allocation bounds for ASC-owned storage before
mutation. A zero extent consumes
nothing and returns an already-complete event without a kernel or event
record. The backing device storage and context remain alive through
completion. The call allocates no computational storage.

As with raw fill, Runtime validation cannot discover an arbitrary external or
smaller logical suballocation. The view's declared physical span must remain
inside the caller's allocation.

### Device exact-count sparse Uniform01

```text
CudaGenerateSparseUniform01<Element>(
    context, extents, exact_count, device_resource,
    structure_stream, structure_subsequence, structure_offset,
    value_stream, value_subsequence, value_offset)
  -> Result<CudaSparseUniform01Generation<Element, ExtentsType>>

CudaSparseUniform01Generation
  array
  completion
  next_structure_offset
  next_value_offset
```

`Element` is exactly `float` or `double`; compile-time rank is zero through
eight. The move-only result owns a device `CoordinateArray` plus its
completion event. Candidate priority, ordinal tie breaking, structure/value
domain separation, canonical coordinate order, value assignment, and word
consumption are bit-identical to `GenerateSparseUniform01`. Rank zero, zero
extent, count zero, and full count follow the provider-free rules.

The caller supplies one device memory resource. Generation requests exactly
the canonical coordinate and value result buffers and no computational
workspace. The deliberately bounded implementation uses one device thread,
`O(exact_count * logical_size + exact_count^2 * Rank)` time, and constant
automatic metadata. It favors auditable low workspace over throughput and is
not an optimized large-domain selector.

Validation covers context, rank, device resource placement, count, independent
structure/value domains, all word and offset arithmetic, allocation, output
span placement/device/bounds, and disjoint result buffers. A failure publishes
no owner; partial result allocations are released. Count zero leaves both
offsets unchanged and returns an already-complete event. The context, device
resource, returned owner, and its storage remain alive through completion.
Borrowed views do not extend owner lifetime.

## CUDA failures, ownership, and concurrency

All recoverable provider failures use `Status`/`Result` and stable ASC error
categories. Typical categories are `kInvalidArgument` for the wrong context,
count, or address-domain relationship; `kMemoryAccess` for placement,
alignment, allocation-span, or device-storage violations; `kOverflow` for
size/offset arithmetic; `kUnsupported` for rank beyond eight; `kAllocation`
for resource failure; and `kProvider` for CUDA launch/event failure. Provider
failures preserve provider name and signed native code. Diagnostic text is not
stable API.

The three generation results are noncopyable and movable because they contain
a move-only completion event or owner. Event destruction does not wait or keep
caller storage alive. A successful nonempty call is asynchronous; wait or
query its event before reusing or releasing destination storage and resources.
Separate contexts and disjoint storage may execute concurrently. Calls share
no hidden random state. Overlapping mutable destinations and unsynchronized
use of one non-thread-safe resource remain caller errors.

## Provider and GPU evidence boundary

The M5 provider-free operations remain synchronous serial reference paths and
perform no provider discovery, dispatch, fallback, transfer, or hidden context
selection. M7 adds only the three opt-in CUDA targets above.

Evidence uses exactly **configure-tested**, **compile-tested**,
**runtime-tested**, **parity-tested**, or **skipped**. Toolkit discovery alone
is configure-tested. Provider compilation alone is compile-tested. Real-device
execution without an independent oracle is runtime-tested. Only comparison
with an independent Philox/Uniform01 or provider-free coordinate oracle is
parity-tested. Missing toolchain, hardware, operation, or matrix is skipped.
The Milestone 7 Publication Checkpoint B report records the actual local label
for each raw, dense, and sparse case; this guide does not infer one label from
another.

## Numerical and statistical boundary

The base component guarantees the named integer algorithm, address mapping,
and exact binary transforms. The facets guarantee their named logical and
canonical-storage mappings. They do not claim statistical suitability for
every application, independent results for overlapping address domains, a
formal distribution over sparse subsets, or different output for every
different address.

The generated scalar values are nonnegative and strictly less than one.
Floating-point NaN, infinity, negative zero, and rounding-mode-dependent
standard-library distributions are not introduced by the documented
transform.

## Provenance

The engine is a clean-room project-owned implementation of mathematical
requirements selected from:

John K. Salmon, Mark A. Moraes, Ron O. Dror, and David E. Shaw,
*Parallel Random Numbers: As Easy as 1, 2, 3*, SC11 (2011).

The exact author-hosted artifact hash, approved sections, clean-room boundary,
and prohibited implementation sources are in the
[Milestone 2 provenance record](../development/asc-cpp-m2-independent-foundations/provenance-record.md).
The scalar transforms are project-owned contracts.

The two storage facets are new project-owned work derived from the approved
Milestone 5 contracts, the public base engine, and elementary coordinate and
priority ordering definitions. Their clean-room boundary is in the
[Milestone 5 provenance record](../development/asc-cpp-m5-random-storage-generation/provenance-record.md).

No MdeCpp or user-deleted asc-cpp implementation, test, literal vector, table,
or prose was copied. No Random123 implementation, upstream test-vector corpus,
generated table, or third-party code was copied. The project remains
Apache-2.0 and Milestone 5 adds no dependency or third-party notice.

## Deferred work

Mutable/default engines, entropy acquisition, skip-ahead objects, additional
distributions, state serialization, Sobol data, statistical test APIs,
density/Bernoulli sparse generation, direct compressed output, optimized or
parallel sparse selection, cuRAND, non-CUDA providers, and any GPU
reproducibility claim beyond recorded Milestone 7 evidence require later
approved milestones.
