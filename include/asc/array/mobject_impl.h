// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/mobject_impl.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MOBJECT_IMPL_H_
#define ASC_MOBJECT_IMPL_H_

/// @file mobject_impl.h
/// @brief Implementation of MObject free function operators
///
/// This file contains the implementations of free function operators for
/// MObject (scalar on left side). Arithmetic operation implementations are
/// handled by the expression template system in expr.h.
/// It should be included at the end of mobject.h.

#include "asc/array/expr.h"
#include "asc/core/operators.h"
#include "asc/array/mobject.h"

namespace asc {

// ============================================================================
// Free Function Operators (scalar on left)
// ============================================================================

template <Arithmetic U, typename Derived>
inline auto operator-(U scalar, const MObject<Derived>& array) {
  const Derived& derived = array.derived();
  return ScalarExpr<U>(scalar, derived.GetSize(), derived.UseDevice()) -
         ObjectExpr<Derived>(derived);
}

template <Arithmetic U, typename Derived>
inline auto operator/(U scalar, const MObject<Derived>& array) {
  const Derived& derived = array.derived();
  return ScalarExpr<U>(scalar, derived.GetSize(), derived.UseDevice()) /
         ObjectExpr<Derived>(derived);
}

}  // namespace asc

#endif  // ASC_MOBJECT_IMPL_H_
