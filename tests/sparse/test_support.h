#ifndef ASC_TESTS_SPARSE_TEST_SUPPORT_H_
#define ASC_TESTS_SPARSE_TEST_SUPPORT_H_

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>

#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"
#include "asc/expression/expression.h"
#include "asc/expression/writable.h"

namespace external_sparse_test {

template <typename Element>
struct Vector {
  using value_type = std::remove_cv_t<Element>;

  Vector* operator&() = delete;
  const Vector* operator&() const = delete;

  Element* data = nullptr;
  asc::extent_t size = 0;
  asc::stride_t stride = 1;
  asc::MemorySpace space = asc::MemorySpace::kHost;
  const void* alias_identity = nullptr;
  std::size_t* read_count = nullptr;
  std::size_t* write_count = nullptr;
  bool poison_reads = false;
};

}  // namespace external_sparse_test

namespace asc {

template <typename Element>
struct ExpressionAdapter<external_sparse_test::Vector<Element>> {
  using value_type = typename external_sparse_test::Vector<Element>::value_type;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  [[nodiscard]] static std::array<extent_t, 1> Shape(
      const external_sparse_test::Vector<Element>& vector) noexcept {
    return {vector.size};
  }

  [[nodiscard]] static value_type Read(
      const external_sparse_test::Vector<Element>& vector,
      std::span<const index_t, 1> indices) noexcept {
    if (vector.read_count != nullptr) {
      ++*vector.read_count;
    }
    if (vector.poison_reads) {
      return std::numeric_limits<value_type>::quiet_NaN();
    }
    const auto offset = static_cast<std::size_t>(indices[0] * vector.stride);
    return vector.data[offset];
  }

  [[nodiscard]] static bool MayAlias(
      const external_sparse_test::Vector<Element>& vector,
      AliasToken alias) noexcept {
    return AliasToken::FromIdentity(vector.alias_identity) == alias;
  }
};

template <typename Element>
struct ExpressionPlacementAdapter<external_sparse_test::Vector<Element>> {
  [[nodiscard]] static MemorySpace Space(
      const external_sparse_test::Vector<Element>& vector) noexcept {
    return vector.space;
  }
};

template <typename Element>
  requires(!std::is_const_v<Element>)
struct WritableExpressionAdapter<external_sparse_test::Vector<Element>> {
  using value_type = typename external_sparse_test::Vector<Element>::value_type;
  static constexpr rank_t kRank = 1;

  [[nodiscard]] static std::array<extent_t, 1> Shape(
      const external_sparse_test::Vector<Element>& vector) noexcept {
    return {vector.size};
  }

  [[nodiscard]] static AliasToken Alias(
      const external_sparse_test::Vector<Element>& vector) noexcept {
    return AliasToken::FromIdentity(vector.alias_identity);
  }

  static void Write(external_sparse_test::Vector<Element>& vector,
                    std::span<const index_t, 1> indices,
                    value_type value) noexcept {
    if (vector.write_count != nullptr) {
      ++*vector.write_count;
    }
    const auto offset = static_cast<std::size_t>(indices[0] * vector.stride);
    vector.data[offset] = value;
  }
};

}  // namespace asc

namespace asc_sparse_test {

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

  template <typename Left, typename Right>
  void CheckRangeEqual(const Left& left, const Right& right,
                       std::string_view left_expression,
                       std::string_view right_expression, std::string_view file,
                       int line) {
    if (left.size() != right.size() ||
        !std::equal(left.begin(), left.end(), right.begin())) {
      std::cerr << file << ':' << line
                << ": range equality check failed: " << left_expression
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
      std::cerr << failures_ << " sparse test check(s) failed\n";
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
    return space_;
  }

  asc::Result<void*> Allocate(std::size_t bytes,
                              std::size_t alignment) override {
    ++allocation_attempts_;
    if (allocation_attempts_ == failed_attempt_) {
      return asc::Status(asc::ErrorCode::kAllocation,
                         "Injected sparse-test allocation failure");
    }
    auto result = host_.Allocate(bytes, alignment);
    if (result.ok()) {
      ++successful_allocations_;
      ++live_allocations_;
      allocated_bytes_ += bytes;
    }
    return result;
  }

  void Deallocate(void* pointer, std::size_t bytes,
                  std::size_t alignment) noexcept override {
    ++deallocations_;
    if (live_allocations_ != 0) {
      --live_allocations_;
    }
    deallocated_bytes_ += bytes;
    host_.Deallocate(pointer, bytes, alignment);
  }

  void FailAllocationAttempt(std::size_t attempt) noexcept {
    failed_attempt_ = attempt;
  }

  void set_space(asc::MemorySpace space) noexcept { space_ = space; }

  [[nodiscard]] std::size_t allocation_attempts() const noexcept {
    return allocation_attempts_;
  }
  [[nodiscard]] std::size_t successful_allocations() const noexcept {
    return successful_allocations_;
  }
  [[nodiscard]] std::size_t deallocations() const noexcept {
    return deallocations_;
  }
  [[nodiscard]] std::size_t live_allocations() const noexcept {
    return live_allocations_;
  }
  [[nodiscard]] std::size_t allocated_bytes() const noexcept {
    return allocated_bytes_;
  }
  [[nodiscard]] std::size_t deallocated_bytes() const noexcept {
    return deallocated_bytes_;
  }

 private:
  asc::HostMemoryResource host_;
  asc::MemorySpace space_ = asc::MemorySpace::kHost;
  std::size_t failed_attempt_ = std::numeric_limits<std::size_t>::max();
  std::size_t allocation_attempts_ = 0;
  std::size_t successful_allocations_ = 0;
  std::size_t deallocations_ = 0;
  std::size_t live_allocations_ = 0;
  std::size_t allocated_bytes_ = 0;
  std::size_t deallocated_bytes_ = 0;
};

}  // namespace asc_sparse_test

#define ASC_SPARSE_TEST_CHECK(context, expression)                      \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_SPARSE_TEST_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_SPARSE_TEST_RANGE_EQ(context, left, right) \
  (context).CheckRangeEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_SPARSE_TEST_NEAR(context, actual, expected, absolute, relative)  \
  (context).CheckNear(static_cast<double>(actual),                           \
                      static_cast<double>(expected), (absolute), (relative), \
                      #actual, #expected, __FILE__, __LINE__)

#endif  // ASC_TESTS_SPARSE_TEST_SUPPORT_H_
