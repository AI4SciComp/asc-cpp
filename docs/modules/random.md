# Random module

`ASC::random` provides reproducible raw Philox4x32-10 words and exact scalar
unit-interval transforms. Its Dense and Sparse storage-generation facets are
separately consumable. Milestone 7 adds three separately requested CUDA
provider facets without changing the provider-free graph. The base is a
compiled library with one direct ASC dependency, `ASC::core`, and no external
dependency.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random)
target_link_libraries(my_target PRIVATE ASC::random)
```

```cpp
#include <asc/random.h>
```

The umbrella includes `<asc/random/engine.h>` and
`<asc/random/distribution.h>`. Base Random does not include Utilities,
Expression, Dense, Sparse, a provider header, or a storage view.

The storage facets are explicit:

| Component | Public header | Direct ASC dependencies |
| --- | --- | --- |
| `ASC::random_dense` | `<asc/random/dense.h>` | `ASC::random`, `ASC::dense` |
| `ASC::random_sparse` | `<asc/random/sparse.h>` | `ASC::random`, `ASC::sparse` |

Linking `ASC::random_dense` does not import Sparse, and linking
`ASC::random_sparse` does not import Dense. The base umbrella deliberately
does not include either facet header. `ASC::cpp` is the convenience aggregate
of all six provider-free modules and both facets; narrower targets never
depend on it.

## Provenance and compatibility

The raw engine is a clean-room implementation independently derived from the
paper and exact project mapping frozen in the
[Milestone 2 provenance record][provenance]. No MdeCpp, deleted asc-cpp,
Random123 implementation, upstream test-vector corpus, generated table, or
vendored source is an implementation input.

Issue 12 adds no Random API. It freezes a generated
[33-row architecture and provenance crosswalk][random-crosswalk] and the
[Random contract for Issues 13–15][random-contract]. The approved future work
uses exact stateful-engine versions, a separately licensed Joe/Kuo Sobol input,
and free-function Dense/Sparse adapters on the existing target graph. New work
is portable serial CPU only unless a later Gate A explicitly approves a GPU
implementation. It may not hide allocation, transfer, synchronization,
provider selection, or fallback.

The examples below intentionally use only currently shipped Philox,
`Uniform01`, Dense fill, Sparse generation, and CUDA APIs. Planned Issue 13–15
names are not public declarations and cannot be consumed yet.

Philox4x32-10 identity, lane mapping, stream/subsequence/offset mapping, and
the scalar transform rules are exact pre-1.0 sequence API first published for
ASCCpp 0.2.x. Milestone 5 extends that sequence contract with exact dense
logical-coordinate mapping and deterministic sparse structure/value mapping.
These guarantees are narrower than statistical suitability for a particular
scientific application and do not imply provider parity.

## Raw blocks and rounds

`Philox4x32Counter` contains four `std::uint32_t` words in public lane order:

```text
[c0, c1, c2, c3]
```

`Philox4x32Key` contains two words:

```text
[k0, k1]
```

The returned block uses the same four-lane order. One round computes unsigned
64-bit products:

```text
p0 = 0xD2511F53 * c0
p1 = 0xCD9E8D57 * c2
```

and returns:

```text
[high(p1) xor c1 xor k0,
 low(p1),
 high(p0) xor c3 xor k1,
 low(p0)]
```

Round zero uses the supplied key. Between successive rounds, unsigned
modulo-2^32 key addition applies:

```text
[k0 + 0x9E3779B9, k1 + 0xBB67AE85]
```

`Philox4x32_10` applies exactly ten rounds and is a pure `noexcept` operation.
It reads no entropy, mutable engine, registry, device, or provider state.

```cpp
const asc::Philox4x32Result block =
    asc::Philox4x32_10(asc::Philox4x32Counter{0, 0, 0, 0},
                       asc::Philox4x32Key{0, 0});
```

For this all-zero input, the public result lanes are:

```text
6627e8d5 e169c58d bc57ac4c 9b00dbd8
```

## Stream, subsequence, and word offset

`RandomStream`, `RandomSubsequence`, and `RandomOffset` are unsigned 64-bit
vocabulary.

- A stream maps little-word-first to `[k0, k1]`.
- A subsequence maps little-word-first to `[c2, c3]`.
- A word offset selects block `offset / 4`, mapped little-word-first to
  `[c0, c1]`.
- `offset % 4` selects the public result lane.

Direct block generation and position-to-word generation are separate public
operations. `AdvanceRandomOffset` is checked and returns `kOverflow` rather
than wrapping.

The exact operations are `GeneratePhilox4x32Block`,
`GeneratePhilox4x32Word`, and `AdvanceRandomOffset`. Direct block generation
takes a 64-bit block index; positioned generation accepts a word offset and
performs the block/lane split above.

```cpp
const asc::RandomStream stream = 7;
const asc::RandomSubsequence subsequence = 2;
const asc::RandomOffset offset = 9;

