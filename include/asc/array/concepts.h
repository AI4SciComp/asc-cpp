// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/array/concepts.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_ARRAY_CONCEPTS_H_
#define ASC_ARRAY_CONCEPTS_H_

#include <type_traits>

#include "asc/core/globals.h"
#include "asc/array/forwards.h"
#include "asc/array/mshape.h"

namespace asc {

template <typename T>
struct IsDenseMArray : std::false_type {};

template <typename T, typename Shape, typename Layout>
struct IsDenseMArray<DenseMArray<T, Shape, Layout>> : std::true_type {};

template <typename T>
struct IsSparseMArray : std::false_type {};

template <typename T, typename Shape, typename Layout>
struct IsSparseMArray<SparseMArray<T, Shape, Layout>> : std::true_type {};

template <typename T>
concept DenseMArrayLike = IsDenseMArray<std::decay_t<T>>::value;

template <typename T>
concept SparseMArrayLike = IsSparseMArray<std::decay_t<T>>::value;

template <typename T>
concept MArrayLike = DenseMArrayLike<T> || SparseMArrayLike<T>;

template <typename T>
struct IsScalar : std::false_type {};

template <typename T, typename Layout>
struct IsScalar<DenseMArray<T, DShape<0>, Layout>> : std::true_type {};

template <typename T, typename Layout>
struct IsScalar<SparseMArray<T, DShape<0>, Layout>> : std::true_type {};

template <typename T>
struct IsVector : std::false_type {};

template <typename T, int N, typename Layout>
struct IsVector<DenseMArray<T, MShape<N>, Layout>> : std::true_type {};

template <typename T, int N, typename Layout>
struct IsVector<SparseMArray<T, MShape<N>, Layout>> : std::true_type {};

template <typename T>
struct IsDVector : std::false_type {};

template <typename T, typename Layout>
struct IsDVector<DenseMArray<T, DShape<1>, Layout>> : std::true_type {};

template <typename T, typename Layout>
struct IsDVector<SparseMArray<T, DShape<1>, Layout>> : std::true_type {};

template <typename T>
struct IsSVector : std::false_type {};

template <typename T, int N, typename Layout>
struct IsSVector<DenseMArray<T, MShape<N>, Layout>> : std::true_type {};

template <typename T, int N, typename Layout>
struct IsSVector<SparseMArray<T, MShape<N>, Layout>> : std::true_type {};

template <typename T>
struct IsMatrix : std::false_type {};

template <typename T, int M, int N, typename Layout>
struct IsMatrix<DenseMArray<T, MShape<M, N>, Layout>> : std::true_type {};

template <typename T, int M, int N, typename Layout>
struct IsMatrix<SparseMArray<T, MShape<M, N>, Layout>> : std::true_type {};

template <typename T>
struct IsDMatrix : std::false_type {};

template <typename T, typename Layout>
struct IsDMatrix<DenseMArray<T, DShape<2>, Layout>> : std::true_type {};

template <typename T, typename Layout>
struct IsDMatrix<SparseMArray<T, DShape<2>, Layout>> : std::true_type {};

template <typename T>
struct IsSMatrix : std::false_type {};

template <typename T, int M, int N, typename Layout>
struct IsSMatrix<DenseMArray<T, MShape<M, N>, Layout>> : std::true_type {};

template <typename T, int M, int N, typename Layout>
struct IsSMatrix<SparseMArray<T, MShape<M, N>, Layout>> : std::true_type {};

template <typename T>
struct IsTensor : std::false_type {};

template <typename T, typename Shape, typename Layout>
struct IsTensor<DenseMArray<T, Shape, Layout>> : std::true_type {};

template <typename T, typename Shape, typename Layout>
struct IsTensor<SparseMArray<T, Shape, Layout>> : std::true_type {};

template <typename T>
concept ScalarLike = Arithmetic<T> || IsScalar<std::decay_t<T>>::value;

template <typename T>
concept VectorLike =
    IsVector<std::decay_t<T>>::value || IsDVector<std::decay_t<T>>::value ||
    IsScalar<std::decay_t<T>>::value;

template <typename T>
concept DynamicVectorLike =
    IsDVector<std::decay_t<T>>::value || IsScalar<std::decay_t<T>>::value;

template <typename T>
concept StaticVectorLike =
    IsSVector<std::decay_t<T>>::value || IsScalar<std::decay_t<T>>::value;

template <typename T>
concept MatrixLike =
    IsMatrix<std::decay_t<T>>::value || IsDMatrix<std::decay_t<T>>::value ||
    VectorLike<T>;

template <typename T>
concept DynamicMatrixLike =
    IsDMatrix<std::decay_t<T>>::value || DynamicVectorLike<T>;

template <typename T>
concept StaticMatrixLike =
    IsSMatrix<std::decay_t<T>>::value || StaticVectorLike<T>;

template <typename T>
concept TensorLike = MArrayLike<T>;

template <typename T>
concept SparseMatrixLike =
    SparseMArrayLike<T> &&
    (IsMatrix<std::decay_t<T>>::value || IsDMatrix<std::decay_t<T>>::value);

template <typename T>
concept DenseMatrixLike =
    DenseMArrayLike<T> &&
    (IsMatrix<std::decay_t<T>>::value || IsDMatrix<std::decay_t<T>>::value);

template <typename T>
concept SparseVectorLike =
    SparseMArrayLike<T> &&
    (IsVector<std::decay_t<T>>::value || IsDVector<std::decay_t<T>>::value);

template <typename T>
concept DenseVectorLike =
    DenseMArrayLike<T> &&
    (IsVector<std::decay_t<T>>::value || IsDVector<std::decay_t<T>>::value);

template <typename T>
concept SparseTensorLike = SparseMArrayLike<T>;

template <typename T>
concept DenseTensorLike = DenseMArrayLike<T>;

}  // namespace asc

#endif  // ASC_ARRAY_CONCEPTS_H_
