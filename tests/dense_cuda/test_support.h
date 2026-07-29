#ifndef ASC_TESTS_DENSE_CUDA_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_CUDA_TEST_SUPPORT_H_

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace asc_dense_cuda_test {

inline constexpr int kSkipReturnCode = 77;

inline bool ForceNoCudaDevice() noexcept {
  const char* value = std::getenv("ASC_CPP_TEST_FORCE_NO_CUDA_DEVICE");
  return value != nullptr && std::string_view(value) == "1";
}

class TestContext {
 public:
  void Check(bool condition, std::string_view expression, std::string_view file,
             int line) {
    if (!condition) {
      std::cerr << file << ':' << line << ": check failed: " << expression
                << '\n';
      ++failures_;
    }
  }

  template <typename Left, typename Right>
  void CheckEqual(const Left& left, const Right& right,
                  std::string_view left_expression,
                  std::string_view right_expression, std::string_view file,
                  int line) {
    if (!(left == right)) {
      std::cerr << file << ':' << line
                << ": equality check failed: " << left_expression
                << " == " << right_expression << '\n';
      ++failures_;
    }
  }

  template <typename Real>
  void CheckNear(Real actual, Real expected, Real scale,
                 std::string_view actual_expression,
                 std::string_view expected_expression, std::string_view file,
                 int line) {
    const Real tolerance = Real{32} * std::numeric_limits<Real>::epsilon() *
                           std::max(Real{1}, scale) *
                           std::max(Real{1}, std::abs(expected));
    if (!std::isfinite(actual) || !std::isfinite(expected) ||
        std::abs(actual - expected) > tolerance) {
      std::cerr << file << ':' << line
                << ": near check failed: " << actual_expression
                << " ~= " << expected_expression << ", actual=" << actual
                << ", expected=" << expected << ", tolerance=" << tolerance
                << '\n';
      ++failures_;
    }
  }

  [[nodiscard]] int Finish() const {
    if (failures_ != 0) {
      std::cerr << failures_ << " test check(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

}  // namespace asc_dense_cuda_test

#define ASC_DENSE_CUDA_CHECK(context, expression)                       \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_DENSE_CUDA_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_DENSE_CUDA_NEAR(context, actual, expected, scale)            \
  (context).CheckNear((actual), (expected), (scale), #actual, #expected, \
                      __FILE__, __LINE__)

#endif  // ASC_TESTS_DENSE_CUDA_TEST_SUPPORT_H_
