#include <sys/mman.h>
#include <unistd.h>

#include <complex>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <new>  // IWYU pragma: keep; nonallocating placement array new.

#include "allocation_probe.h"
#include "asc/core/memory.h"
#include "asc/dense/blas.h"
#include "asc/dense/lapack/triangular_band_view.h"
#include "test_support.h"

namespace {

using asc_dense_test::TestContext;
using Layout = asc::DenseBlasLayout;
using Triangle = asc::DenseBlasTriangle;

template <typename T>
void TestNoCoefficientReads(TestContext& test) {
  // Linux diagnostic on an actual mapping containing a live C++20 array.
  // The recorded test platform is explicit; there is no fabricated capacity.
  const auto page_size = sysconf(_SC_PAGESIZE);
  ASC_DENSE_TEST_CHECK(test, page_size > 0);
  if (page_size <= 0) {
    return;
  }
  const auto bytes = static_cast<std::size_t>(page_size);
  void* memory = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASC_DENSE_TEST_CHECK(test, memory != MAP_FAILED);
  if (memory == MAP_FAILED) {
    return;
  }
  auto* data = ::new (memory) T[8];
  for (std::size_t i = 0; i < 8; ++i) {
    data[i] = T{17};
  }
  const int protected_result = mprotect(memory, bytes, PROT_NONE);
  ASC_DENSE_TEST_EQ(test, protected_result, 0);
  if (protected_result == 0) {
    std::size_t allocations = 0;
    {
      asc_dense_test::AllocationProbe probe;
      for (auto layout : {Layout::kColumnMajor, Layout::kRowMajor}) {
        for (auto triangle : {Triangle::kUpper, Triangle::kLower}) {
          const auto view = asc::LapackTriangularBandView<T>::Create(
              data, 2, 2, triangle, layout, 3,
              {data, 8 * sizeof(T), asc::MemorySpace::kHost});
          ASC_DENSE_TEST_CHECK(test, view.ok());
          const auto immutable = asc::LapackTriangularBandView<const T>::Create(
              data, 2, 2, triangle, layout, 3,
              {data, 8 * sizeof(T), asc::MemorySpace::kHost});
          ASC_DENSE_TEST_CHECK(test, immutable.ok());
        }
      }
      allocations = probe.count();
    }
    ASC_DENSE_TEST_EQ(test, allocations, std::size_t{0});
  }
  const int restored = mprotect(memory, bytes, PROT_READ | PROT_WRITE);
  ASC_DENSE_TEST_EQ(test, restored, 0);
  if (restored == 0) {
    for (std::size_t i = 0; i < 8; ++i) {
      ASC_DENSE_TEST_EQ(test, data[i], T{17});
      std::destroy_at(data + i);
    }
  }
  ASC_DENSE_TEST_EQ(test, munmap(memory, bytes), 0);
}

}  // namespace

int main() {
  TestContext test;
  TestNoCoefficientReads<float>(test);
  TestNoCoefficientReads<double>(test);
  TestNoCoefficientReads<std::complex<float>>(test);
  TestNoCoefficientReads<std::complex<double>>(test);
  return test.Finish();
}
