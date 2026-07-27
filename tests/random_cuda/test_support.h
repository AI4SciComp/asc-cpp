#ifndef ASC_TESTS_RANDOM_CUDA_TEST_SUPPORT_H_
#define ASC_TESTS_RANDOM_CUDA_TEST_SUPPORT_H_

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/providers/cuda.h"
#include "asc/core/result.h"
#include "asc/core/status.h"
#include "asc/core/types.h"

namespace asc_random_cuda_test {

inline constexpr int kSkipReturnCode = 77;

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

  template <typename Value>
    requires std::is_floating_point_v<Value>
  void CheckBitEqual(Value left, Value right, std::string_view left_expression,
                     std::string_view right_expression, std::string_view file,
                     int line) {
    using Bits = std::conditional_t<sizeof(Value) == sizeof(std::uint32_t),
                                    std::uint32_t, std::uint64_t>;
    CheckEqual(std::bit_cast<Bits>(left), std::bit_cast<Bits>(right),
               left_expression, right_expression, file, line);
  }

  [[nodiscard]] int Finish() const {
    if (failures_ != 0) {
      std::cerr << failures_ << " Milestone 7 CUDA verification check(s) "
                << "failed\n";
      return 1;
    }
    return 0;
  }

 private:
  int failures_ = 0;
};

[[nodiscard]] inline asc::Device CudaDevice(std::int32_t ordinal = 0) {
  return asc::Device{asc::Backend::kCuda, ordinal};
}

[[nodiscard]] inline bool HasCudaDevice() {
  const auto count = asc::CudaDeviceCount();
  return count.ok() && *count > 0;
}

inline asc::Status CopyAndWait(const asc::ExecutionContext& execution,
                               asc::MutableMemoryView destination,
                               asc::ConstMemoryView source) {
  auto event = asc::CopyBytes(execution, destination, source);
  if (!event.ok()) {
    return event.status();
  }
  return event->Wait();
}

template <typename Element>
asc::Result<asc::Buffer> Upload(std::span<const Element> source,
                                asc::MemoryResource& device_resource,
                                const asc::ExecutionContext& execution) {
  auto bytes = asc::CheckedMultiply(source.size(), sizeof(Element));
  if (!bytes.ok()) {
    return bytes.status();
  }
  auto destination =
      asc::Buffer::Allocate(device_resource, *bytes, alignof(Element));
  if (!destination.ok()) {
    return destination.status();
  }
  auto destination_view = destination->mutable_view();
  if (!destination_view.ok()) {
    return destination_view.status();
  }
  const asc::ConstMemoryView source_view(source.data(), *bytes,
                                         asc::MemorySpace::kHost);
  const asc::Status copy_status =
      CopyAndWait(execution, *destination_view, source_view);
  if (!copy_status.ok()) {
    return copy_status;
  }
  return std::move(*destination);
}

template <typename Element>
asc::Result<std::vector<Element>> Download(
    const void* source, std::size_t count, asc::MemorySpace source_space,
    const asc::ExecutionContext& execution) {
  auto bytes = asc::CheckedMultiply(count, sizeof(Element));
  if (!bytes.ok()) {
    return bytes.status();
  }
  std::vector<Element> result(count);
  const asc::Status copy_status = CopyAndWait(
      execution,
      asc::MutableMemoryView(result.data(), *bytes, asc::MemorySpace::kHost),
      asc::ConstMemoryView(source, *bytes, source_space));
  if (!copy_status.ok()) {
    return copy_status;
  }
  return result;
}

template <typename Element>
asc::Result<std::vector<Element>> Download(
    const asc::Buffer& source, const asc::ExecutionContext& execution) {
  if (source.size() % sizeof(Element) != 0) {
    return asc::Status(asc::ErrorCode::kInvalidArgument,
                       "Test buffer size is not an element multiple");
  }
  auto source_space = source.space();
  if (!source_space.ok()) {
    return source_space.status();
  }
  return Download<Element>(source.data(), source.size() / sizeof(Element),
                           *source_space, execution);
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
