// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/marray.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MARRAY_H_
#define ASC_MARRAY_H_

#include "asc/array/concepts.h"
#include "asc/array/mobject.h"
#include "asc/array/dsmarray.h"
#include "asc/array/spmarray.h"

namespace asc {

template <typename T, typename Layout = DefaultLayout>
using Scalar = DenseMArray<T, DShape<0>, Layout>;

template <typename T, typename Layout = DefaultSparseLayout>
using SpScalar = SparseMArray<T, DShape<0>, Layout>;

template <typename T, int Rank, typename Layout = DefaultLayout>
using DTensor = DenseMArray<T, DShape<Rank>, Layout>;

template <typename T, int Rank, typename Layout = DefaultSparseLayout>
using SpDTensor = SparseMArray<T, DShape<Rank>, Layout>;

template <typename T, typename Layout = DefaultLayout>
using DVector = DTensor<T, 1, Layout>;

template <typename T, typename Layout = DefaultSparseLayout>
using SpDVector = SpDTensor<T, 1, Layout>;

template <typename T, typename Layout = DefaultLayout>
using DMatrix = DTensor<T, 2, Layout>;

template <typename T, typename Layout = DefaultSparseLayout>
using SpDMatrix = SpDTensor<T, 2, Layout>;

template <typename T, typename Layout = DefaultLayout, int... Extents>
using STensor = DenseMArray<T, MShape<Extents...>, Layout>;

template <typename T, typename Layout = DefaultSparseLayout, int... Extents>
using SpSTensor = SparseMArray<T, MShape<Extents...>, Layout>;

template <typename T, int N, typename Layout = DefaultLayout>
using SVector = STensor<T, Layout, N>;

template <typename T, int N, typename Layout = DefaultSparseLayout>
using SpSVector = SpSTensor<T, Layout, N>;

template <typename T, int M, int N, typename Layout = DefaultLayout>
using SMatrix = STensor<T, Layout, M, N>;

template <typename T, int M, int N, typename Layout = DefaultSparseLayout>
using SpSMatrix = SpSTensor<T, Layout, M, N>;

template <typename T>
using ScalarView = MArrayView<T, DShape<0>>;

template <typename T>
using SpScalarView = SpMArrayView<T, DShape<0>>;

template <typename T, int Rank>
using DTensorView = MArrayView<T, DShape<Rank>>;

template <typename T, int Rank>
using SpDTensorView = SpMArrayView<T, DShape<Rank>>;

template <typename T>
using DVectorView = DTensorView<T, 1>;

template <typename T>
using SpDVectorView = SpDTensorView<T, 1>;

template <typename T>
using DMatrixView = DTensorView<T, 2>;

template <typename T>
using SpDMatrixView = SpDTensorView<T, 2>;

template <typename T, int... Extents>
using STensorView = MArrayView<T, MShape<Extents...>>;

template <typename T, int... Extents>
using SpSTensorView = SpMArrayView<T, MShape<Extents...>>;

template <typename T, int N>
using SVectorView = STensorView<T, N>;

template <typename T, int N>
using SpSVectorView = SpSTensorView<T, N>;

template <typename T, int M, int N>
using SMatrixView = STensorView<T, M, N>;

template <typename T, int M, int N>
using SpSMatrixView = SpSTensorView<T, M, N>;

}  // namespace asc

#endif  // ASC_MARRAY_H_
