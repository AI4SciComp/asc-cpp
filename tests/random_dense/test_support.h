#ifndef ASC_TESTS_RANDOM_DENSE_TEST_SUPPORT_H_
#define ASC_TESTS_RANDOM_DENSE_TEST_SUPPORT_H_

#include <iostream>
#include <string_view>

namespace asc_random_dense_test {

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

  [[nodiscard]] int Finish() const {
    if (failures_ != 0) {
      std::cerr << failures_ << " random-dense test check(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

}  // namespace asc_random_dense_test

#define ASC_RANDOM_DENSE_TEST_CHECK(context, expression)                \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_RANDOM_DENSE_TEST_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#endif  // ASC_TESTS_RANDOM_DENSE_TEST_SUPPORT_H_
