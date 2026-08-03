#ifndef ASC_TESTS_RANDOM_CUDA_TEST_SUPPORT_H_
#define ASC_TESTS_RANDOM_CUDA_TEST_SUPPORT_H_

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/types.h"

namespace asc_random_cuda_test {

inline constexpr int kSkipReturnCode = 77;
inline constexpr std::int32_t kCudaDevice = 0;

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
  void CheckBitEqual(Real left, Real right, std::string_view left_expression,
                     std::string_view right_expression, std::string_view file,
                     int line) {
    static_assert(std::same_as<Real, float> || std::same_as<Real, double>);
    using Bits = std::conditional_t<std::same_as<Real, float>, std::uint32_t,
                                    std::uint64_t>;
    CheckEqual(std::bit_cast<Bits>(left), std::bit_cast<Bits>(right),
               left_expression, right_expression, file, line);
  }

  [[nodiscard]] int Finish() const {
    if (failures_ != 0) {
      std::cerr << failures_
                << " CUDA Sparse and Random CUDA verification check(s) "
                   "failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

inline bool HasCudaDevice() {
  auto count = asc::CudaDeviceCount();
  return count.ok() && *count > kCudaDevice;
}

inline asc::Status CopyAndWait(const asc::ExecutionContext& context,
                               asc::MutableMemoryView destination,
                               asc::ConstMemoryView source) {
  auto event = asc::CopyBytes(context, destination, source);
  if (!event.ok()) {
    return event.status();
  }
  return event->Wait();
}

template <typename Element>
asc::Result<asc::Buffer> Upload(std::span<const Element> source,
                                asc::MemoryResource& resource,
                                const asc::ExecutionContext& context) {
  auto bytes = asc::CheckedMultiply(source.size(), sizeof(Element));
  if (!bytes.ok()) {
    return bytes.status();
  }
  auto destination = asc::Buffer::Allocate(resource, *bytes, alignof(Element));
  if (!destination.ok()) {
    return destination.status();
  }
  const asc::Status copied = CopyAndWait(
      context,
      asc::MutableMemoryView(destination->data(), *bytes, resource.space()),
      asc::ConstMemoryView(source.data(), *bytes, asc::MemorySpace::kHost));
  if (!copied.ok()) {
    return copied;
  }
  return std::move(*destination);
}

template <typename Element, std::size_t Size>
asc::Result<asc::Buffer> Upload(const std::array<Element, Size>& source,
                                asc::MemoryResource& resource,
                                const asc::ExecutionContext& context) {
  return Upload<Element>(std::span<const Element>(source), resource, context);
}

template <typename Element>
asc::Result<std::vector<Element>> Download(
    const void* source, std::size_t count, asc::MemorySpace source_space,
    const asc::ExecutionContext& context) {
  auto bytes = asc::CheckedMultiply(count, sizeof(Element));
  if (!bytes.ok()) {
    return bytes.status();
  }
  std::vector<Element> result(count);
  const asc::Status copied = CopyAndWait(
      context,
      asc::MutableMemoryView(result.data(), *bytes, asc::MemorySpace::kHost),
      asc::ConstMemoryView(source, *bytes, source_space));
  if (!copied.ok()) {
    return copied;
  }
  return result;
}

template <typename Element>
asc::Result<std::vector<Element>> Download(
    const asc::Buffer& source, const asc::ExecutionContext& context) {
  if (source.size() % sizeof(Element) != 0) {
    return asc::Status(asc::ErrorCode::kInvalidArgument,
                       "Test buffer size is not an element multiple");
  }
  auto space = source.space();
  if (!space.ok()) {
    return space.status();
  }
  return Download<Element>(source.data(), source.size() / sizeof(Element),
                           *space, context);
}

}  // namespace asc_random_cuda_test

#define ASC_M7_CUDA_CHECK(context, expression)                          \
  (context).Check(static_cast<bool>(expression), #expression, __FILE__, \
                  __LINE__)

#define ASC_M7_CUDA_EQ(context, left, right) \
  (context).CheckEqual((left), (right), #left, #right, __FILE__, __LINE__)

#define ASC_M7_CUDA_BIT_EQ(context, left, right) \
  (context).CheckBitEqual((left), (right), #left, #right, __FILE__, __LINE__)

#endif  // ASC_TESTS_RANDOM_CUDA_TEST_SUPPORT_H_
