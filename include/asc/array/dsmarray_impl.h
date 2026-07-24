// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/dsmarray_impl.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_DSMARRAY_IMPL_H_
#define ASC_DSMARRAY_IMPL_H_

#include <algorithm>
#include <array>
#include <utility>

#include "asc/array/dsmarray.h"

namespace asc {

template <typename T, typename Shape, typename Layout>
inline DenseMArray<T, Shape, Layout>::DenseMArray() {
  if constexpr (Shape::GetDynamicRank() == 0 &&
                !std::is_same_v<Layout, LayoutStride>) {
    // Static shape (non-stride): initialize map with default shape
    map_ = MapType(Shape{});
    if (map_.GetSize() > 0) {
      data_.New(map_.GetSize());
    }
  }
  // For dynamic shapes or LayoutStride, map_ is default-constructed
}

template <typename T, typename Shape, typename Layout>
inline DenseMArray<T, Shape, Layout>::DenseMArray(MemoryType mt) : data_(mt) {
  if constexpr (Shape::GetDynamicRank() == 0 &&
                !std::is_same_v<Layout, LayoutStride>) {
    map_ = MapType(Shape{});
    if (map_.GetSize() > 0) {
      data_.New(map_.GetSize());
    }
  }
  // For dynamic shapes or LayoutStride, map_ is default-constructed
}

template <typename T, typename Shape, typename Layout>
inline DenseMArray<T, Shape, Layout>::DenseMArray(const Shape& shape)
    : map_(shape) {
  if (map_.GetSize() > 0) {
    data_.New(map_.GetSize());
  }
}

template <typename T, typename Shape, typename Layout>
inline DenseMArray<T, Shape, Layout>::DenseMArray(const Shape& shape,
                                                  MemoryType mt)
    : data_(mt), map_(shape) {
  if (map_.GetSize() > 0) {
    data_.New(map_.GetSize(), mt);
  }
}

template <typename T, typename Shape, typename Layout>
template <Integral... Extents>
inline DenseMArray<T, Shape, Layout>::DenseMArray(Extents... extents)
    : map_(Shape(extents...)) {
  if (map_.GetSize() > 0) {
    data_.New(map_.GetSize());
  }
}

template <typename T, typename Shape, typename Layout>
template <Integral... Extents>
inline DenseMArray<T, Shape, Layout>::DenseMArray(MemoryType mt,
                                                  Extents... extents)
    : data_(mt), map_(Shape(extents...)) {
  if (map_.GetSize() > 0) {
    data_.New(map_.GetSize(), mt);
  }
}

template <typename T, typename Shape, typename Layout>
inline DenseMArray<T, Shape, Layout>::DenseMArray(const DenseMArray& other)
    : map_(other.map_) {
  if (map_.GetSize() > 0) {
    data_.New(map_.GetSize(), other.data_.GetMemoryType());
    data_.CopyFrom(other.data_, map_.GetSize());
    data_.UseDevice(other.data_.UseDevice());
  }
}

// Assignment operator with broadcasting and expression template support
template <typename T, typename Shape, typename Layout>
template <typename Other>
  requires(!Arithmetic<std::decay_t<Other>>)
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator=(
    const Other& other) {
  if constexpr (ExpressionLike<Other>) {
    other.EvalTo(*this);
  } else {
    Broadcast(other, AssignOp<T>());
  }
  return *this;
}

// Compound assignment operators with DenseMArray (unified template version)
template <typename T, typename Shape, typename Layout>
template <typename Other>
  requires(!Arithmetic<std::decay_t<Other>>)
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator+=(
    const Other& other) {
  Broadcast(other, AddOp<T>());
  return *this;
}

template <typename T, typename Shape, typename Layout>
template <typename Other>
  requires(!Arithmetic<std::decay_t<Other>>)
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator-=(
    const Other& other) {
  Broadcast(other, SubOp<T>());
  return *this;
}

template <typename T, typename Shape, typename Layout>
template <typename Other>
  requires(!Arithmetic<std::decay_t<Other>>)
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator*=(
    const Other& other) {
  Broadcast(other, MulOp<T>());
  return *this;
}

template <typename T, typename Shape, typename Layout>
template <typename Other>
  requires(!Arithmetic<std::decay_t<Other>>)
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator/=(
    const Other& other) {
  Broadcast(other, DivOp<T>());
  return *this;
}

// Compound assignment operators with scalar
template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator+=(
    const U& value) {
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const bool use_dev = data_.UseDevice();
    T* x = ReadWrite(use_dev);
    const int N = GetSize();
    const T scalar = T(value);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] += scalar;);
    return *this;
  }
  T* x = HostReadWrite();
  MIterator<Shape, Layout> iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    int offset = iter.GetOffset();
    x[offset] += T(value);
  }
  return *this;
}