const std::uint32_t word =
    asc::GeneratePhilox4x32Word(stream, subsequence, offset);
auto next = asc::AdvanceRandomOffset(offset, 4);
if (!next.ok()) {
  return 1;
}
```

Address values are explicit caller-owned values. There is no hidden current
position and no mutable/default engine. Independent calls with identical
inputs return identical raw words.

## Exact unit transforms

`Uniform01<float>` consumes one raw 32-bit word. It selects the most
significant 24 bits and multiplies by the exactly representable scale `2^-24`.

`Uniform01<double>` consumes two words in `(high, low)` order, concatenates
them as a 64-bit bit string, selects the most significant 53 bits, and
multiplies by the exactly representable scale `2^-53`.

Both transforms return exactly in `[0, 1)`:

| Input | `float` result | `double` result |
| --- | --- | --- |
| All zero words | positive zero | positive zero |
| All one words | `1 - 2^-24` | `1 - 2^-53` |

The transforms do not call a standard-library distribution. Their documented
bit selection, input order, and output sequence do not vary with a standard
library implementation.

```cpp
const float unit_float = asc::Uniform01<float>(word);
const double unit_double =
    asc::Uniform01<double>(block[0], block[1]);
```

## Dense generation facet

Request and link the facet explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense)
target_link_libraries(my_target PRIVATE ASC::random_dense)
```

```cpp
#include <asc/dense.h>
#include <asc/random/dense.h>
```

`FillDenseUniform01` accepts an explicit `ExecutionContext`, a caller-owned
mutable `DenseView<float, Rank>` or `DenseView<double, Rank>`, and explicit
stream, subsequence, and starting word offset. It returns
`Result<RandomOffset>` containing the first unused word offset. There is no
mutable cursor or default state.

For logical ordinal `i`, `float` uses word `offset + i`. `double` uses words
`offset + 2*i` and `offset + 2*i + 1` in high-word/low-word order. Logical
coordinates increment dimension zero fastest. That order is independent of
`LayoutLeft`, `LayoutRight`, and a proven-unique non-negative
`LayoutStride`; physical padding is never visited or changed.

Rank zero generates one value. A shape with any zero extent generates no
value, changes no byte, consumes no word, and returns the input offset.
Partitioned fills reproduce a whole fill when each partition receives the
checked offset corresponding to its first logical element.

```cpp
using Shape =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

asc::HostMemoryResource resource;
auto shape = Shape::Create(2, 3);
if (!shape.ok()) {
  return 1;
}
auto values = asc::DenseArray<double, Shape>::Create(
    resource, *shape, asc::LayoutRight{});
if (!values.ok()) {
  return 1;
}
auto view = values->view();
if (!view.ok()) {
  return 1;
}

const asc::ExecutionContext context = asc::ExecutionContext::Serial();
auto next = asc::FillDenseUniform01(context, *view, 17, 3, 8);
if (!next.ok()) {
  return 1;
}
// Six doubles consume twelve words, so *next == 20.
```

Backend, placement, logical size, word count, and offset overflow are checked
before the first write. A successful fill allocates no storage or workspace
and performs no packing, materialization, transfer, synchronization, provider
selection, or fallback.

## Sparse generation facet

