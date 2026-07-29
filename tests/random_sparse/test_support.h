#ifndef ASC_TESTS_RANDOM_SPARSE_TEST_SUPPORT_H_
#define ASC_TESTS_RANDOM_SPARSE_TEST_SUPPORT_H_

#include <array>
#include <cstddef>
#include <iostream>
#include <limits>
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
      std::cerr << failures_ << " test check(s) failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

struct AllocationRecord {
  std::size_t bytes = 0;
  std::size_t alignment = 0;
  void* pointer = nullptr;
};

class TrackingMemoryResource final : public asc::MemoryResource {
 public:
  explicit TrackingMemoryResource(
      asc::MemorySpace space = asc::MemorySpace::kHost) noexcept
      : space_(space) {}

  [[nodiscard]] asc::MemorySpace space() const noexcept override {
    return space_;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    const std::size_t call = allocation_attempts_++;
    if (call < records_.size()) {
      records_[call].bytes = bytes;
      records_[call].alignment = alignment;
    }
    if (call == failure_call_) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected random Sparse allocation failure");
    }
    auto allocated = host_.Allocate(bytes, alignment);
    if (allocated.ok()) {
      ++successful_allocations_;
      if (*allocated != nullptr) {
        ++live_allocations_;
      }
      if (call < records_.size()) {
        records_[call].pointer = *allocated;
      }
    }
    return allocated;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    bool known_live = false;
    for (std::size_t call = 0;
         call < allocation_attempts_ && call < records_.size(); ++call) {
      if (records_[call].pointer == pointer && pointer != nullptr) {
        known_live = true;
        records_[call].pointer = nullptr;
        break;
      }
    }
    if (!known_live && pointer != nullptr) {
      ++duplicate_or_unknown_deallocations_;
    } else if (pointer != nullptr) {
      ++deallocations_;
      --live_allocations_;
    }
    host_.Deallocate(pointer, bytes, alignment);
  }

  void FailOnCall(std::size_t call) noexcept { failure_call_ = call; }

  [[nodiscard]] std::size_t allocation_attempts() const noexcept {
    return allocation_attempts_;
  }
  [[nodiscard]] std::size_t successful_allocations() const noexcept {
    return successful_allocations_;
  }
  [[nodiscard]] std::size_t live_allocations() const noexcept {
    return live_allocations_;
  }
  [[nodiscard]] std::size_t deallocations() const noexcept {
    return deallocations_;
  }
  [[nodiscard]] std::size_t duplicate_or_unknown_deallocations()
      const noexcept {
    return duplicate_or_unknown_deallocations_;
  }
  [[nodiscard]] const AllocationRecord& record(std::size_t call) const {
    return records_[call];
  }

 private:
  asc::HostMemoryResource host_;
  asc::MemorySpace space_;
  std::array<AllocationRecord, 8> records_{};
  std::size_t allocation_attempts_ = 0;
  std::size_t successful_allocations_ = 0;
  std::size_t live_allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t duplicate_or_unknown_deallocations_ = 0;
  std::size_t failure_call_ = std::numeric_limits<std::size_t>::max();
};

}  // namespace asc_random_sparse_test

#define ASC_RANDOM_SPARSE_TEST_CHECK(context, expression)               \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_RANDOM_SPARSE_TEST_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#endif  // ASC_TESTS_RANDOM_SPARSE_TEST_SUPPORT_H_