template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator-=(
    const U& value) {
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const bool use_dev = data_.UseDevice();
    T* x = ReadWrite(use_dev);
    const int N = GetSize();
    const T scalar = T(value);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] -= scalar;);
    return *this;
  }
  T* x = HostReadWrite();
  MIterator<Shape, Layout> iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    int offset = iter.GetOffset();
    x[offset] -= T(value);
  }
  return *this;
}

template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator*=(
    const U& value) {
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const bool use_dev = data_.UseDevice();
    T* x = ReadWrite(use_dev);
    const int N = GetSize();
    const T scalar = T(value);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] *= scalar;);
    return *this;
  }
  T* x = HostReadWrite();
  MIterator<Shape, Layout> iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    int offset = iter.GetOffset();
    x[offset] *= T(value);
  }
  return *this;
}

template <typename T, typename Shape, typename Layout>
template <Arithmetic U>
inline DenseMArray<T, Shape, Layout>& DenseMArray<T, Shape, Layout>::operator/=(
    const U& value) {
  ASC_VERIFY(value != U(0), "Division by zero.");
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const bool use_dev = data_.UseDevice();
    T* x = ReadWrite(use_dev);
    const int N = GetSize();
    const T scalar = T(value);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] /= scalar;);
    return *this;
  }
  T* x = HostReadWrite();
  MIterator<Shape, Layout> iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    int offset = iter.GetOffset();
    x[offset] /= T(value);
  }
  return *this;
}