Request and link the sparse facet independently:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_sparse)
target_link_libraries(my_target PRIVATE ASC::random_sparse)
```

```cpp
#include <asc/random/sparse.h>
#include <asc/sparse.h>
```

`GenerateSparseUniform01<Element>` accepts an explicit serial context,
fixed-rank Core extents, a nonnegative exact count, an explicit host
`MemoryResource`, and independent structure and value address domains. The
element type is exactly unqualified `float` or `double`.

The return value is a move-only
`SparseUniform01Generation<Element, ExtentsType>` containing:

- `array`, an owning canonical `CoordinateArray`;
- `next_structure_offset`; and
- `next_value_offset`.

The structure and value `(stream, subsequence)` pairs must differ. Offsets
alone do not make equal pairs independent. Count must not exceed logical shape
size. A zero extent accepts only count zero; rank zero accepts count zero or
one.

For a nonzero count, structure generation assigns every logical ordinal a
64-bit priority from two consecutive structure words in high-word/low-word
order. Logical ordinals use lexicographic coordinates with the last dimension
varying fastest. Candidates are ordered by `(priority, ordinal)`, the
`exact_count` smallest are selected without replacement, and their coordinates
are finalized in canonical lexicographic order. The ordinal tie-break is part
of the sequence contract.

This is a deterministic pseudorandom exact-count reference algorithm, not a
claim of perfectly uniform subset sampling. Nonzero generation consumes
`2 * logical_size` structure words regardless of count; count zero consumes
none. Values are generated only after canonical finalization, in stored-entry
order. A `float` consumes one value word per entry and a `double` consumes two.
Explicit zero values are retained.

```cpp
using Shape =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

asc::HostMemoryResource resource;
auto shape = Shape::Create(4, 5);
if (!shape.ok()) {
  return 1;
}

const asc::ExecutionContext context = asc::ExecutionContext::Serial();
auto generated = asc::GenerateSparseUniform01<double>(
    context, *shape, 3, resource,
    /*structure_stream=*/11, /*structure_subsequence=*/0,
    /*structure_offset=*/4,
    /*value_stream=*/12, /*value_subsequence=*/0,
    /*value_offset=*/9);
if (!generated.ok()) {
  return 1;
}
// Twenty logical coordinates consume forty structure words.
// Three doubles consume six value words.
if (generated->next_structure_offset != 44 ||
    generated->next_value_offset != 15) {
  return 1;
}
```

Changing only the value address leaves the selected structure unchanged.
Changing only the structure address leaves the canonical value sequence
unchanged for the same shape, count, and value address, though the values may
be attached to different coordinates.

Metadata, count, address-domain, context, placement, and both offset advances
are validated before allocation. Successful generation performs exactly the
coordinate builder's coordinate and value allocations and no computational
workspace allocation. The serial selection requires
`O(exact_count * logical_size)` time and `O(rank)` local computational
storage, in addition to the builder's visible allocation and canonicalization
cost. Allocation failure publishes no owner and releases partial storage
exactly once.

## Optional CUDA Random facets

CUDA is default-off. Build asc-cpp with `ASC_CPP_ENABLE_CUDA=ON`, CUDAToolkit
12 or newer, and a caller-selected `CMAKE_CUDA_ARCHITECTURES` value. Request
only the facet needed by the consumer:

| Component/target | Public header | Exact installed component closure |
| --- | --- | --- |
| `random_cuda` / `ASC::random_cuda` | `<asc/random/providers/cuda.h>` | `core;random;core_cuda;random_cuda` |
| `random_dense_cuda` / `ASC::random_dense_cuda` | `<asc/random/providers/dense_cuda.h>` | `core;expression;dense;random;random_dense;core_cuda;random_cuda;random_dense_cuda` |
| `random_sparse_cuda` / `ASC::random_sparse_cuda` | `<asc/random/providers/sparse_cuda.h>` | `core;expression;sparse;random;random_sparse;core_cuda;random_cuda;random_sparse_cuda` |

For example:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense_cuda)
target_link_libraries(my_target PRIVATE ASC::random_dense_cuda)
```

All three targets use `ASC::core_cuda`. `ASC::random_cuda` directly links
`ASC::random` and `ASC::core_cuda`; each storage provider directly links its
provider-free storage facet, `ASC::random_cuda`, and `ASC::core_cuda`. CUDA
Runtime use is private through Core CUDA. There is no cuRAND, Thrust/CUB API
dependency, or other third-party dependency.

Requesting `random`, either provider-free storage facet, the provider-free
`cpp` aggregate, or no component does not discover CUDAToolkit or import a
CUDA target. Enabling CUDA without a usable compiler, toolkit, or runtime is a
configuration failure rather than silent provider disablement.

### Raw Philox words

`CudaFillPhilox4x32` fills a caller-owned device byte span:

```text
CudaFillPhilox4x32(context, destination, word_count,
                   stream, subsequence, offset)
    -> Result<CudaRandomWordGeneration>
```

