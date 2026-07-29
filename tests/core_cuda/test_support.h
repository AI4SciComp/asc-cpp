#ifndef ASC_TESTS_CORE_CUDA_TEST_SUPPORT_H_
#define ASC_TESTS_CORE_CUDA_TEST_SUPPORT_H_

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>

#include "asc/core/status.h"

namespace asc_core_cuda_test {

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

  void CheckProviderFailure(const asc::Status& status,
                            std::string_view expression, std::string_view file,
                            int line) {
    if (status.ok() || status.provider() != "CUDA" ||
        status.native_code() == std::int64_t{0}) {
      std::cerr << file << ':' << line
                << ": provider failure check failed: " << expression
                << " (code=" << static_cast<std::uint32_t>(status.code())
                << ", provider='" << status.provider()
                << "', native=" << status.native_code() << ")\n";
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

}  // namespace asc_core_cuda_test

#define ASC_CORE_CUDA_CHECK(context, expression)                        \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_CORE_CUDA_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_CORE_CUDA_PROVIDER_FAILURE(context, status) \
  (context).CheckProviderFailure((status), #status, __FILE__, __LINE__)

#endif  // ASC_TESTS_CORE_CUDA_TEST_SUPPORT_H_
