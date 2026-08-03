#ifndef ASC_CORE_EXECUTION_H_
#define ASC_CORE_EXECUTION_H_

/**
 * @file
 * @brief Public Core declarations for ASCCpp 0.9.0.
 *
 * Generated public contract documentation baseline for ASCCpp 0.9.0.
 * Every declaration below is governed by the module, ownership, failure,
 * memory-placement, numerical, concurrency, and package contracts linked
 * from the generated API reference.
 * @ingroup asc_core
 */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "asc/core/export.h"
#include "asc/core/memory.h"
#include "asc/core/result.h"
#include "asc/core/status.h"

namespace asc {

namespace internal_core_execution {

class Access;
class CompletionState;
class ExecutionState;

}  // namespace internal_core_execution

/**
 * @brief Selects the execution backend.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
enum class Backend : std::uint8_t {
  kSerial = 0,  ///< Synchronous provider-free CPU execution.
  kCuda = 1,    ///< CUDA execution; experimental in ASCCpp 0.9.0.
  kHip = 2,     ///< HIP execution identifier; no 0.9.0 provider is implemented.
  kSycl = 3,  ///< SYCL execution identifier; no 0.9.0 provider is implemented.
};

/**
 * @brief Performs the public BackendName operation defined by the Core
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] backend The backend value required by this contract.
 * @return The documented value; references and views do not extend owner
 * lifetime.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT const char* BackendName(Backend backend) noexcept;

/**
 * @brief Identifies one execution device.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
struct ASC_CORE_EXPORT Device {
  /**
   * @brief Stores the backend value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  Backend backend = Backend::kSerial;
  /**
   * @brief Stores the ordinal value for this contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  std::int32_t ordinal = 0;

  /**
   * @brief Performs the public Serial operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  static Device Serial() noexcept;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  friend bool operator==(const Device&, const Device&) = default;
};

class CompletionEvent;

/**
 * @brief Selects the public Determinism policy.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @ingroup asc_core
 */
enum class Determinism : std::uint8_t {
  kDeterministic = 0,   ///< Selects deterministic behavior.
  kBackendDefault = 1,  ///< Selects backend default behavior.
};

/**
 * @brief Describes backend execution, accessibility, and native ordering state.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class ExecutionContext {
 public:
  /**
   * @brief Performs the public Serial operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT ExecutionContext Serial() noexcept;
  /**
   * @brief Validates inputs and creates the requested Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] backend The backend value required by this contract.
   * @param[in] device The device value required by this contract.
   * @param[in] determinism The determinism value required by this contract.
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  static ASC_CORE_EXPORT Result<ExecutionContext> Create(
      Backend backend, Device device,
      Determinism determinism = Determinism::kDeterministic);

  /**
   * @brief Performs the public backend operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] Backend backend() const noexcept { return backend_; }
  /**
   * @brief Performs the public device operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] Device device() const noexcept { return device_; }
  /**
   * @brief Performs the public determinism operation defined by the Core
   * contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] Determinism determinism() const noexcept {
    return determinism_;
  }
  /**
   * @brief Reports whether the documented CanAccess condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] space The space value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT bool CanAccess(
      MemorySpace space) const noexcept;

 private:
  /**
   * @brief Performs the public Access operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  friend class internal_core_execution::Access;
  friend ASC_CORE_EXPORT Result<CompletionEvent> CopyBytes(
      const ExecutionContext& context, MutableMemoryView destination,
      ConstMemoryView source, std::size_t byte_count);

  ASC_CORE_EXPORT ExecutionContext(
      Backend backend, Device device, Determinism determinism,
      std::shared_ptr<const internal_core_execution::ExecutionState> state =
          nullptr) noexcept;

  Backend backend_;
  Device device_;
  Determinism determinism_;
  std::shared_ptr<const internal_core_execution::ExecutionState> state_;
};

/**
 * @brief Owns a backend completion token for ordered asynchronous work.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 * @ingroup asc_core
 */
class CompletionEvent {
 public:
  /**
   * @brief Constructs a CompletionEvent with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  CompletionEvent(const CompletionEvent&) = delete;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  CompletionEvent& operator=(const CompletionEvent&) = delete;
  /**
   * @brief Constructs a CompletionEvent with the documented ownership and
   * validity state.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] other The other value required by this contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT CompletionEvent(CompletionEvent&& other) noexcept;
  /**
   * @brief Replaces this object's state while preserving ownership invariants.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @param[in] other The other value required by this contract.
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT CompletionEvent& operator=(CompletionEvent&& other) noexcept;
  /**
   * @brief Releases owned resources after required completion/lifetime
   * conditions.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   * @ingroup asc_core
   */
  ASC_CORE_EXPORT ~CompletionEvent();

  /**
   * @brief Reports whether the documented valid condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool valid() const noexcept { return valid_; }
  /**
   * @brief Reports whether the documented is_complete condition holds.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The documented value; references and views do not extend owner
   * lifetime.
   * @ingroup asc_core
   */
  [[nodiscard]] bool is_complete() const noexcept {
    return valid_ && complete_;
  }
  /**
   * @brief Performs the public Query operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return The value on success, or a non-OK Status describing validation,
   * access, allocation, provider, or numerical failure.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Result<bool> Query() const;
  /**
   * @brief Performs the wait state transition defined by this Core object.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @return OK on success; otherwise a stable failure category with optional
   * diagnostics.
   * @ingroup asc_core
   */
  [[nodiscard]] ASC_CORE_EXPORT Status Wait() const;

 private:
  /**
   * @brief Performs the public Access operation defined by the Core contract.
   *
   * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
   * semantics follow the public Core module contract.
   *
   * @ingroup asc_core
   */
  friend class internal_core_execution::Access;
  friend ASC_CORE_EXPORT Result<CompletionEvent> CopyBytes(
      const ExecutionContext& context, MutableMemoryView destination,
      ConstMemoryView source, std::size_t byte_count);

  explicit ASC_CORE_EXPORT CompletionEvent(bool complete) noexcept;
  explicit ASC_CORE_EXPORT CompletionEvent(
      std::unique_ptr<internal_core_execution::CompletionState> state) noexcept;

  bool valid_ = false;
  mutable bool complete_ = false;
  std::unique_ptr<internal_core_execution::CompletionState> state_;
};

/**
 * @brief Computes the CopyBytes operation defined by the Core numerical
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @param[in] source Input source, valid and accessible for the operation.
 * @param[in] byte_count The byte count value required by this contract.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
ASC_CORE_EXPORT Result<CompletionEvent> CopyBytes(
    const ExecutionContext& context, MutableMemoryView destination,
    ConstMemoryView source, std::size_t byte_count);

/**
 * @brief Computes the CopyBytes operation defined by the Core numerical
 * contract.
 *
 * Ownership, lifetime, failure, memory-placement, aliasing, and concurrency
 * semantics follow the public Core module contract.
 *
 * @param[in] context Execution backend and accessibility/order contract.
 * @param[out] destination Destination storage with the required size and
 * accessibility.
 * @param[in] source Input source, valid and accessible for the operation.
 * @return The value on success, or a non-OK Status describing validation,
 * access, allocation, provider, or numerical failure.
 * @ingroup asc_core
 */
inline Result<CompletionEvent> CopyBytes(const ExecutionContext& context,
                                         MutableMemoryView destination,
                                         ConstMemoryView source) {
  return CopyBytes(context, destination, source, source.size());
}

}  // namespace asc

#endif  // ASC_CORE_EXECUTION_H_
