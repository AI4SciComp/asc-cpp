#ifndef ASC_TESTS_RANDOM_SPARSE_TEST_SUPPORT_H_
#define ASC_TESTS_RANDOM_SPARSE_TEST_SUPPORT_H_

#include <cstddef>
#include <iostream>
#include <string_view>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc_random_sparse_test {

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
      std::cerr << failures_ << " random-sparse test check(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

class CountingMemoryResource final : public asc::MemoryResource {
 public:
  explicit CountingMemoryResource(
      asc::MemorySpace space = asc::MemorySpace::kHost) noexcept
      : space_(space) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return space_;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_attempts_;
    if (fail_on_attempt_ != 0 && allocation_attempts_ == fail_on_attempt_) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected random-sparse allocation failure");
    }
    auto allocated = host_.Allocate(bytes, alignment);
    if (allocated.ok() && *allocated != nullptr) {
      ++successful_allocations_;
      allocated_bytes_ += bytes;
    }
    return allocated;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    if (pointer != nullptr) {
      ++deallocations_;
      deallocated_bytes_ += bytes;
    }
    host_.Deallocate(pointer, bytes, alignment);
  }

  void FailOnAttempt(std::size_t attempt) noexcept {
    fail_on_attempt_ = attempt;
  }

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
  [[nodiscard]] std::size_t live_allocations() const noexcept {
    return successful_allocations_ - deallocations_;
  }

 private:
  asc::HostMemoryResource host_;
  asc::MemorySpace space_;
  std::size_t fail_on_attempt_ = 0;
  std::size_t allocation_attempts_ = 0;
  std::size_t successful_allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t deallocated_bytes_ = 0;
};

}  // namespace asc_random_sparse_test

#define ASC_RANDOM_SPARSE_TEST_CHECK(context, expression)               \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_RANDOM_SPARSE_TEST_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#endif  // ASC_TESTS_RANDOM_SPARSE_TEST_SUPPORT_H_
