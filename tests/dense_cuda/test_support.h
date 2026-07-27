#ifndef ASC_TESTS_DENSE_CUDA_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_CUDA_TEST_SUPPORT_H_

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

namespace asc_dense_cuda_test {

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

  template <typename Actual, typename Expected>
  void CheckNear(Actual actual, Expected expected, long double scale,
                 std::size_t reduction_length,
                 std::string_view actual_expression,
                 std::string_view expected_expression, std::string_view file,
                 int line) {
    using Scalar = std::common_type_t<Actual, Expected>;
    const long double actual_value = static_cast<long double>(actual);
    const long double expected_value = static_cast<long double>(expected);
    if (std::isnan(expected_value)) {
      if (!std::isnan(actual_value)) {
        ReportNearFailure(actual_value, expected_value, 0.0L, actual_expression,
                          expected_expression, file, line);
      }
      return;
    }
    if (std::isinf(expected_value)) {
      if (actual_value != expected_value) {
        ReportNearFailure(actual_value, expected_value, 0.0L, actual_expression,
                          expected_expression, file, line);
      }
      return;
    }
    const long double tolerance =
        8.0L *
        static_cast<long double>(std::numeric_limits<Scalar>::epsilon()) *
        static_cast<long double>(std::max<std::size_t>(1, reduction_length)) *
        std::max({1.0L, std::fabs(expected_value), std::fabs(scale)});
    if (!(std::fabs(actual_value - expected_value) <= tolerance)) {
      ReportNearFailure(actual_value, expected_value, tolerance,
                        actual_expression, expected_expression, file, line);
    }
  }

  [[nodiscard]] int Finish() const {
    if (failures_ != 0) {
      std::cerr << failures_ << " dense CUDA test check(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  void ReportNearFailure(long double actual, long double expected,
                         long double tolerance,
                         std::string_view actual_expression,
                         std::string_view expected_expression,
                         std::string_view file, int line) {
    std::cerr << file << ':' << line
              << ": numerical check failed: " << actual_expression
              << " ~= " << expected_expression << " (actual "
              << static_cast<double>(actual) << ", expected "
              << static_cast<double>(expected) << ", tolerance "
              << static_cast<double>(tolerance) << ")\n";
    ++failures_;
  }

  int failures_ = 0;
};

}  // namespace asc_dense_cuda_test

#define ASC_DENSE_CUDA_CHECK(context, expression)                       \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_DENSE_CUDA_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_DENSE_CUDA_NEAR(context, actual, expected, scale, length)   \
  (context).CheckNear((actual), (expected), (scale), (length), #actual, \
                      #expected, __FILE__, __LINE__)

#endif  // ASC_TESTS_DENSE_CUDA_TEST_SUPPORT_H_
