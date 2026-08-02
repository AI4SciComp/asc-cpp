# Random module

`ASC::random` provides explicit seed acquisition, four versioned stateful
engines, generic engine/distribution composition, unbiased uniform integer
generation, half-open uniform real generation, scalar Box-Muller normal
generation, storage-neutral Latin/Halton/Hammersley/Sobol QMC, reproducible
Philox4x32-10 words, and exact raw-word unit transforms. Its Dense and Sparse
storage-generation facets are separately consumable. The base remains a
compiled library with one direct ASC dependency, `ASC::core`, and no external
dependency.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random)
target_link_libraries(my_target PRIVATE ASC::random)
```

```cpp
#include <asc/random.h>
```

The umbrella includes `<asc/random/engine.h>`,
`<asc/random/distribution.h>`, `<asc/random/generator.h>`,
`<asc/random/quasi.h>`, and `<asc/random/seed.h>`. Base Random does not include
Utilities, Expression, Dense, Sparse, a provider header, or a storage view.

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
Random123 implementation, or upstream test-vector corpus is an implementation
input.

Issue 13 implements rows `RND-001` and `RND-003` through `RND-010` from the
generated [33-row architecture and provenance crosswalk][random-crosswalk].
SplitMix64 and both xoroshiro engines are original asc-cpp expressions derived
from the exact pinned public-domain Blackman/Vigna artifacts. PCG32 is an
original expression of the pinned Apache-2.0 PCG minimal-C 0.9 XSH-RR
contract. The integer rejection mapping, real transform, value-composition
API, and Box-Muller implementation are independently authored from the
accepted [Random contract][random-contract]. No MdeCpp source, test, vector,
benchmark, data, or prose was copied or mechanically translated.

Issue 14 implements the storage-neutral portions of `RND-011` through
`RND-014` and `RND-018` through `RND-022`. Prime, radical-inverse,
permutation, Latin, Halton, Hammersley, Sobol recurrence, tests, and benchmark
expressions are original work from the frozen mathematical contract. The only
import is the exact Joe--Kuo D(6) direction-number dataset through dimension
21201 under its retained BSD-3-Clause license. Its input and license hashes,
original offline generator, generated output, compiled direction words,
installed artifacts, and `THIRD_PARTY_NOTICES` are checked independently. No
Joe--Kuo program is copied, and no MdeCpp/Burkardt source, test, converter, or
data is copied.

Issue 15 completes `RND-017` through `RND-021` and `RND-026` through
`RND-028`. Generic and QMC Dense fills and all Sparse adapter code are
original asc-cpp compositions over the approved engine, view, and priority
contracts. Multivariate normal uses an independently written Cholesky and
Box--Muller composition; unit-sphere sampling uses the published
normalized-Gaussian method. The GPL-covered MdeCpp implementations, tests,
fixtures, literal vectors, benchmarks, data, and prose remain prohibited and
were not used.

The Issue 13 and 14 base additions are portable serial CPU operations that
accept no execution context or storage. Issue 15 storage adapters accept an
explicit context and reject every non-serial backend. No new CUDA
implementation or CPU/GPU bit-parity claim is declared. Existing Philox and
`Uniform01` CPU/CUDA behavior is unchanged. No added operation hides an
allocation, transfer, synchronization, provider selection, or fallback.

Philox4x32-10 identity, lane mapping, stream/subsequence/offset mapping, and
the scalar transform rules are exact pre-1.0 sequence API first published for
ASCCpp 0.2.x. Milestone 5 extends that sequence contract with exact dense
logical-coordinate mapping and deterministic sparse structure/value mapping.
These guarantees are narrower than statistical suitability for a particular
scientific application and do not imply provider parity.

## Storage-neutral QMC contract

`float` and `double` QMC APIs write caller-owned spans. They own no storage,
perform no hidden allocation, and use no global or thread-local prime,
permutation, or direction cache.

- `PrimeAt(d)` enumerates the zero-based prime, with `PrimeAt(0) == 2`, inside
  the 21201-dimension QMC domain. `RadicalInverse<Real>` applies the frozen
  least-significant-digit/FMA order to a `uint64_t` index and base at least
  two. `ScrambledRadicalInverse` additionally validates a complete immutable
  digit permutation whose zero digit maps to zero.
- `GenerateLowDiscrepancyPermutation` performs unbiased Fisher--Yates into a
  writable `uint32_t` span using an explicit canonical engine. No singleton
  permutation exists.
- Latin midpoint and jittered generation accept sample and dimension counts,
  an explicit engine, a `sample_count * dimension_count` permutation
  workspace, and an equally sized sample-major output. Dimensions shuffle in
  increasing order; each shuffle visits limits from `n` down to 2. Jitter is
  then consumed sample-major, dimension zero first. Empty logical output
  consumes no engine state. Every dimension contains one coordinate in every
  half-open stratum.
- Halton coordinate `d` uses the `d`th prime and unsigned indexed radical
  inverse; scrambling requires explicit per-coordinate digit permutations.
  Hammersley requires a positive total count and index below it, writes
  `index / total_count` first, and uses successive prime radical inverses for
  later coordinates.
- Sobol computes `g = index ^ (index >> 1)` and XORs the selected 64 direction
  words. Float returns the high 24 bits times `2^-24`; double returns the high
  53 bits times `2^-53`. Dimensions are zero-based in `[0, 21201)`. The
  checked-in compact table is compiled into `ASC::random`; runtime evaluation
  never opens the installed data file.

`SobolSequence` stores only dimension count, current `uint64_t` index, and an
exhaustion flag. `Next` equals indexed evaluation at the current index and
then advances once. `Reset`, `Skip`, and `SkipTo` mutate the explicit index
without replay; overflow or exhaustion fails before an output write. Indexed
evaluation and immutable direction-table reads are safe for concurrent use.
Concurrent mutation of one `SobolSequence`, one Latin engine, or one writable
workspace is not safe; independent values and disjoint spans may run
concurrently.

Indexed Halton, Hammersley, and Sobol have no seed, stream, or mutable state.
Latin reproducibility is defined by QMC sequence version 1 plus engine type,
engine sequence version/state, scalar type, shape, midpoint/jitter choice, and
the documented consumption order. The QMC functions are portable serial CPU
operations only: they accept no execution context, make no CPU/GPU parity
claim, and never transfer, synchronize, select a provider, or fall back. The
existing Philox/Uniform01 CUDA contract is unchanged.

Shape, dimension, index, parameter, permutation, workspace, and overlap
validation completes before the first engine call or destination write.
Valid arithmetic uses the destination type and repairs a rounded upper
endpoint toward zero; any other nonfinite or out-of-range result is
`kNumerical`, with earlier completed output and documented engine consumption
remaining observable. Prime enumeration uses constant auxiliary storage;
radical inverse is logarithmic in the index; permutation is linear; and a
Sobol point reads at most 64 direction words per coordinate.

The raw Joe--Kuo input and license install below
`share/doc/ASCCpp/random`, while `THIRD_PARTY_NOTICES` installs in
`share/doc/ASCCpp`. Builds and consumers need neither those runtime files nor
Python. See the [storage-neutral QMC example][qmc-example].

## Issue 15 advanced adapter contract

Issue 15 adds only free functions to `ASC::random_dense` and
`ASC::random_sparse`; base `ASC::random` remains storage-neutral. There is no
sampler hierarchy, virtual dispatch, raw generator owner, new target edge,
factorization dependency, or storage customization protocol. All destinations
and workspaces are explicit non-owning views or spans.

`FillDensePseudo` invokes a caller-owned generator once per logical element,
with dimension zero varying fastest independent of physical layout. The
`FillDenseLatinHypercubeMidpoints` and `FillDenseLatinHypercubeJittered`
adapters use a caller-owned `uint32_t` Dense permutation workspace matching
the output shape. `FillDenseHalton`, `FillDenseScrambledHalton`,
`FillDenseHammersley`, and `FillDenseSobol` use a disjoint one-point Dense
workspace. Their logical shape is `[sample_count, dimension_count]`; dimension
zero is generated first inside each sample, while padding is untouched.

`PrepareDenseMultivariateNormal` requires finite mean, finite square
covariance, exact scalar symmetry, matching output-factor shape, and strict
positive definiteness. It writes a lower Cholesky factor to caller storage and
zeros the upper triangle. Invalid shape, placement, aliasing, mean, covariance,
or symmetry leaves the factor unchanged. An SPD failure is `kNumerical` and
may leave the explicitly designated factor workspace partially updated; no
prepared result is published. Factorization uses `O(d^3)` work and the
caller-owned `O(d^2)` factor, with no hidden factorization or BLAS dependency.

`FillDenseMultivariateNormal` requires that prepared factor, a finite mean, a
standard `NormalGenerator` (mean zero, deviation one), a
`[sample_count,d]` destination, and a disjoint `d`-element workspace. It draws
standard normals in increasing dimension order and evaluates `mean + L*z` in
the written FMA order. The transform is version 1. Engine type/version/state,
normal-transform version, scalar type, mean/factor values, sample count, and
logical order determine reproducibility. Transcendentals repeat on the same
supported math ABI but are not promised bit-identical across libm or CPU/GPU.

`FillDenseUnitSphere` also uses a `d`-element workspace. For `d == 1`, each
sample consumes one canonical 32-bit request and maps its high bit to `-1` or
`+1`. For `d > 1`, each version-1 attempt draws `d` standard normals in
dimension order and accepts only a finite nonzero norm. At most
`kUnitSphereMaximumAttemptsVersion1 == 64` attempts are made per sample.
Attempt exhaustion is `kNumerical`. A completed sample is normalized and
published as one unit; on failure, earlier samples and engine consumption are
observable, the failing sample is untouched, and later samples are untouched.
Each attempt is `O(d)` with only caller workspace.

Every Issue 15 Dense operation is synchronous, serial CPU, and host-only. It
does not allocate, pack, transfer, synchronize, or fall back. Context,
placement, shape, parameter, index/offset, workspace, and alias validation
precedes random consumption and destination mutation. Stateful engines and
generators are not safe for concurrent mutation; immutable factors/QMC data,
independent engine copies, disjoint destinations, and separate workspaces are
safe under the ordinary C++ memory model. See the [advanced adapter
example][advanced-example].

## Explicit nondeterministic seed acquisition

`AcquireNondeterministicSeed` is the only nondeterministic seed operation. It
borrows a caller-owned `std::random_device`; asc-cpp never constructs one and
never invokes it from an engine, distribution, generator, reset, or default
constructor. Version 1 accepts only a source with `min() == 0` and
`max() == UINT32_MAX`. An unsupported range returns `kUnsupported` before a
draw. A supported source is called exactly twice, high word then low word, and
the result is `(uint64_t(high) << 32) | low`.

```cpp
std::random_device source;
auto seed = asc::AcquireNondeterministicSeed(source);
if (!seed.ok()) {
  return 1;
}
asc::Pcg32 engine(*seed, /*stream=*/7);
```

A standard-library acquisition exception becomes `kUnavailable` and no seed
is published. Calls already made by the external source cannot be rolled back.
The diagnostic contains operation context but no acquired word. Time-based
seeding is not provided. Tests and scientific examples use fixed literals.

## Stateful engine contract

All engines are non-cryptographic, copyable value types with full unsigned
result domains and no default constructor or hidden state.

| Engine | Result and state | Explicit construction | Partition support |
| --- | --- | --- | --- |
| `SplitMix64` | `uint64_t`; one `uint64_t` state | one `uint64_t` seed | no skip/jump API |
| `Pcg32` | `uint32_t`; `uint64_t` state plus odd encoded increment | seed with stream zero, or initial state plus stream; only the low 63 stream bits select a sequence | no Issue 13 advance API |
| `Xoroshiro64Star` | `uint32_t`; two nonzero-together `uint32_t` words | one `uint64_t` seed expanded by two SplitMix64 outputs, narrowed to their low words | fixed version-1 `Jump` and `LongJump` |
| `Xoroshiro128Plus` | `uint64_t`; two nonzero-together `uint64_t` words | one `uint64_t` seed expanded by two complete SplitMix64 outputs | fixed version-1 `Jump` and `LongJump` |

The algorithm identities are SplitMix64 fixed-increment 2015, PCG32 XSH-RR
set-sequence 0.9, xoroshiro64* 1.0, and xoroshiro128+ 1.0 with parameters
24, 16, and 37. `Pcg32(initial_state, stream)` follows the official
set-sequence initialization; its public exported state stores the already
encoded odd increment.

```cpp
asc::Xoroshiro128Plus engine(/*seed=*/0);
const std::uint64_t first = engine();
const asc::Xoroshiro128PlusState checkpoint = engine.ExportState();

