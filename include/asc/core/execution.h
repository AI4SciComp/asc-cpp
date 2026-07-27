#ifndef ASC_CORE_EXECUTION_H_
#define ASC_CORE_EXECUTION_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "asc/core/export.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

namespace internal_core_execution {

class CompletionState;
class CompletionAccess;
class ExecutionAccess;
class ExecutionState;

}  // namespace internal_core_execution

enum class Backend : std::uint8_t {
  kSerial = 0,
  kCuda = 1,
  kHip = 2,
  kSycl = 3,
};

ASC_CORE_EXPORT const char* BackendName(Backend backend) noexcept;

struct ASC_CORE_EXPORT Device {
  Backend backend = Backend::kSerial;
  std::int32_t ordinal = 0;

  static Device Serial() noexcept;
  friend bool operator==(const Device&, const Device&) = default;
};

class CompletionEvent;

enum class Determinism : std::uint8_t {
  kDeterministic = 0,
  kBackendDefault = 1,
};

class ASC_CORE_EXPORT ExecutionContext {
 public:
  static ExecutionContext Serial() noexcept;
  static Result<ExecutionContext> Create(
      Backend backend, Device device,
      Determinism determinism = Determinism::kDeterministic);

  [[nodiscard]] Backend backend() const noexcept { return backend_; }
  [[nodiscard]] Device device() const noexcept { return device_; }
  [[nodiscard]] Determinism determinism() const noexcept {
    return determinism_;
  }
  [[nodiscard]] bool CanAccess(MemorySpace space) const noexcept;

 private:
  friend class internal_core_execution::ExecutionAccess;
  friend ASC_CORE_EXPORT Result<CompletionEvent> CopyBytes(
      const ExecutionContext&, MutableMemoryView, ConstMemoryView, std::size_t);

  ExecutionContext(
      Backend backend, Device device, Determinism determinism,
      std::shared_ptr<const internal_core_execution::ExecutionState> state =
          nullptr) noexcept;

  Backend backend_;
  Device device_;
  Determinism determinism_;
  std::shared_ptr<const internal_core_execution::ExecutionState> state_;
};

class ASC_CORE_EXPORT CompletionEvent {
 public:
  CompletionEvent(const CompletionEvent&) = delete;
  CompletionEvent& operator=(const CompletionEvent&) = delete;
  CompletionEvent(CompletionEvent&& other) noexcept;
  CompletionEvent& operator=(CompletionEvent&& other) noexcept;
  ~CompletionEvent();

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] bool is_complete() const noexcept {
    return valid_ && complete_;
  }
  [[nodiscard]] Result<bool> Query() const;
  [[nodiscard]] Status Wait() const;

 private:
  friend class internal_core_execution::CompletionAccess;
  friend ASC_CORE_EXPORT Result<CompletionEvent> CopyBytes(
      const ExecutionContext&, MutableMemoryView, ConstMemoryView, std::size_t);

  explicit CompletionEvent(bool complete) noexcept;
  explicit CompletionEvent(
      std::unique_ptr<internal_core_execution::CompletionState> state) noexcept;

  bool valid_ = false;
  mutable bool complete_ = false;
  std::unique_ptr<internal_core_execution::CompletionState> state_;
};

ASC_CORE_EXPORT Result<CompletionEvent> CopyBytes(
    const ExecutionContext& context, MutableMemoryView destination,
    ConstMemoryView source, std::size_t byte_count);

inline Result<CompletionEvent> CopyBytes(const ExecutionContext& context,
                                         MutableMemoryView destination,
                                         ConstMemoryView source) {
  return CopyBytes(context, destination, source, source.size());
}

}  // namespace asc

#endif  // ASC_CORE_EXECUTION_H_
