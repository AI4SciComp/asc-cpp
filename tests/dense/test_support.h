#ifndef ASC_TESTS_DENSE_TEST_SUPPORT_H_
#define ASC_TESTS_DENSE_TEST_SUPPORT_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

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

  void CheckNear(double actual, double expected, double absolute_tolerance,
                 double relative_tolerance, std::string_view actual_expression,
                 std::string_view expected_expression, std::string_view file,
                 int line) {
    const double scale = std::max(std::fabs(actual), std::fabs(expected));
    const double allowed =
        std::max(absolute_tolerance, relative_tolerance * scale);
    if (!(std::fabs(actual - expected) <= allowed)) {
      std::cerr << file << ':' << line
                << ": near check failed: " << actual_expression
                << " ~= " << expected_expression << " (actual " << actual
                << ", expected " << expected << ", tolerance " << allowed
                << ")\n";
      ++failures_;
    }
  }

  [[nodiscard]] int Finish() const {
    if (failures_ != 0) {
      std::cerr << failures_ << " dense test check(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

class CountingMemoryResource final : public asc::MemoryResource {
 public:
  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return asc::MemorySpace::kHost;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_attempts_;
    if (fail_next_allocation_) {
      fail_next_allocation_ = false;
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected dense-test allocation failure");
    }
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok()) {
      ++successful_allocations_;
      allocated_bytes_ += bytes;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocations_;
    deallocated_bytes_ += bytes;
    host_.Deallocate(pointer, bytes, alignment);
  }

  void FailNextAllocation() noexcept { fail_next_allocation_ = true; }

  [[nodiscard]] std::size_t allocation_attempts() const noexcept {
    return allocation_attempts_;
  }
  [[nodiscard]] std::size_t successful_allocations() const noexcept {
    return successful_allocations_;
  }
  [[nodiscard]] std::size_t deallocations() const noexcept {
    return deallocations_;
  }
  [[nodiscard]] std::size_t allocated_bytes() const noexcept {
    return allocated_bytes_;
  }
  [[nodiscard]] std::size_t deallocated_bytes() const noexcept {
    return deallocated_bytes_;
  }

 private:
  asc::HostMemoryResource host_;
  bool fail_next_allocation_ = false;
  std::size_t allocation_attempts_ = 0;
  std::size_t successful_allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t deallocated_bytes_ = 0;
};

}  // namespace asc_dense_test

#define ASC_DENSE_TEST_CHECK(context, expression)                       \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_DENSE_TEST_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_DENSE_TEST_NEAR(context, actual, expected, absolute, relative)   \
  (context).CheckNear(static_cast<double>(actual),                           \
                      static_cast<double>(expected), (absolute), (relative), \
                      #actual, #expected, __FILE__, __LINE__)

#endif  // ASC_TESTS_DENSE_TEST_SUPPORT_H_