// Unary negation operator
template <typename T, typename Shape, typename Layout>
inline DenseMArray<T, Shape, Layout> DenseMArray<T, Shape, Layout>::operator-()
    const {
  DenseMArray<T, Shape, Layout> result(GetShape());
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const bool use_dev = data_.UseDevice();
    result.UseDevice(use_dev);
    const T* x = Read(use_dev);
    T* y = result.ReadWrite(use_dev);
    const int N = GetSize();
    ASC_FORALL_SWITCH(use_dev, i, N, y[i] = -x[i];);
    return result;
  }
  const T* x = HostRead();
  T* y = result.HostReadWrite();
  MIterator<Shape, Layout> iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    int offset = iter.GetOffset();
    y[offset] = -x[offset];
  }
  return result;
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::SetShape(const Shape& shape,
                                                    MemoryType mt) {
  if (mt == data_.GetMemoryType()) {
    if (shape.GetSize() <= data_.GetCapacity()) {
      map_ = MapType(shape);
      return;
    }
  }
  const bool use_dev = data_.UseDevice();
  data_.Delete();
  if (shape.GetSize() > 0) {
    data_.New(shape.GetSize(), mt);
  } else {
    data_.Reset();
  }
  data_.UseDevice(use_dev);
  map_ = MapType(shape);
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::SetShape(const Shape& shape,
                                                    const T& val) {
  const int old_size = map_.GetSize();
  SetShape(shape);
  if (map_.GetSize() > old_size) {
    T* data = HostReadWrite();
    for (int i = old_size; i < map_.GetSize(); ++i) {
      data[i] = val;
    }
  }
}

template <typename T, typename Shape, typename Layout>
template <typename U>
inline void DenseMArray<T, Shape, Layout>::CopyTo(U* dest) const {
  const T* data = HostRead();
  for (int i = 0; i < GetSize(); ++i) {
    dest[i] = static_cast<U>(data[i]);
  }
}

template <typename T, typename Shape, typename Layout>
template <typename U>
inline void DenseMArray<T, Shape, Layout>::CopyFrom(const U* src) {
  T* data = HostWrite();
  for (int i = 0; i < GetSize(); ++i) {
    data[i] = static_cast<T>(src[i]);
  }
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::CopyFrom(
    const DenseMArray& src) {
  if constexpr (!std::is_same_v<Layout, LayoutStride> &&
                Shape::GetRank() > 0) {
    SetShape(src.GetShape());
  } else {
    ASC_VERIFY(GetSize() == src.GetSize(),
                  "Strided CopyFrom destination must already have matching "
                  "size.");
  }
  if (GetSize() == 0) return;

  const T* src_data = src.HostRead();
  T* dest_data = HostReadWrite();
  auto src_iter = src.GetIterator();
  auto dest_iter = GetIterator();
  for (; !src_iter.IsExhausted(); ++src_iter, ++dest_iter) {
    dest_data[dest_iter.GetOffset()] = src_data[src_iter.GetOffset()];
  }
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::CopyTo(DenseMArray& dest) const {
  dest.CopyFrom(*this);
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::GrowMemory(int min_size) {
  const int new_size = std::max(min_size, 2 * data_.GetCapacity());
  Memory<T> new_data(new_size, data_.GetMemoryType());
  new_data.CopyFrom(data_, map_.GetSize());
  new_data.UseDevice(data_.UseDevice());
  data_.Delete();
  data_ = new_data;
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::CheckBounds(int offset) const {
  ASC_ASSERT(
      offset >= 0 && offset < map_.GetSize(),
      "index " << offset << " out of bounds [0, " << map_.GetSize() << ")");
}

template <typename T, typename Shape, typename Layout>
template <typename... Indices>
inline void DenseMArray<T, Shape, Layout>::CheckBounds(
    Indices... indices) const {
  static_assert(sizeof...(indices) == GetRank(), "incorrect number of indices");
  int index_array[] = {static_cast<int>(indices)...};
  for (int i = 0; i < GetRank(); ++i) {
    ASC_ASSERT(index_array[i] >= 0 && index_array[i] < GetExtent(i),
                  "index " << index_array[i] << " out of bounds for dimension "
                           << i << " [0, " << GetExtent(i) << ")");
  }
}

template <typename T, typename Shape, typename Layout>
template <typename Other, typename BinaryOp>
inline void DenseMArray<T, Shape, Layout>::Broadcast(const Other& other,
                                                     BinaryOp op) {
  if constexpr (DenseMArrayLike<Other> &&
                std::is_same_v<BinaryOp, AssignOp<T>> &&
                Shape::IsDynamic() && Shape::GetRank() > 0 &&
                !std::is_same_v<Layout, LayoutStride>) {
    if (other.GetRank() == GetRank()) {
      bool same_shape = true;
      for (int d = 0; same_shape && d < GetRank(); ++d) {
        same_shape = other.GetExtent(d) == GetExtent(d);
      }
      if (!same_shape) {
        int extents[Shape::GetNDims()];
        for (int d = 0; d < GetRank(); ++d) {
          extents[d] = other.GetExtent(d);
        }
        SetShape(Shape(extents));
      }
    }
  }

  if constexpr (DenseMArrayLike<Other> &&
                !std::is_same_v<Layout, LayoutStride> &&
                std::is_same_v<Layout,
                               typename std::decay_t<Other>::LayoutType>) {
    if (GetSize() == other.GetSize()) {
      bool same_shape = other.GetRank() == GetRank();
      for (int d = 0; same_shape && d < GetRank(); ++d) {
        same_shape = other.GetExtent(d) == GetExtent(d);
      }
      if (same_shape) {
        const bool use_dev = data_.UseDevice() && other.UseDevice();
        T* x_data = ReadWrite(use_dev);
        const auto* y_data = other.Read(use_dev);
        const int N = GetSize();
        ASC_FORALL_SWITCH(use_dev, i,
                             N, x_data[i] = op(x_data[i], y_data[i]););
        return;
      }
    }
  }

  T* x_data = HostReadWrite();
  const auto* y_data = other.HostRead();

  if (other.GetRank() == 0) {
    const auto scalar_value = y_data[0];
    MIterator<Shape, Layout> iter = GetIterator();
    for (; !iter.IsExhausted(); ++iter) {
      const int offset = iter.GetOffset();
      x_data[offset] = op(x_data[offset], scalar_value);
    }
    return;
  }

  if (GetSize() == other.GetSize()) {
    bool same_shape = other.GetRank() == GetRank();
    for (int d = 0; same_shape && d < GetRank(); ++d) {
      same_shape = other.GetExtent(d) == GetExtent(d);
    }

    if (same_shape) {
      constexpr int kRank = Shape::GetRank();
      auto lhs_iter = GetIterator();
      for (; !lhs_iter.IsExhausted(); ++lhs_iter) {
        std::array<int, (kRank > 0 ? kRank : 1)> index{};
        for (int d = 0; d < kRank; ++d) index[d] = lhs_iter[d];
        const int lhs_offset = lhs_iter.GetOffset();
        const int rhs_offset = other.GetMap().Fold(index.data());
        x_data[lhs_offset] = op(x_data[lhs_offset], y_data[rhs_offset]);
      }
    } else {
      auto lhs_iter = GetIterator();
      auto rhs_iter = other.GetIterator();
      for (; !lhs_iter.IsExhausted(); ++lhs_iter, ++rhs_iter) {
        const int lhs_offset = lhs_iter.GetOffset();
        const int rhs_offset = rhs_iter.GetOffset();
        x_data[lhs_offset] = op(x_data[lhs_offset], y_data[rhs_offset]);
      }
    }
    return;
  }

  ASC_VERIFY(other.GetRank() == 1 || other.GetRank() == GetRank(),
                "Broadcasting: incompatible ranks");

  if (other.GetRank() == 1) {
    ASC_VERIFY(other.GetSize() == GetExtent(0),
                  "Broadcasting: vector size must match first dimension");

    MIterator<Shape, Layout> iter = GetIterator();
    for (; !iter.IsExhausted(); ++iter) {
      const int lhs_offset = iter.GetOffset();
      const int vec_index = iter[0];
      const int rhs_offset = other.GetMap().Fold(&vec_index);
      x_data[lhs_offset] = op(x_data[lhs_offset], y_data[rhs_offset]);
    }
    return;
  }

  bool broadcastable = true;
  for (int d = 0; d < GetRank(); ++d) {
    if (GetExtent(d) != other.GetExtent(d) && other.GetExtent(d) != 1) {
      broadcastable = false;
      break;
    }
  }
  ASC_VERIFY(broadcastable, "Broadcasting: dimensions must be compatible");

  constexpr int kRank = Shape::GetRank();
  MIterator<Shape, Layout> iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    std::array<int, (kRank > 0 ? kRank : 1)> other_index{};
    for (int d = 0; d < kRank; ++d) {
      other_index[d] = (other.GetExtent(d) == 1) ? 0 : iter[d];
    }
    const int lhs_offset = iter.GetOffset();
    const int rhs_offset = other.GetMap().Fold(other_index.data());
    x_data[lhs_offset] = op(x_data[lhs_offset], y_data[rhs_offset]);
  }
}

template <typename T, typename Shape, typename Layout>
void DenseMArray<T, Shape, Layout>::PrintImpl(std::ostream& os, int axis,
                                              int offset, int indent) const {
  if constexpr (GetRank() == 0) {
    PPrint(os, HostRead()[0]);
    if (axis == 0) os << "\n";
    return;
  }

  const T* data = HostRead();
  const int limit = PrintFlag<T>::kLimit;
  int size = map_.GetExtent(axis);
  int ellipsis_start = limit / 2;
  int ellipsis_end = size - (limit / 2);
  if (axis == GetRank() - 1) {
    os << "[";
    for (int i = 0; i < size; ++i) {
      if (size > limit && i == ellipsis_start) {
        os << "...";
        i = ellipsis_end - 1;
      } else {
        PPrint(os, data[offset + i * map_.GetStride(axis)]);
        if (i < size - 1) os << ", ";
      }
    }
    os << "]";
  } else {
    os << "[";
    for (int i = 0; i < size; ++i) {
      if (size > limit && i == ellipsis_start) {
        i = ellipsis_end - 1;
        if (i > 0) {
          os << ",\n";
          for (int j = 0; j < indent + 1; ++j) {
            os << ' ';
          }
          os << "...\n";
          for (int j = 0; j < indent + 1; ++j) {
            os << ' ';
          }
        }
      } else {
        if (i > 0) {
          os << ",\n";
          for (int j = 0; j < indent + 1; ++j) {
            os << ' ';
          }
        }
      }
      PrintImpl(os, axis + 1, offset + i * map_.GetStride(axis), indent + 1);
    }
    os << "]";
  }
  if (axis == 0) os << "\n";
}

template <typename T, typename Shape, typename Layout>
void DenseMArray<T, Shape, Layout>::Save(std::ostream& out, int fmt) const {
  if (fmt == 0) {
    out << GetRank() << " ";
    for (int i = 0; i < GetRank(); ++i) {
      out << GetExtent(i) << " ";
    }
    out << std::endl;
  }

  const T* data = HostRead();
  auto iter = GetIterator();
  int count = 0;
  for (; !iter.IsExhausted(); ++iter, ++count) {
    out << data[iter.GetOffset()];
    if (count < GetSize() - 1) out << " ";
  }
  out << std::endl;
}

template <typename T, typename Shape, typename Layout>
void DenseMArray<T, Shape, Layout>::Load(std::istream& in, int fmt) {
  if (fmt == 0) {
    int rank;
    in >> rank;
    ASC_VERIFY(in.good(), "failed to read array rank");
    ASC_VERIFY(rank == GetRank(), "rank mismatch");

    int extents[Shape::GetNDims()];
    for (int i = 0; i < GetRank(); ++i) {
      in >> extents[i];
      ASC_VERIFY(in.good(), "failed to read array extent");
    }

    if constexpr (Shape::GetRank() == 0) {
      // Rank-0 scalar has no extents to load.
    } else if constexpr (Shape::IsDynamic()) {
      for (int i = 0; i < GetRank(); ++i) {
        ASC_VERIFY(extents[i] >= 0, "invalid extent in input stream");
      }
      SetShape(Shape(extents));
    } else {
      for (int i = 0; i < GetRank(); ++i) {
        ASC_VERIFY(extents[i] == GetExtent(i),
                      "static shape extent mismatch in input stream");
      }
    }
  }

  T* data = HostWrite();
  auto iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    in >> data[iter.GetOffset()];
    ASC_VERIFY(in.good(), "failed to read array value");
  }
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::SetZeros() {
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const int N = map_.GetSize();
    const bool use_dev = data_.UseDevice();
    T* x = Write(use_dev);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] = T(0););
    return;
  }
  T* x = HostReadWrite();
  auto iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    x[iter.GetOffset()] = T(0);
  }
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::SetOnes() {
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const int N = map_.GetSize();
    const bool use_dev = data_.UseDevice();
    T* x = Write(use_dev);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] = T(1););
    return;
  }
  T* x = HostReadWrite();
  auto iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    x[iter.GetOffset()] = T(1);
  }
}

