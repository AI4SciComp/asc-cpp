// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_CORE_EXECUTION_CONTEXT_H_
#define ASC_CORE_EXECUTION_CONTEXT_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "asc/core/config.h"
#include "asc/core/event.h"
#include "asc/core/memory_resource.h"
#include "asc/core/status.h"

namespace asc {

/// @brief Policy for operation fallback when a preferred provider is absent.
enum class FallbackPolicy : std::uint8_t {
  kDisallow = 0,
  kSameSpaceReference,
};

/// @brief Requested determinism strength for operation providers.
enum class DeterminismPolicy : std::uint8_t {
  kBestEffort = 0,
  kRequireDeterministic,
};

/// @brief General execution and memory capabilities exposed by a context.
enum class ExecutionCapability : std::uint8_t {
  kSynchronous = 0,
  kAsynchronous,
  kHostMemory,
  kPinnedHostMemory,
  kDeviceMemory,
  kManagedMemory,
};

/// @brief Immutable options used to construct an execution context.
struct ExecutionContextOptions {
  /// Requested provider backend.
  BackendKind backend = BackendKind::kSerial;

  /// Provider device identifier. Serial M1 accepts only zero.
  int device_id = 0;

  /// Fallback policy; implicit transfer is never permitted.
  FallbackPolicy fallback = FallbackPolicy::kDisallow;

  /// Determinism requested from operation providers.
  DeterminismPolicy determinism =
      DeterminismPolicy::kRequireDeterministic;
};

namespace detail {
class ExecutionContextState;
}  // namespace detail

/// @brief Cheap copyable handle to immutable execution-provider state.
///
/// Core M1 implements only a synchronous serial provider. OpenMP and CUDA
/// requests return unavailable status rather than selecting legacy backends.
class ASC_EXPORT ExecutionContext {
 public:
  /// @brief Construct a deterministic serial execution context.
  static ExecutionContext Serial();

  /// @brief Validate options and construct a supported execution context.
  static Result<ExecutionContext> Create(
      const ExecutionContextOptions& options);

  /// @brief Share immutable state with another context handle.
  ExecutionContext(const ExecutionContext&) noexcept = default;

  /// @brief Share immutable state by copy assignment.
  ExecutionContext& operator=(const ExecutionContext&) noexcept = default;

  /// @brief Release this context handle.
  ~ExecutionContext() = default;

  /// @brief Return the selected backend.
  BackendKind GetBackend() const noexcept;

  /// @brief Return the selected provider device identifier.
  int GetDeviceId() const noexcept;

  /// @brief Return the context fallback policy.
  FallbackPolicy GetFallbackPolicy() const noexcept;

  /// @brief Return the context determinism policy.
  DeterminismPolicy GetDeterminismPolicy() const noexcept;

  /// @brief Test whether the provider exposes a general capability.
  bool Supports(ExecutionCapability capability) const noexcept;

  /// @brief Return the allocation resource for a supported memory space.
  ///
  /// Unsupported spaces fail before any allocation or pointer acquisition.
  Result<MemoryResourcePtr> GetMemoryResource(MemorySpace space) const;

  /// @brief Wait for all provider work issued through this context.
  Status Synchronize() const;

 private:
  explicit ExecutionContext(
      std::shared_ptr<const detail::ExecutionContextState> state)
      : state_(std::move(state)) {}

  std::shared_ptr<const detail::ExecutionContextState> state_;
};

}  // namespace asc

#endif  // ASC_CORE_EXECUTION_CONTEXT_H_
