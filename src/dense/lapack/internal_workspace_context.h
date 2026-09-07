#ifndef ASC_DENSE_LAPACK_INTERNAL_WORKSPACE_CONTEXT_H_
#define ASC_DENSE_LAPACK_INTERNAL_WORKSPACE_CONTEXT_H_

#include <span>

#include "asc/core/execution.h"
#include "asc/core/memory.h"
#include "asc/core/status.h"
#include "asc/dense/lapack/types.h"
#include "asc/dense/lapack/workspace.h"
#include "asc/dense/providers/lapack.h"

namespace asc::internal_lapack_workspace {

// The neutral validator's CPU-addressability policy does not select an
// execution context. Every nonempty supplied region, including an unused
// role, must also be admitted by this explicitly selected provider context.
// Empty-region metadata keeps its existing neutral validation semantics.
inline Status Validate(const ReferenceLapackProvider& provider,
                       const LapackWorkspacePlan& plan,
                       const LapackPlanIdentity& identity,
                       const LapackWorkspace& workspace,
                       std::span<const ConstMemoryView> operands) {
  for (const auto& region : workspace.regions) {
    if (region.size() != 0 && !provider.context().CanAccess(region.space())) {
      return Status(ErrorCode::kMemoryAccess);
    }
  }
  return ValidateLapackWorkspace(plan, identity, workspace, operands);
}

}  // namespace asc::internal_lapack_workspace

#endif  // ASC_DENSE_LAPACK_INTERNAL_WORKSPACE_CONTEXT_H_
