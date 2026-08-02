# Storage-neutral quasi-random points

The `ASC::random` component exposes QMC algorithms through caller-owned spans.
No Dense or Sparse object is required, and no operation allocates a result.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random)
target_link_libraries(my_qmc PRIVATE ASC::random)
```

Generate one indexed Sobol point directly:

```cpp
#include <array>

#include <asc/random.h>

std::array<double, 3> point{};
const asc::Status status = asc::GenerateSobolPoint<double>(42, point);
if (!status.ok()) {
  return 1;
}
```

`SobolSequence` stores only the dimension, current unsigned 64-bit index, and
exhaustion flag. `Reset`, `Skip`, and `SkipTo` change that explicit index; they
do not replay earlier points or allocate a cache.

```cpp
auto sequence = asc::SobolSequence::Create(/*dimension_count=*/3);
if (!sequence.ok()) {
  return 1;
}
std::array<float, 3> first{};
if (!sequence->Next<float>(first).ok()) {
  return 1;
}
```

Latin-hypercube generation consumes an explicit engine and an explicit
permutation workspace. Output is sample-major with dimension zero first.

```cpp
constexpr std::size_t kSamples = 8;
constexpr std::size_t kDimensions = 3;
asc::SplitMix64 engine(/*seed=*/2026);
std::array<std::uint32_t, kSamples * kDimensions> permutations{};
std::array<double, kSamples * kDimensions> samples{};

const asc::Status latin = asc::GenerateLatinHypercubeJittered<double>(
    kSamples, kDimensions, engine, permutations, samples);
if (!latin.ok()) {
  return 1;
}
```

For a fixed engine type/state, scalar type, shape, and sequence version, Latin
permutations and jitter repeat exactly on a supported CPU math ABI. Indexed
Halton, Hammersley, and Sobol evaluation has no seed or mutable state. These
Issue 14 operations are portable serial CPU APIs only; they do not accept a
CUDA context and do not transfer, synchronize, allocate, or fall back.
Issue 15 maps the same point definitions into caller-owned Dense views through
the `random_dense` facet; see the [advanced adapter example](random-advanced.md)
for the explicit context and workspace boundary.