/// @brief Set all elements to a constant value
/// @param value Constant value to set
template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::SetConstants(const T& value) {
  if constexpr (!std::is_same_v<Layout, LayoutStride>) {
    const int N = map_.GetSize();
    const bool use_dev = data_.UseDevice();
    T* x = Write(use_dev);
    ASC_FORALL_SWITCH(use_dev, i, N, x[i] = value;);
    return;
  }
  T* x = HostReadWrite();
  auto iter = GetIterator();
  for (; !iter.IsExhausted(); ++iter) {
    x[iter.GetOffset()] = value;
  }
}

template <typename T, typename Shape, typename Layout>
inline void DenseMArray<T, Shape, Layout>::SetIdentity() {
  T* x = HostReadWrite();
  if constexpr (GetRank() == 0) {
    x[0] = T(1);
    return;
  }

  // Create iterator based on whether shape is static or dynamic
  MIterator<Shape, Layout> iter = GetIterator();
  for (int i = iter.GetOffset(); !iter.IsExhausted();
       i = (++iter).GetOffset()) {
    // Check if all indices are equal (diagonal element)
    bool is_diagonal = true;
    const int first_idx = iter[0];
    for (int dim = 1; dim < GetRank(); ++dim) {
      if (iter[dim] != first_idx) {
        is_diagonal = false;
        break;
      }
    }
    x[i] = is_diagonal ? T(1) : T(0);
  }
}

