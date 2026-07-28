#ifndef ASC_TESTS_DENSE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_TEST_SUPPORT_H_

#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace asc_dense_test {

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
  void CheckNear(Real actual, Real expected, Real absolute_tolerance,
                 Real relative_tolerance, std::string_view actual_expression,
                 std::string_view expected_expression, std::string_view file,
                 int line) {
    const Real difference = std::abs(actual - expected);
    const Real limit =
        absolute_tolerance + relative_tolerance * std::abs(expected);
    if (!(difference <= limit)) {
      std::cerr << file << ':' << line
                << ": near check failed: " << actual_expression
                << " ~= " << expected_expression
                << ", difference=" << difference << ", limit=" << limit << '\n';
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

template <typename Real>
constexpr Real DefaultTolerance() {
  return Real{16} * std::numeric_limits<Real>::epsilon();
}

}  // namespace asc_dense_test

#define ASC_DENSE_TEST_CHECK(context, expression)                       \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_DENSE_TEST_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_DENSE_TEST_NEAR(context, actual, expected, absolute, relative)   \
  (context).CheckNear((actual), (expected), (absolute), (relative), #actual, \
                      #expected, __FILE__, __LINE__)

#endif  // ASC_TESTS_DENSE_TEST_SUPPORT_H_
