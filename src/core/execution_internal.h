#ifndef ASC_SRC_CORE_EXECUTION_INTERNAL_H_
#define ASC_SRC_CORE_EXECUTION_INTERNAL_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {
namespace internal_core_execution {

class CompletionState {
 public:
  CompletionState() = default;
  CompletionState(const CompletionState&) = delete;
  CompletionState& operator=(const CompletionState&) = delete;
  virtual ~CompletionState() = default;

  [[nodiscard]] virtual Result<bool> Query() const = 0;
  [[nodiscard]] virtual Status Wait() const = 0;
};

class CompletionAccess {
 public:
  [[nodiscard]] static CompletionEvent Completed() noexcept {
    return CompletionEvent(true);
  }

  [[nodiscard]] static CompletionEvent Make(
      std::unique_ptr<CompletionState> state) noexcept {
    return CompletionEvent(std::move(state));
  }
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
};

class ExecutionAccess {
 public:
  [[nodiscard]] static ExecutionContext Make(
      Backend backend, Device device, Determinism determinism,
      std::shared_ptr<const ExecutionState> state) noexcept {
    return ExecutionContext(backend, device, determinism, std::move(state));
  }

  [[nodiscard]] static const std::shared_ptr<const ExecutionState>& State(
      const ExecutionContext& context) noexcept {
    return context.state_;
  }
};

}  // namespace internal_core_execution
}  // namespace asc

#endif  // ASC_SRC_CORE_EXECUTION_INTERNAL_H_