template <typename T, typename Shape, typename Layout>
inline MArrayView<T, TransposedShape<Shape>>
DenseMArray<T, Shape, Layout>::Transpose() const {
  using ResultShape = TransposedShape<Shape>;

  ResultShape trans_shape = CreateReverseShape<Shape>(GetShape());

  // Create transposed strides (reverse order)
  constexpr int kRank = GetRank();
  int trans_strides[kRank];
  for (int i = 0; i < kRank; ++i) {
    trans_strides[i] = map_.GetStride(kRank - 1 - i);
  }

  // Create strided layout map with transposed shape and strides
  typename LayoutStride::template Map<ResultShape> trans_map(trans_shape,
                                                             trans_strides);

  // Create result with LayoutStride
  MArrayView<T, ResultShape> result;
  result.data_.MakeAlias(data_, 0, GetSize());
  result.map_ = trans_map;

  return result;
}

template <typename T, typename Shape, typename Layout>
template <size_t... Is>
inline MArrayView<T, PermutedShape<Shape, Is...>>
DenseMArray<T, Shape, Layout>::Permute() const {
  using ResultShape = PermutedShape<Shape, Is...>;

  ASC_VERIFY(sizeof...(Is) == GetRank(),
                "Permutation size must equal array rank");

  // Verify permutation is valid (contains each index 0..rank-1 exactly once)
  constexpr int perm[] = {Is...};
  bool seen[GetRank()] = {false};
  for (int i = 0; i < GetRank(); ++i) {
    int p = perm[i];
    ASC_VERIFY(p >= 0 && p < GetRank(),
                  "Permutation index out of range: " + std::to_string(p));
    ASC_VERIFY(!seen[p],
                  "Duplicate index in permutation: " + std::to_string(p));
    seen[p] = true;
  }

  ResultShape perm_shape = CreatePermuteShape<Shape, Is...>(GetShape());

  // Create permuted strides using compile-time permutation
  constexpr int kRank = GetRank();
  int perm_strides[kRank];
  for (int i = 0; i < kRank; ++i) {
    perm_strides[i] = map_.GetStride(perm[i]);
  }

  // Create strided layout map
  typename LayoutStride::template Map<ResultShape> perm_map(perm_shape,
                                                            perm_strides);

  // Create result
  MArrayView<T, ResultShape> result;
  result.data_.MakeAlias(data_, 0, GetSize());
  result.map_ = perm_map;

  return result;
}