`destination` is the owner-approved Core `MutableMemoryView`, not a typed
pointer or an SDK span. For a nonzero count, it must truthfully describe
device storage on the CUDA context's device, contain at least
`word_count * sizeof(uint32_t)` bytes, and be aligned for `uint32_t`. Extra
bytes are untouched. The view owns nothing and does not prove its declaration;
the provider validates the address before enqueue.

The result contains a move-only `completion` and `next_offset`. Logical word
`i` is bit-identical to
`GeneratePhilox4x32Word(stream, subsequence, offset + i)`. A zero count is an
exact no-op, consumes no offset, and returns an already-complete event. The
operation is `O(word_count)` and uses no allocation or workspace.

```cpp
asc::Status FillCudaWords(const asc::ExecutionContext& context,
                          asc::Buffer& device_buffer,
                          std::uint64_t word_count) {
  auto destination = device_buffer.mutable_view();
  if (!destination.ok()) {
    return destination.status();
  }
  auto generated = asc::CudaFillPhilox4x32(
      context, *destination, word_count,
      /*stream=*/7, /*subsequence=*/2, /*offset=*/9);
  if (!generated.ok()) {
    return generated.status();
  }
  auto expected = asc::AdvanceRandomOffset(9, word_count);
  if (!expected.ok() || generated->next_offset != *expected) {
    return asc::Status(asc::ErrorCode::kInvalidState,
                       "Unexpected CUDA random offset");
  }
  return generated->completion.Wait();
}
```

### Dense Uniform01

`CudaFillDenseUniform01` accepts a caller-owned unique device
`DenseView<float, Rank>` or `DenseView<double, Rank>` and returns
`CudaDenseUniform01Generation<Element, Rank>` with completion and the first
unused word offset. Ranks zero through eight, zero extents, `LayoutLeft`,
`LayoutRight`, and valid unique padded nonnegative strides are supported.

Logical coordinates vary dimension zero fastest, regardless of physical
layout or launch partition. Float consumes one word and double consumes two
adjacent words in high-word/low-word order, exactly as the provider-free
facet. Padding is neither consumed nor written. A zero logical size is an
exact no-op. The operation is `O(logical_size * rank)`, allocates no storage or
workspace, and performs no transfer, packing, or fallback.

```cpp
asc::Status FillCudaDense(const asc::ExecutionContext& context,
                          asc::DenseView<double, 2> destination) {
  auto generated = asc::CudaFillDenseUniform01(
      context, destination, /*stream=*/17, /*subsequence=*/3, /*offset=*/8);
  if (!generated.ok()) {
    return generated.status();
  }
  return generated->completion.Wait();
}
```

### Sparse exact-count Uniform01

`CudaGenerateSparseUniform01` has the same explicit extents, exact count,
structure address, value address, and checked offset contract as
`GenerateSparseUniform01`, but requires a CUDA context and a caller-owned
device resource. Success returns a move-only
`CudaSparseUniform01Generation<Element, ExtentsType>` containing:

- `array`, a `CudaCoordinateArray` owning canonical device coordinates and
  values;
- `completion`; and
- `next_structure_offset` and `next_value_offset`.

The array performs exactly two visible allocations from the supplied resource.
Its `view()` publishes trusted canonical device structure; the array owns the
bytes, while the returned view owns nothing. The resource must outlive the
array and its final deallocation.

Candidate priorities, `(priority, ordinal)` tie-breaking, word consumption,
last-dimension-fastest ordinal-to-coordinate decoding, canonical stored order,
separate structure/value domains, and value assignment are bit-identical to
the provider-free operation. Nonzero generation consumes
`2 * logical_size` structure words and one or two value words per selected
entry. Count zero consumes neither domain. Rank zero, zero extents, empty
count, and full count follow the same base contract.

The bounded low-workspace selection is
`O(logical_size * exact_count + exact_count^2 + rank * exact_count)`.
Priority selection and insertion into canonical ordinal order currently run
serially on one device thread; coordinate decoding and value generation are
parallel. The algorithm uses the output coordinate buffer transiently and has
no hidden computational workspace. This is a deterministic reference provider,
not a speedup claim; large logical domains or counts can be slow.

```cpp
asc::Status GenerateCudaSparse(const asc::ExecutionContext& context,
                               asc::MemoryResource& device_resource) {
  using Shape =
      asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
  auto shape = Shape::Create(4, 5);
  if (!shape.ok()) {
    return shape.status();
  }
  auto generated = asc::CudaGenerateSparseUniform01<double>(
      context, *shape, 3, device_resource,
      /*structure_stream=*/11, /*structure_subsequence=*/0,
      /*structure_offset=*/4,
      /*value_stream=*/12, /*value_subsequence=*/0,
      /*value_offset=*/9);
  if (!generated.ok()) {
    return generated.status();
  }
  // Keep generated->array and device_resource alive through completion.
  return generated->completion.Wait();
}
```