auto restored = asc::Xoroshiro128Plus::FromState(checkpoint);
if (!restored.ok() || (*restored)() != engine()) {
  return 1;
}
```

Every public state struct carries `sequence_version`. Version 1 is
`kRandomSequenceVersion1`. `FromState` and `RestoreState` reject an unknown
tag with `kVersion`; PCG rejects an even increment and xoroshiro rejects an
all-zero state with `kInvalidState`. `RestoreState` validates before changing
the engine. No byte-stream or object-layout serialization format is defined.

A copy continues with the same sequence independently. A move is equivalent
to a copy for these trivially copyable states. Concurrent mutation of one
engine is a data race; separate engine values, immutable exported states, and
the existing pure Philox functions may be used concurrently.

## Generator and distribution contract

`Generator<Engine, Distribution>` owns its caller-selected engine and valid
distribution by value. `UniformGenerator<Engine, Value>` and
`NormalGenerator<Engine, Real>` are transparent aliases. There is no virtual
base, seed provider, global pool, allocation, spare-normal cache, or reference
to caller storage.

Distribution factories validate parameters before a generator exists or an
engine can be consumed:

- `UniformIntegerDistribution<Integer>::Create(lower, upper)` supports
  non-Boolean 8-, 16-, 32-, and 64-bit integral types and the closed interval
  `[lower, upper]`. It assembles a candidate with the result width, handles a
  full-width signed or unsigned interval explicitly, and uses threshold
  rejection rather than biased modulo-only mapping.
- `UniformRealDistribution<Real>::Create(lower, upper)` supports `float` and
  `double` on finite strictly ordered bounds with finite span. It returns
  `[lower, upper)`, uses a 24- or 53-leading-bit unit value, evaluates
  `fma(upper - lower, unit, lower)`, and repairs a rounded upper endpoint with
  `nextafter` toward the lower bound.
- `NormalDistribution<Real>::Create(mean, standard_deviation)` requires a
  finite mean and finite positive deviation. Version 1 consumes exactly two
  unit values, computes `u_radius = 1 - U1`, `u_angle = U2`, and returns only
  the cosine Box-Muller result. It has no cached sine spare.

For an approved 64-bit engine, one float or double unit value consumes one
engine call. For an approved 32-bit engine, float consumes one call and double
consumes two. Complete engine results are concatenated in call order from high
to low; unused low bits are discarded and never cached. Engine type is thus
part of the sequence contract.

```cpp
auto distribution = asc::UniformRealDistribution<double>::Create(-2.0, 3.0);
if (!distribution.ok()) {
  return 1;
}
asc::UniformGenerator<asc::Pcg32, double> generator(
    asc::Pcg32(/*initial_state=*/42, /*stream=*/54), *distribution);
