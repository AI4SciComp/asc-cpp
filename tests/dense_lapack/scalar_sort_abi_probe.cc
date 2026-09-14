#include <array>
#include <cstdio>
#include <limits>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "asc/dense/providers/lapack_scalar_sort.h"
#include "installed_lu/normal_return_guard.h"
#include "scalar_sort_test_support.h"
#if defined(ASC_SORT_SLASRT_HEADER)
#include <type_traits>
#define slasrt_ emitted_slasrt
#include ASC_SORT_SLASRT_HEADER
#undef slasrt_
#define dlasrt_ emitted_dlasrt
#include ASC_SORT_DLASRT_HEADER
#undef dlasrt_
#endif
namespace {
namespace sort = asc_scalar_sort_test;
#if defined(ASC_SORT_SLASRT_HEADER)
template <typename T>
struct Arg {
  using Type = T;
};
template <typename T>
struct Arg<T*> {
  using Type = std::remove_const_t<T>*;
};
template <typename T>
struct Sig;
template <typename R, typename... A>
struct Sig<R(A...)> {
  using Type = R(typename Arg<A>::Type...);
};
static_assert(std::is_same_v<Sig<decltype(LAPACK_slasrt_base)>::Type,
                             Sig<decltype(emitted_slasrt)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_dlasrt_base)>::Type,
                             Sig<decltype(emitted_dlasrt)>::Type>);
#endif
template <typename T>
void Run(sort::TestContext& test) {
  for (auto n : sort::kLengths) {
    for (auto order : sort::kOrders) {
      for (unsigned pattern = 0; pattern < 8; ++pattern) {
        for (unsigned seed = 0; seed < 16; ++seed) {
          sort::Buffer<T> buffer(n, pattern, seed);
          const auto before = buffer.values;
          const bool nan = buffer.HasNan();
          std::array<lapack_int, 3> size{89, static_cast<lapack_int>(n), 97};
          std::array<lapack_int, 3> info{
              101, std::numeric_limits<lapack_int>::max(), 103};
          std::array<char, 3> id{
              'a', order == asc::LapackSortOrder::kIncreasing ? 'I' : 'D', 'b'};
          const auto old_size = size;
          const auto old_id = id;
          sort::Native(&id[1], &size[1], buffer.values.data() + 1, &info[1]);
          ASC_DENSE_TEST_CHECK(test, size == old_size);
          ASC_DENSE_TEST_CHECK(test, id == old_id);
          ASC_DENSE_TEST_EQ(test, info[0], 101);
          ASC_DENSE_TEST_EQ(test, info[1], 0);
          ASC_DENSE_TEST_EQ(test, info[2], 103);
          ASC_DENSE_TEST_CHECK(test, sort::Permutation(buffer, before));
          if (!nan) {
            ASC_DENSE_TEST_CHECK(test, sort::Ordered(buffer, order));
          }
        }
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  sort::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf("5120 scalar sort ABI cases; integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
