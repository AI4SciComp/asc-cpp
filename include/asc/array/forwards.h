// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/array/forwards.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_ARRAY_FORWARDS_H_
#define ASC_ARRAY_FORWARDS_H_

/// @file forwards.h
/// @brief Forward declarations for the generic array object hierarchy.
///
/// This header intentionally contains declarations only. It lets concepts,
/// aliases, and expression templates refer to array types without including
/// the full dense/sparse implementations.

namespace asc {

template <typename Derived>
class MObject;

template <typename T, typename Shape, typename Layout>
class DenseMArray;

template <typename T, typename Shape, typename Layout>
class SparseMArray;

}  // namespace asc

#endif  // ASC_ARRAY_FORWARDS_H_