auto value = generator();
if (!value.ok()) {
  return 1;
}
```

Invalid parameters return `kInvalidArgument` before consumption. A valid real
or normal transform that produces a nonfinite or out-of-range value returns
`kNumerical` after its documented draws and publishes no value; engine state
cannot be rolled back. Normal transcendental results repeat for the same
supported math ABI but are not promised bit-identical across different libm
implementations or between CPU and GPU.

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

The same facet owns `FillDensePseudo`, prepared multivariate-normal and
unit-sphere sampling, and all Dense QMC fills described in the Issue 15
contract above. Those APIs preserve the same host-only placement, logical
layout invariance, explicit-workspace, and no-hidden-allocation boundary.

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

`FillSparseUniform01` overloads accept mutable canonical `CoordinateView` and
rank-two `CompressedSparseView` (CSR or CSC). They write values only in stored
order and return the first unused Philox value offset. Coordinates, offsets,
indices, entry count, and order never change; generated zeros remain explicit.
`FillSparsePseudo` provides the same structure-preserving operation for an
explicit generator returning the exact stored scalar type. Both operations
are `O(nnz)`, allocate nothing, and may leave earlier values and consumed
generator state observable if a later generator result fails.

`GenerateSparseStructure` separates structure selection from value generation.
It accepts fixed-rank extents, an exact count, one Philox address, a
`logical_size`-element `SparseRandomStructureCandidate` workspace for nonzero
count, and an `exact_count`-element `uint64_t` output span. The spans must be
disjoint. It assigns the same version-1 `(priority, ordinal)` keys used by
combined generation, selects the exact smallest set in average `O(n)` work,
and writes the selected ordinals in strictly increasing canonical order. It
allocates no owner and consumes no value domain. Count zero requires empty
spans and consumes no structure words; nonzero count addresses exactly `2*n`
structure words regardless of selected count. Workspace contents after return
are unspecified.

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

Value-only, structure-only, and combined generation are separate contracts.
The value-only operations cannot alter structure; structure-only generation
cannot consume or publish values; combined generation remains the only one
that allocates a Sparse owner through an explicit `MemoryResource`.

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
Advanced Dense preparation/sampling additionally borrows every factor, mean,
permutation, point, or sample workspace named by the signature. The
destination owner and all borrowed storage outlive the complete synchronous
operation.
Sparse generation returns a move-only owner whose caller-provided resource
must outlive final deallocation. Input address values are never modified;
successful operations return new checked offsets.

Philox, raw-word transforms, and facet operations retain no global,
thread-local, or mutable shared state. The added stateful engines and
generators own only their explicit value state and are not safe for concurrent
mutation. Independent copies may run concurrently when every destination,
owner, and resource use is safe under the C++ memory model.
Writing the same or overlapping dense storage, concurrently using a
non-thread-safe resource, or moving/destroying storage during an operation
requires caller synchronization. Reusing an address intentionally reproduces
the same sequence; callers assign disjoint addresses when independent samples
are required.

## Provider and failure boundary

The scalar engines, distributions, and generators are CPU-only and
storage-neutral; they accept neither an execution context nor a memory view.
Provider-free storage facets accept exactly serial execution and host memory.
An unsupported storage backend reports `kUnsupported`; inaccessible or
non-host storage and resources are rejected. The CUDA facets require an
explicit Core CUDA context and exact device placement. No operation silently
transfers, synchronizes, selects a provider, falls back, changes precision, or
narrows metadata.

Every public failure is a `Status` carried directly or by `Result<T>`.
Validation and checked offset advance are transactional: dense failure leaves
the destination unchanged until generation begins, and sparse combined failure
publishes no owner. A data-dependent generator or numerical failure preserves
completed outputs and consumed state, does not publish the failing logical
output, and leaves later outputs untouched. Cholesky SPD failure may modify
only the explicitly designated factor workspace. Message text is diagnostic
rather than a compatibility guarantee.

## Deliberately absent from the current product

Issues 13 through 15 deliberately provide no:

- time-based or implicit seed acquisition, default engine, global or
  thread-local engine, or mutable pool;
- arbitrary jump-polynomial machinery or byte serialization;
- user-defined distribution registration or cached normal spare;
- density/Bernoulli sparse mode or statistically uniform subset claim;
- compressed sparse output, duplicate combination, densification, or hidden
  coordinate conversion;
- shared fill header or a base Random dependency on Dense or Sparse;
- sampler base class, virtual generator owner, new storage adapter protocol,
  or implicit result owner;
- multivariate-normal or sphere GPU implementation;
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
[qmc-example]: ../examples/random-qmc.md
[advanced-example]: ../examples/random-advanced.md
