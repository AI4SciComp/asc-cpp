#ifndef ASC_SRC_CORE_EXECUTION_INTERNAL_H_
#define ASC_SRC_CORE_EXECUTION_INTERNAL_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "asc/core/execution.h"

namespace asc::internal_core_execution {

class CompletionState {
 public:
  CompletionState() = default;
  CompletionState(const CompletionState&) = delete;
  CompletionState& operator=(const CompletionState&) = delete;
  virtual ~CompletionState() = default;

  [[nodiscard]] virtual Result<bool> Query() const = 0;
  [[nodiscard]] virtual Status Wait() const = 0;
};

class ExecutionState {
 public:
  ExecutionState() = default;
  ExecutionState(const ExecutionState&) = delete;
  ExecutionState& operator=(const ExecutionState&) = delete;
  virtual ~ExecutionState() = default;

  [[nodiscard]] virtual bool CanAccess(MemorySpace space) const noexcept = 0;
  [[nodiscard]] virtual Result<std::unique_ptr<CompletionState>> CopyBytes(
      MutableMemoryView destination, ConstMemoryView source,
      std::size_t byte_count) const = 0;
  [[nodiscard]] virtual Result<std::unique_ptr<CompletionState>> RecordEvent()
      const = 0;
  [[nodiscard]] virtual void* NativeExecutionHandle(
      Backend backend) const noexcept = 0;
};

class Access {
 public:
  static ExecutionContext MakeContext(
      Backend backend, Device device, Determinism determinism,
      std::shared_ptr<const ExecutionState> state) noexcept {
    return ExecutionContext(backend, device, determinism, std::move(state));
  }

  static CompletionEvent MakeEvent(
      std::unique_ptr<CompletionState> state) noexcept {
    return CompletionEvent(std::move(state));
  }

  static CompletionEvent MakeCompletedEvent() noexcept {
    return CompletionEvent(true);
  }

  [[nodiscard]] static const std::shared_ptr<const ExecutionState>& State(
      const ExecutionContext& context) noexcept {
    return context.state_;
  }
};

}  // namespace asc::internal_core_execution

#endif  // ASC_SRC_CORE_EXECUTION_INTERNAL_H_