### Asynchronous lifetime and failure boundary

All CUDA generation calls are asynchronous on success. Until completion, keep
the Core CUDA context, every resource and owner, all views and external
destination bytes, and each returned result object alive and unmodified. A
completion event retains provider completion state, not caller storage.
Destroying it does not complete work or synchronize the device. Independent
contexts may run concurrently only when their destination storage and resource
use do not race.

Context/backend, device placement, address span/alignment, shape, stride,
logical count, domain separation, and checked offset advance are validated
before enqueue or destination mutation. Failures use `Status`/`Result` with
stable ASC codes and diagnostic provider/native details. There is no entropy,
default state, mutable pool, implicit transfer, hidden synchronization,
precision change, allocation outside the two sparse output buffers, or
fallback.

## Ownership, state, and concurrency

Counters, keys, stream addresses, and raw blocks are caller-owned values. The
engine and transforms retain no reference and allocate no result storage.
Returned words and scalars are ordinary values.

Dense generation borrows the destination view and never acquires ownership.
The destination owner and storage outlive the complete synchronous operation.
Sparse generation returns a move-only owner whose caller-provided resource
must outlive final deallocation. Input address values are never modified;
successful operations return new checked offsets.

Engine, transforms, and facet operations retain no global, thread-local, or
mutable shared state. Independent calls may run concurrently when every
destination, owner, and resource use is safe under the C++ memory model.
Writing the same or overlapping dense storage, concurrently using a
non-thread-safe resource, or moving/destroying storage during an operation
requires caller synchronization. Reusing an address intentionally reproduces
the same sequence; callers assign disjoint addresses when independent samples
are required.

## Provider and failure boundary

The provider-free facets accept exactly serial execution and host memory. An
unsupported backend reports `kUnsupported`; inaccessible or non-host storage
and resources are rejected. The CUDA facets require an explicit Core CUDA
context and exact device placement. No operation silently transfers,
synchronizes, selects a provider, falls back, changes precision, or narrows
metadata.

Every public failure is a `Status` carried directly or by `Result<T>`.
Validation and checked offset advance are transactional: dense failure leaves
the destination unchanged, and sparse failure publishes no owner. Message text
is diagnostic rather than a compatibility guarantee.

## Deliberately absent from the current product

Issue 12 is design-only, so the current Random product still provides no:

- entropy acquisition, seed facility, global or thread-local engine;
- mutable engine, default engine, pool, or implicit advancing state;
- normal, rejection, integer-range, affine, or user-defined distribution;
- density/Bernoulli sparse mode or statistically uniform subset claim;
- compressed sparse output, duplicate combination, densification, or hidden
  coordinate conversion;
- shared fill header or a base Random dependency on Dense or Sparse;
- Sobol implementation or direction data;
- serialized engine state or object-layout persistence;
- optimized CPU provider, OpenMP, TBB, Eigen, BLAS/LAPACK, oneMKL, or
  third-party dependency;
- cuRAND, provider-native public type, hidden GPU workspace, or additional
  GPU distribution; or
- HIP, SYCL, or another GPU provider.

Final local Milestone 7 evidence classifies each of `random_cuda`,
`random_dense_cuda`, and `random_sparse_cuda` as **configure-tested**,
**compile-tested**, **runtime-tested**, and **parity-tested** on the recorded
RTX 3060 environment. Exact commands, versions, counts, sanitizer/package
status, and hardware details belong to Publication Checkpoint B. Documentation
and compilation alone do not establish runtime or parity evidence.

The [frozen Milestone 7 contract][m7-contract] is authoritative for the CUDA
provider surface and deferred work. The [Milestone 5 contract][m5-contract]
remains authoritative for provider-free storage generation.

[provenance]: ../development/asc-cpp-m2-independent-foundations/provenance-record.md
[m5-contract]: ../development/asc-cpp-m5-random-storage-generation/milestone-contract.md
[m7-contract]: ../development/asc-cpp-m7-gpu-sparse-random/milestone-contract.md
[random-contract]: ../development/asc-cpp-architecture/decisions/0020-random-contract.md
[random-crosswalk]: ../random-crosswalk.md