template <typename T, typename Shape, typename Layout>
template <HighRankShape S>
inline MArrayView<T, DShape<S::GetRank() - 1>>
DenseMArray<T, Shape, Layout>::Slice(int dim, int index) const {
  ASC_VERIFY(dim >= 0 && dim < GetRank(),
                "Slice dimension out of bounds: " + std::to_string(dim) +
                    " (must be in [0, " + std::to_string(GetRank()) + "))");
  ASC_VERIFY(index >= 0 && index < GetExtent(dim),
                "Slice index out of bounds: " + std::to_string(index) +
                    " (must be in [0, " + std::to_string(GetExtent(dim)) +
                    "))");

  constexpr int ResultRank = S::GetRank() - 1;
  using ResultShape = DShape<ResultRank>;

  // Build new shape with dimension `dim` removed
  int new_extents[ResultRank];
  int new_strides[ResultRank];
  int out_idx = 0;
  for (int i = 0; i < GetRank(); ++i) {
    if (i != dim) {
      new_extents[out_idx] = GetExtent(i);
      new_strides[out_idx] = map_.GetStride(i);
      out_idx++;
    }
  }

  // Create result shape
  ResultShape result_shape = ResultShape(new_extents);

  // Create result strides
  int strides[ResultRank];
  for (int i = 0; i < ResultRank; ++i) {
    strides[i] = new_strides[i];
  }

  typename LayoutStride::template Map<ResultShape> map(result_shape, strides);

  // Create result view
  MArrayView<T, ResultShape> result;

  // Compute offset for the sliced index
  const int offset = index * map_.GetStride(dim);

  // For the MakeAlias, we need the memory range that will be accessed.
  // The sliced view has its own shape/stride, and it will access elements
  // through its stride pattern starting from offset.
  // The maximum linear index that can be accessed is:
  // offset + sum((extent[i]-1) * stride[i]) for all i in result dimensions
  int max_offset = offset;
  for (int i = 0; i < ResultRank; ++i) {
    max_offset += (new_extents[i] - 1) * new_strides[i];
  }
  // Size of memory range that might be accessed
  int alias_size = max_offset - offset + 1;

  result.data_.MakeAlias(data_, offset, alias_size);
  result.map_ = map;

  return result;
}

// ============================================================================
// Reduction Operations (DenseMArray-specific, uses MIterator for
// layout-awareness)
// ============================================================================

