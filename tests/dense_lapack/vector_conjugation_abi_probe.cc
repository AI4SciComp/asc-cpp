#include <array>
#include <complex>
#include <cstddef>
#include <cstdio>

#include "../../src/dense/lapack/internal_tridiagonal.h"
#include "../dense/test_support.h"
#include "installed_lu/normal_return_guard.h"
#include "vector_conjugation_test_support.h"
#if defined(ASC_CONJ_CLACGV_HEADER)
#include <type_traits>
#define clacgv_ emitted_clacgv
#include ASC_CONJ_CLACGV_HEADER
#undef clacgv_
#define zlacgv_ emitted_zlacgv
#include ASC_CONJ_ZLACGV_HEADER
#undef zlacgv_
#endif
namespace {
namespace conj = asc_vector_conjugation_test;
#if defined(ASC_CONJ_CLACGV_HEADER)
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
static_assert(std::is_same_v<Sig<decltype(LAPACK_clacgv)>::Type,
                             Sig<decltype(emitted_clacgv)>::Type>);
static_assert(std::is_same_v<Sig<decltype(LAPACK_zlacgv)>::Type,
                             Sig<decltype(emitted_zlacgv)>::Type>);
#endif
template <typename T>
void Run(conj::TestContext& test) {
  for (T real : conj::Values<T>()) {
    for (T imaginary : conj::Values<T>()) {
      for (lapack_int n : {0, 1, 2, 5, 13}) {
        for (lapack_int increment : {-5, -2, -1, 0, 1, 2, 5}) {
          conj::Buffer<T> buffer(n, increment,
                                 std::complex<T>{real, imaginary});
          const auto expected = buffer.Expected(n, increment);
          std::array<lapack_int, 3> count{79, n, 83};
          std::array<lapack_int, 3> stride{89, increment, 97};
          const auto before_count = count;
          const auto before_stride = stride;
          conj::Native(&count[1], buffer.values.data() + 1, &stride[1]);
          ASC_DENSE_TEST_CHECK(test, count == before_count);
          ASC_DENSE_TEST_CHECK(test, stride == before_stride);
          for (std::size_t i = 0; i < expected.size(); ++i) {
            ASC_DENSE_TEST_CHECK(test,
                                 conj::Same(buffer.values[i], expected[i]));
          }
        }
      }
    }
  }
}
}  // namespace
int main() {
  const asc_lapack_test::NormalReturnGuard guard;
  conj::TestContext test;
  Run<float>(test);
  Run<double>(test);
  std::printf("28000 vector conjugation ABI cases; integer bytes=%zu\n",
              sizeof(lapack_int));
  return test.Finish();
}
