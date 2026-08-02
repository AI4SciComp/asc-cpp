# Advanced Random storage adapters

The advanced portable-CPU samplers live in the existing Random-owned storage
facets. Request only the facets used by the application:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED
             COMPONENTS random_dense random_sparse)
target_link_libraries(my_sampler PRIVATE
                      ASC::random_dense ASC::random_sparse)
```

This complete example prepares a two-dimensional covariance in caller-owned
Dense storage and generates four multivariate-normal samples:

```cpp
#include <array>

#include <asc/core.h>
#include <asc/dense.h>
#include <asc/random/dense.h>
#include <asc/random/generator.h>

constexpr std::array<asc::extent_t, 1> kVectorShape{2};
constexpr std::array<asc::extent_t, 2> kMatrixShape{2, 2};
constexpr std::array<asc::extent_t, 2> kSampleShape{4, 2};

auto vector_layout = asc::DenseLayout<1>::Create(kVectorShape);
auto matrix_layout = asc::DenseLayout<2>::Create(kMatrixShape,
                                                 asc::LayoutRight{});
auto sample_layout = asc::DenseLayout<2>::Create(kSampleShape,
                                                 asc::LayoutRight{});
if (!vector_layout.ok() || !matrix_layout.ok() || !sample_layout.ok()) {
  return 1;
}

std::array<double, 2> mean_values{2.0, -1.0};
std::array<double, 4> covariance_values{4.0, 2.0, 2.0, 3.0};
std::array<double, 4> factor_values{};
std::array<double, 8> sample_values{};
std::array<double, 2> workspace_values{};

auto mutable_mean = asc::DenseView<double, 1>::Create(
    mean_values.data(), *vector_layout, asc::MemorySpace::kHost);
auto mutable_covariance = asc::DenseView<double, 2>::Create(
    covariance_values.data(), *matrix_layout, asc::MemorySpace::kHost);
auto factor = asc::DenseView<double, 2>::Create(
    factor_values.data(), *matrix_layout, asc::MemorySpace::kHost);
auto samples = asc::DenseView<double, 2>::Create(
    sample_values.data(), *sample_layout, asc::MemorySpace::kHost);
auto workspace = asc::DenseView<double, 1>::Create(
    workspace_values.data(), *vector_layout, asc::MemorySpace::kHost);
if (!mutable_mean.ok() || !mutable_covariance.ok() || !factor.ok() ||
    !samples.ok() || !workspace.ok()) {
  return 1;
}

asc::DenseView<const double, 1> mean = *mutable_mean;
asc::DenseView<const double, 2> covariance = *mutable_covariance;
const asc::ExecutionContext context = asc::ExecutionContext::Serial();
if (!asc::PrepareDenseMultivariateNormal(context, mean, covariance, *factor)
         .ok()) {
  return 1;
}

auto standard_normal = asc::NormalDistribution<double>::Create(0.0, 1.0);
if (!standard_normal.ok()) {
  return 1;
}
asc::NormalGenerator<asc::Pcg32, double> generator(
    asc::Pcg32(/*initial_state=*/42, /*stream=*/54), *standard_normal);
asc::DenseView<const double, 2> prepared_factor = *factor;
if (!asc::FillDenseMultivariateNormal(context, *samples, mean,
                                      prepared_factor, generator, *workspace)
         .ok()) {
  return 1;
}
```

Preparation checks finite mean/covariance, exact symmetry, matching shapes,
and positive definiteness. It writes a lower Cholesky factor only to the
explicit factor view; an indefinite covariance returns `kNumerical` and may
leave that factor workspace partially updated. Sampling requires a standard
normal generator and a disjoint dimension-sized workspace. Each completed
sample is published as one unit. On a later numerical failure, earlier samples
and consumed engine state remain observable, the failing sample is not
published, and later samples are untouched.

Structure-only Sparse generation uses one candidate record per logical
coordinate and writes exactly the requested canonical ordinals:

```cpp
#include <array>

#include <asc/core.h>
#include <asc/random/sparse.h>

using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;
auto shape = Shape::Create(4, 5);
if (!shape.ok()) {
  return 1;
}

std::array<asc::SparseRandomStructureCandidate, 20> candidates{};
std::array<std::uint64_t, 3> ordinals{};
auto next = asc::GenerateSparseStructure(
    asc::ExecutionContext::Serial(), *shape, /*exact_count=*/3,
    /*stream=*/11, /*subsequence=*/0, /*offset=*/4, candidates, ordinals);
if (!next.ok() || *next != 44) {
  return 1;
}
```

The selected ordinals are strictly increasing and unique. Nonzero generation
addresses two Philox words per logical coordinate, so the next offset is
independent of the selected count. The operation allocates no owner or hidden
workspace; both spans must be caller-owned, disjoint, and host-accessible.
Use `FillSparseUniform01` or `FillSparsePseudo` to change only values in an
existing canonical coordinate/CSR/CSC view, or `GenerateSparseUniform01` when
an explicitly allocated coordinate owner is required.

All Issue 15 adapters require a serial context and host views. A CUDA context
returns `kUnsupported` before access, mutation, random consumption,
allocation, transfer, or synchronization. Engines and generators are mutable
value state and are not safe for concurrent mutation; independent engine
copies, disjoint destinations, immutable prepared inputs, and separate
workspaces may be used concurrently. Sequence version 1 fixes logical order,
normal composition, unit-sphere attempts, QMC index mapping, sparse priority
ordering, and stored-value order; a mapping change requires a new named
version.