template <typename T, typename Shape, typename Layout>
inline T DenseMArray<T, Shape, Layout>::Norm(int p) const {
  const int N = GetSize();
  ASC_VERIFY(N > 0, "array is empty");
  if (p == 0) {
    // L0 "norm": total number of elements
    return T(N);
  }

  const T* x = HostRead();
  MIterator<Shape, Layout> iter = GetIterator();
  T result = T(0);
  for (; !iter.IsExhausted(); ++iter) {
    const int i = iter.GetOffset();
    if (p == 1) {
      // L1 norm: sum of absolute values
      result += std::abs(x[i]);
    } else if (p == 2) {
      // L2 norm: Euclidean norm
      result += x[i] * x[i];
    } else if (p == -1) {
      // L-infinity norm: maximum absolute value
      T abs_val = std::abs(x[i]);
      if (abs_val > result) result = abs_val;
    } else if (p > 0) {
      // General Lp norm
      result += std::pow(std::abs(x[i]), T(p));
    }
  }
  if (p == 2) {
    return std::sqrt(result);
  } else if (p > 0) {
    return std::pow(result, T(1) / T(p));
  } else {
    return result;
  }
}

template <typename T, typename Shape, typename Layout>
inline T DenseMArray<T, Shape, Layout>::Sum() const {
  const int N = GetSize();
  ASC_VERIFY(N > 0, "array is empty");
  const T* x = HostRead();
  MIterator<Shape, Layout> iter = GetIterator();
  T result = T(0);
  for (; !iter.IsExhausted(); ++iter) {
    const int i = iter.GetOffset();
    result += x[i];
  }
  return result;
}

template <typename T, typename Shape, typename Layout>
inline T DenseMArray<T, Shape, Layout>::Product() const {
  const int N = GetSize();
  ASC_VERIFY(N > 0, "array is empty");
  const T* x = HostRead();
  MIterator<Shape, Layout> iter = GetIterator();
  T result = T(1);
  for (; !iter.IsExhausted(); ++iter) {
    const int i = iter.GetOffset();
    if constexpr (std::is_same_v<T, bool>) {
      result = result && x[i];
    } else {
      result *= x[i];
    }
  }
  return result;
}

template <typename T, typename Shape, typename Layout>
inline T DenseMArray<T, Shape, Layout>::Mean() const {
  return Sum() / T(GetSize());
}

template <typename T, typename Shape, typename Layout>
inline T DenseMArray<T, Shape, Layout>::Maximum() const {
  const int N = GetSize();
  ASC_VERIFY(N > 0, "array is empty");
  const T* x = HostRead();
  MIterator<Shape, Layout> iter = GetIterator();
  int i = iter.GetOffset();
  T result = x[i];
  for (; !iter.IsExhausted(); ++iter) {
    i = iter.GetOffset();
    if (x[i] > result) result = x[i];
  }
  return result;
}

template <typename T, typename Shape, typename Layout>
inline T DenseMArray<T, Shape, Layout>::Minimum() const {
  const int N = GetSize();
  ASC_VERIFY(N > 0, "array is empty");
  const T* x = HostRead();
  MIterator<Shape, Layout> iter = GetIterator();
  int i = iter.GetOffset();
  T result = x[i];
  for (; !iter.IsExhausted(); ++iter) {
    i = iter.GetOffset();
    if (x[i] < result) result = x[i];
  }
  return result;
}

template <typename T, typename Shape, typename Layout>
template <typename Other>
inline T DenseMArray<T, Shape, Layout>::Dot(const Other& other) const {
  const int N = GetSize();
  ASC_VERIFY(N > 0, "array is empty");
  ASC_VERIFY(N == other.GetSize(), "tensor size mismatch for dot product");
  const T* xdata = HostRead();
  const auto* ydata = other.HostRead();
  T result = T(0);
  bool same_shape = other.GetRank() == GetRank();
  for (int d = 0; same_shape && d < GetRank(); ++d) {
    same_shape = other.GetExtent(d) == GetExtent(d);
  }
  if (same_shape) {
    constexpr int kRank = Shape::GetRank();
    auto x_iter = GetIterator();
    for (; !x_iter.IsExhausted(); ++x_iter) {
      std::array<int, (kRank > 0 ? kRank : 1)> index{};
      for (int d = 0; d < kRank; ++d) index[d] = x_iter[d];
      result += xdata[x_iter.GetOffset()] *
                ydata[other.GetMap().Fold(index.data())];
    }
  } else {
    auto x_iter = GetIterator();
    auto y_iter = other.GetIterator();
    for (; !x_iter.IsExhausted(); ++x_iter, ++y_iter) {
      result += xdata[x_iter.GetOffset()] * ydata[y_iter.GetOffset()];
    }
  }
  return result;
}

}  // namespace asc

#endif  // ASC_DSMARRAY_IMPL_H_
