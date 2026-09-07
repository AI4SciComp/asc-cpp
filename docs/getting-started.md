# Getting started in a new project

This tutorial builds a small C++20 application against an installed ASCCpp
0.9.0 package. The application creates a row-major dense view, fills it with a
reproducible uniform random sequence, and prints the values and next random
offset.

ASCCpp has six provider-free modules: Core, Utilities, Expression, Dense,
Sparse, and Random. Dense and Sparse random-storage adapters are exposed as
separate package components. Recoverable failures are returned as `Status` or
`Result<T>`; the library does not use exceptions for ordinary error handling.

## 1. Install ASCCpp

ASCCpp requires CMake 3.25 or newer, a C++20 compiler, and exactly
`ASCCMake 0.1.0`. Prepare the dependency at the commit and checksum recorded in
[Installation and immutable dependencies](installation.md). From the ASCCpp
source directory, a provider-free release build can then be installed with:

```bash
cmake --preset test-release \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE=/path/to/asc-cmake
cmake --build --preset test-release --parallel 2
ctest --preset test-release --no-tests=error --output-on-failure
cmake --install ../asc-cpp-build/test-release \
  --prefix /tmp/asccpp-prefix
```

The test step is intentional: it verifies the same package configuration that
will be installed. Replace `/tmp/asccpp-prefix` with a durable prefix when
installing for regular use.

## 2. Create the consumer project

Create this directory structure outside the ASCCpp source tree:

```text
asccpp-hello/
├── CMakeLists.txt
└── main.cc
```

Use the following `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)

project(asccpp_hello LANGUAGES CXX)

include(CTest)

find_package(
  ASCCpp 0.9.0 EXACT CONFIG REQUIRED
  COMPONENTS random_dense
)

add_executable(asccpp_hello main.cc)
target_compile_features(asccpp_hello PRIVATE cxx_std_20)
set_target_properties(asccpp_hello PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(asccpp_hello PRIVATE ASC::random_dense)

if(BUILD_TESTING)
  add_test(NAME asccpp_hello.runs COMMAND asccpp_hello)
endif()
```

Requesting `random_dense` is sufficient here. Its imported target brings the
provider-free Random, Dense, Expression, and Core dependencies required by
that facet. It does not bring Sparse or a CUDA provider into the application.

Add this `main.cc`:

```cpp
#include <array>
#include <iostream>

#include <asc/core/execution.h>
#include <asc/core/memory.h>
#include <asc/core/result.h>
#include <asc/core/status.h>
#include <asc/core/types.h>
#include <asc/dense/layout.h>
#include <asc/dense/view.h>
#include <asc/random/dense.h>

int main() {
  constexpr std::array<asc::extent_t, 2> kShape{2, 3};
  auto layout = asc::DenseLayout<2>::Create(kShape, asc::LayoutRight{});
  if (!layout.ok()) {
    std::cerr << "layout creation failed: " << layout.status().ToString()
              << '\n';
    return 1;
  }

  std::array<float, 6> values{};
  auto view = asc::DenseView<float, 2>::Create(
      values.data(), *layout, asc::MemorySpace::kHost);
  if (!view.ok()) {
    std::cerr << "view creation failed: " << view.status().ToString() << '\n';
    return 2;
  }

  auto next_offset = asc::FillDenseUniform01(
      asc::ExecutionContext::Serial(), *view, 42, 7, 0);
  if (!next_offset.ok()) {
    std::cerr << "random fill failed: "
              << next_offset.status().ToString() << '\n';
    return 3;
  }

  std::cout << "generated values:";
  for (float value : values) {
    std::cout << ' ' << value;
  }
  std::cout << "\nnext random offset: " << *next_offset << '\n';
  return *next_offset == 6 ? 0 : 4;
}
```

The stream (`42`), subsequence (`7`), starting offset (`0`), scalar type, and
shape identify this generated sequence. A `float` consumes one random word per
element, so filling six elements returns the next offset `6`.

## 3. Configure, build, and run

From `asccpp-hello`, configure an out-of-source build and point CMake at the
installation prefix:

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/tmp/asccpp-prefix \
  -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --no-tests=error --output-on-failure
./build/asccpp_hello
```

On a multi-configuration generator, add `--config Release` to the build and
test commands and run the executable from the generator's Release output
directory.

If CMake cannot find ASCCpp, either correct `CMAKE_PREFIX_PATH` or point
directly at the package configuration directory:

```bash
cmake -S . -B build \
  -DASCCpp_DIR=/tmp/asccpp-prefix/lib/cmake/ASCCpp
```

Do not add the ASCCpp source directory with `add_subdirectory` in an installed
package consumer. Using the exported targets preserves component isolation,
required compile features, and platform-specific link settings.

## 4. Understand the API pattern

ASCCpp constructors that validate metadata commonly use named `Create`
functions and return `Result<T>`. Check `ok()` before dereferencing the result:

```cpp
auto result = SomeCheckedOperation();
if (!result.ok()) {
  std::cerr << result.status().ToString() << '\n';
  return 1;
}
UseValue(*result);
```

Operations with no result value return `Status`:

```cpp
asc::Status status = SomeOperation();
if (!status.ok()) {
  std::cerr << status.ToString() << '\n';
  return 1;
}
```

Views are non-owning. In the example, `values` must outlive `view`. Owning
dense and sparse containers instead allocate through an explicit
`MemoryResource`; that resource must outlive the owner and its deallocation.
Execution and memory placement are also explicit. The provider-free CPU
operations in this tutorial use `ExecutionContext::Serial()` and host memory.

## 5. Select only the components you use

Use the smallest component set that provides the needed API:

| Component | Imported target | Use it for |
| --- | --- | --- |
| `core` | `ASC::core` | status/result, types, memory, execution, configuration, and I/O |
| `utilities` | `ASC::utilities` | command-line parsing and timers |
| `expression` | `ASC::expression` | storage-neutral expression customization |
| `dense` | `ASC::dense` | dense arrays/views, evaluation, reductions, and Dense BLAS |
| `sparse` | `ASC::sparse` | coordinate/CSR/CSC storage, conversion, evaluation, and Sparse BLAS |
| `random` | `ASC::random` | engines, distributions, seeds, and storage-neutral QMC |
| `random_dense` | `ASC::random_dense` | deterministic random fills for dense views |
| `random_sparse` | `ASC::random_sparse` | deterministic sparse structure and value generation |
| `cpp` | `ASC::cpp` | all supported provider-free components |

For example, a Dense BLAS-only application should request `COMPONENTS dense`
and link `ASC::dense`; it does not need `ASC::cpp`. CUDA components and targets
have a `_cuda` suffix and are experimental in 0.9.0. See the
[support matrix](support-matrix.md) before enabling them.

## 6. Continue with the installed examples

The repository contains compiled examples for Core, Dense BLAS, Sparse
conversion/BLAS, Random/QMC, random storage, and experimental CUDA completion.
Build all provider-free examples against the installed package with:

```bash
cmake -S /path/to/asc-cpp/examples -B /tmp/asccpp-examples \
  -DCMAKE_PREFIX_PATH=/tmp/asccpp-prefix \
  -DBUILD_TESTING=ON
cmake --build /tmp/asccpp-examples --parallel 2
ctest --test-dir /tmp/asccpp-examples \
  --no-tests=error --output-on-failure
```

For deeper API semantics, continue with the module guides for
[Core](modules/core.md), [Dense](modules/dense.md),
[Sparse](modules/sparse.md), and [Random](modules/random.md).
