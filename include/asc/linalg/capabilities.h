// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#ifndef ASC_LINALG_CAPABILITIES_H_
#define ASC_LINALG_CAPABILITIES_H_

#include <string_view>

#include "asc/core/config.h"
#include "asc/core/execution_context.h"
#include "asc/core/status.h"

namespace asc {

/// @brief Canonical linear-algebra operations available in Linalg M1.
enum class LinalgOperation {
  kCopy,
  kScal,
  kAxpy,
  kDot,
  kNrm2,
  kGemv,
  kGemm,
};

/// @brief Immutable description of the selected Linalg operation provider.
class ASC_EXPORT LinalgCapabilities {
 public:
  /// @brief Return the backend that executes supported operations.
  BackendKind GetBackend() const noexcept;

  /// @brief Return the stable provider diagnostic name.
  std::string_view GetProviderName() const noexcept;

  /// @brief Whether the provider uses a deterministic operation order.
  bool IsDeterministic() const noexcept;

  /// @brief Whether the provider implements an operation.
  bool Supports(LinalgOperation operation) const noexcept;

 private:
  LinalgCapabilities() = default;

  friend Result<LinalgCapabilities> GetLinalgCapabilities(
      const ExecutionContext& context);
};

/// @brief Select and query the provider used by canonical Linalg operations.
ASC_EXPORT Result<LinalgCapabilities> GetLinalgCapabilities(
    const ExecutionContext& context);

}  // namespace asc

#endif  // ASC_LINALG_CAPABILITIES_H_
