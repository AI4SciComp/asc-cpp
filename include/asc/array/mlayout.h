// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/mlayout.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MLAYOUT_H_
#define ASC_MLAYOUT_H_

#include <type_traits>

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/array/mshape.h"

namespace asc {

// ============================================================================
// Layout padding policy
// ============================================================================

/// @brief Boundary treatment used by dense layout maps.
enum class PaddingType {
  kNone,       ///< No padding. Out-of-bounds indices are invalid.
  kEmpty,      ///< Out-of-bounds indices map to -1.
  kWrap,       ///< Periodic padding.
  kEdge,       ///< Clamp out-of-bounds indices to the nearest edge.
  kReflect,    ///< Reflect without repeating edge values.
  kSymmetric,  ///< Reflect while repeating edge values.
};

/// @brief Base class for all layout mapping policies (CRTP)
///
/// LayoutMapBase provides common functionality for all Layout::Map classes:
/// - Shape and stride storage
/// - Multi -> linear mapping
/// - Linear -> multi mapping (delegated to derived)
/// - Common query methods
///
/// @tparam Derived The derived Map class (LayoutLeft::Map, LayoutRight::Map,
/// etc.)
/// @tparam Shape The shape type
/// @tparam Padding Boundary padding policy
template <typename Shape, PaddingType Padding, typename Derived>
class LayoutMapBase {
 public:
  ASC_STATIC_ASSERT(Shape::IsSupported(),
                       "Layout maps support only fully static or fully dynamic "
                       "MShape extents.");

  /// @brief Get the rank (number of dimensions)
  /// @return Rank
  static ASC_HOST_DEVICE constexpr int GetRank() { return Shape::GetRank(); }

  /// @brief Get the number of dimensions
  /// @return Number of dimensions
  static ASC_HOST_DEVICE constexpr int GetNDims() {
    return Shape::GetNDims();
  }

  // ========== Shape Accessors ==========

  /// @brief Get the shape object
  /// @return Shape object
  ASC_HOST_DEVICE constexpr const Shape& GetShape() const { return shape_; }

  /// @brief Get extent in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  ASC_HOST_DEVICE constexpr int GetExtent(int dim) const {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(dim < GetRank(), "Dimension index out of bounds");
#endif
    return shape_.GetExtent(dim);
  }

  /// @brief Get total size (product of all extents)
  /// @return Total size
  ASC_HOST_DEVICE constexpr int GetSize() const { return shape_.GetSize(); }

  // ========== Stride Accessors ==========

  /// @brief Get stride in dimension dim
  /// @param dim Dimension index (0 to GetRank()-1)
  /// @return Stride in dimension dim
  ASC_HOST_DEVICE constexpr int GetStride(int dim) const {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(dim < GetRank(), "Dimension index out of bounds");
#endif
    return strides_[dim];
  }

  /// @brief Get all strides as array
  /// @return Stride array
  ASC_HOST_DEVICE constexpr const int* GetStrides() const {
    return strides_;
  }

  // ========== Multi → Linear Mapping ==========

  /// @brief Fold index tuple to linear offset
  /// @tparam Indices Variadic index types
  /// @param indices Integral indices
  /// @return Linear memory offset
  template <Integral... Indices>
  ASC_HOST_DEVICE constexpr int operator()(Indices... indices) const {
    ASC_STATIC_ASSERT(sizeof...(indices) == GetRank(),
                         "Index count mismatch");
    int index[GetNDims()] = {static_cast<int>(indices)...};
    return Fold(index);
  }

  /// @brief Fold multi-index to linear offset
  /// @param index Multi-dimensional index array
  /// @return Linear memory offset
  ASC_HOST_DEVICE constexpr int Fold(const int* index) const {
    int bounded[GetNDims()];
    for (int dim = 0; dim < GetRank(); ++dim) {
      bounded[dim] = ApplyPadding(index[dim], GetExtent(dim));
      if (bounded[dim] < 0) return -1;
    }
    return FoldImpl(std::make_index_sequence<GetRank()>{}, bounded);
  }

  /// @brief Fold in-bounds multi-index to linear offset without padding.
  /// @param index Multi-dimensional index array
  /// @return Linear memory offset
  ASC_HOST_DEVICE constexpr int FoldInBounds(const int* index) const {
#if !defined(__CUDA_ARCH__)
    for (int dim = 0; dim < GetRank(); ++dim) {
      ASC_ASSERT(index[dim] >= 0 && index[dim] < GetExtent(dim),
                    "Index out of bounds");
    }
#endif
    return FoldImpl(std::make_index_sequence<GetRank()>{}, index);
  }

  // ========== Offset → Multi Mapping (delegated to derived) ==========

  /// @brief Convert memory offset to multi-dimensional indices (Unfold offset
  /// to multi-dimensional indices)
  ///
  /// @param offset Memory offset in iteration order
  /// @param index Output array for multi-dimensional indices
  ASC_HOST_DEVICE constexpr void Unfold(int offset, int* index) const {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(offset >= 0 && offset <= GetSize(),
                  "Memory offset out of bounds");
#endif
    if (offset < GetSize()) {
      static_cast<const Derived*>(this)->UnfoldImpl(offset, index);
    }
  }

  // ========== Required Size (delegated to derived) ==========

  /// @brief Get required memory size for this layout
  /// @return Required memory size
  ASC_HOST_DEVICE constexpr int GetRequiredSize() const {
    return static_cast<const Derived*>(this)->GetRequiredSizeImpl();
  }

  // ========== Layout Properties (delegated to derived) ==========

  /// @brief Check if layout represents contiguous memory
  /// @return True if contiguous, false otherwise
  ASC_HOST_DEVICE constexpr bool IsContiguous() const {
    return static_cast<const Derived*>(this)->IsContiguousImpl();
  }

 protected:
  Shape shape_;              ///< Multi-dimensional shape
  int strides_[GetNDims()];  ///< Stride for each dimension

  // Allow derived classes to construct
  constexpr LayoutMapBase() = default;
  ASC_HOST_DEVICE constexpr explicit LayoutMapBase(const Shape& shape)
      : shape_(shape) {}

 private:
  // Helper: Compute offset for single index
  template <typename Idx>
  ASC_HOST_DEVICE constexpr int ComputeOffsetImpl(int dim, Idx idx) const {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(
        static_cast<int>(idx) >= 0 && static_cast<int>(idx) < GetExtent(dim),
        "Index out of bounds");
#endif
    return static_cast<int>(idx) * strides_[dim];
  }

  // Helper: Compute offset recursively for multiple indices
  template <typename First, typename... Rest>
  ASC_HOST_DEVICE constexpr int ComputeOffsetImpl(int dim, First first,
                                                     Rest... rest) const {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(static_cast<int>(first) >= 0 &&
                      static_cast<int>(first) < GetExtent(dim),
                  "Index out of bounds");
#endif
    return static_cast<int>(first) * strides_[dim] +
           ComputeOffsetImpl(dim + 1, rest...);
  }

  ASC_HOST_DEVICE static constexpr int PositiveMod(int value, int modulus) {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(modulus > 0, "Modulus must be positive");
#endif
    int result = value % modulus;
    if (result < 0) result += modulus;
    return result;
  }

  ASC_HOST_DEVICE static constexpr int ApplyPadding(int index, int extent) {
#if !defined(__CUDA_ARCH__)
    ASC_ASSERT(extent > 0, "Padding extent must be positive");
#endif
    if (index >= 0 && index < extent) return index;

    if constexpr (Padding == PaddingType::kNone) {
#if !defined(__CUDA_ARCH__)
      ASC_ASSERT(index >= 0 && index < extent, "Index out of bounds");
#endif
      return index;
    } else if constexpr (Padding == PaddingType::kEmpty) {
      return -1;
    } else if constexpr (Padding == PaddingType::kWrap) {
      return PositiveMod(index, extent);
    } else if constexpr (Padding == PaddingType::kEdge) {
      return index < 0 ? 0 : extent - 1;
    } else if constexpr (Padding == PaddingType::kReflect) {
      if (extent == 1) return 0;
      const int period = 2 * extent - 2;
      int wrapped = PositiveMod(index, period);
      if (wrapped >= extent) wrapped = period - wrapped;
      return wrapped;
    } else if constexpr (Padding == PaddingType::kSymmetric) {
      if (extent == 1) return 0;
      const int period = 2 * extent;
      int wrapped = PositiveMod(index, period);
      if (wrapped >= extent) wrapped = period - 1 - wrapped;
      return wrapped;
    } else {
#if !defined(__CUDA_ARCH__)
      ASC_ASSERT(false, "Unknown padding policy");
#endif
      return index;
    }
  }

  // Helper: Compute offset from array using fold expression
  template <std::size_t... Is>
  ASC_HOST_DEVICE constexpr int FoldImpl(std::index_sequence<Is...>,
                                            const int* indices) const {
    if constexpr (sizeof...(Is) == 0) {
      return 0;  // Rank-0 (scalar): always offset 0
    } else {
      return ((indices[Is] * strides_[Is]) + ...);
    }
  }
};

/// @brief Column-major (Fortran-style) layout policy
///
/// LayoutLeft stores multi-dimensional arrays in column-major order where the
/// first dimension varies fastest in memory.
///
/// @par Memory Layout:
/// For a 2x3 array: (0,0), (1,0), (0,1), (1,1), (0,2), (1,2)
///
/// @par Stride Pattern:
/// - stride[0] = 1
/// - stride[i] = stride[i-1] * extent[i-1]
struct LayoutLeft {
  template <typename Shape, PaddingType Padding = PaddingType::kNone>
  class Map : public LayoutMapBase<Shape, Padding, Map<Shape, Padding>> {
   private:
    using Base = LayoutMapBase<Shape, Padding, Map<Shape, Padding>>;
    friend Base;

   public:
    /// @brief Default constructor
    ASC_HOST_DEVICE constexpr Map() : Base() { ComputeStrides(); }

    /// @brief Construct from shape
    /// @param shape Multi-dimensional shape
    ASC_HOST_DEVICE constexpr explicit Map(const Shape& shape)
        : Base(shape) {
      ComputeStrides();
    }

    /// @brief Construct from shape and strides
    /// @param shape Multi-dimensional shape
    /// @param strides User-provided strides (ignored)
    ASC_HOST_DEVICE constexpr Map(const Shape& shape, const int* strides)
        : Base(shape) {
      ComputeStrides();
    }

   private:
    using Base::shape_;
    using Base::strides_;

    /// @brief Compute column-major strides
    ASC_HOST_DEVICE constexpr void ComputeStrides() {
      if constexpr (Base::GetRank() > 0) {
        strides_[0] = 1;
        for (int i = 1; i < Base::GetRank(); ++i) {
          strides_[i] = strides_[i - 1] * shape_.GetExtent(i - 1);
        }
      }
    }

    /// @brief Convert memory offset to multi-dimensional (column-major)
    ASC_HOST_DEVICE constexpr void UnfoldImpl(int offset,
                                                 int* multi_idx) const {
      int tmp = offset;
      for (int i = 0; i < Base::GetRank(); ++i) {
        multi_idx[i] = tmp % shape_.GetExtent(i);
        tmp /= shape_.GetExtent(i);
      }
    }

    /// @brief Get required memory size (contiguous)
    ASC_HOST_DEVICE constexpr int GetRequiredSizeImpl() const {
      return shape_.GetSize();
    }

    /// @brief Check if contiguous (always true for LayoutLeft)
    ASC_HOST_DEVICE constexpr bool IsContiguousImpl() const { return true; }
  };
};

/// @brief Row-major (C-style) layout policy
///
/// LayoutRight stores multi-dimensional arrays in row-major order where the
/// last dimension varies fastest in memory.
///
/// @par Memory Layout:
/// For a 2x3 array: (0,0), (0,1), (0,2), (1,0), (1,1), (1,2)
///
/// @par Stride Pattern:
/// - stride[rank-1] = 1
/// - stride[i] = stride[i+1] * extent[i+1]
struct LayoutRight {
  template <typename Shape, PaddingType Padding = PaddingType::kNone>
  class Map : public LayoutMapBase<Shape, Padding, Map<Shape, Padding>> {
   private:
    using Base = LayoutMapBase<Shape, Padding, Map<Shape, Padding>>;
    friend Base;

   public:
    /// @brief Default constructor
    ASC_HOST_DEVICE constexpr Map() : Base() { ComputeStrides(); }

    /// @brief Construct from shape
    /// @param shape Multi-dimensional shape
    ASC_HOST_DEVICE constexpr explicit Map(const Shape& shape)
        : Base(shape) {
      ComputeStrides();
    }

    /// @brief Construct from shape and strides
    /// @param shape Multi-dimensional shape
    /// @param strides User-provided strides (ignored)
    ASC_HOST_DEVICE constexpr Map(const Shape& shape, const int* strides)
        : Base(shape) {
      ComputeStrides();
    }

   private:
    using Base::shape_;
    using Base::strides_;

    /// @brief Compute row-major strides
    ASC_HOST_DEVICE constexpr void ComputeStrides() {
      if constexpr (Base::GetRank() > 0) {
        const int rank = Base::GetRank();
        strides_[rank - 1] = 1;
        for (int i = rank - 2; i >= 0; --i) {
          strides_[i] = strides_[i + 1] * shape_.GetExtent(i + 1);
        }
      }
    }

    /// @brief Convert memory offset to multi-dimensional (row-major)
    ASC_HOST_DEVICE constexpr void UnfoldImpl(int offset,
                                                 int* multi_idx) const {
      int tmp = offset;
      for (int i = Base::GetRank() - 1; i >= 0; --i) {
        multi_idx[i] = tmp % shape_.GetExtent(i);
        tmp /= shape_.GetExtent(i);
      }
    }

    ASC_HOST_DEVICE constexpr int GetRequiredSizeImpl() const {
      return shape_.GetSize();
    }

    ASC_HOST_DEVICE constexpr bool IsContiguousImpl() const { return true; }
  };
};

/// @brief Custom stride layout policy
///
/// LayoutStride allows arbitrary stride patterns for non-contiguous or
/// specially-ordered memory layouts.
///
/// @par Usage:
/// Requires explicit stride specification at construction.
struct LayoutStride {
  template <typename Shape, PaddingType Padding = PaddingType::kNone>
  class Map : public LayoutMapBase<Shape, Padding, Map<Shape, Padding>> {
   private:
    using Base = LayoutMapBase<Shape, Padding, Map<Shape, Padding>>;
    friend Base;

   public:
    /// @brief Default constructor
    ASC_HOST_DEVICE constexpr Map() : Base() {}

    /// @brief Construct from shape and user-provided strides
    /// @param shape Multi-dimensional shape
    /// @param strides User-provided strides
    ASC_HOST_DEVICE constexpr Map(const Shape& shape, const int* strides)
        : Base(shape) {
      for (int i = 0; i < Base::GetRank(); ++i) {
        this->strides_[i] = strides[i];
      }
    }

   private:
    using Base::shape_;
    using Base::strides_;

    /// @brief Convert memory offset to multi-dimensional (canonical
    /// column-major)
    ASC_HOST_DEVICE constexpr void UnfoldImpl(int offset, int* index) const {
      // Use canonical column-major iteration order
      int tmp = offset;
      for (int i = 0; i < Base::GetRank(); ++i) {
        index[i] = tmp % shape_.GetExtent(i);
        tmp /= shape_.GetExtent(i);
      }
    }

    /// @brief Get required memory size (non-contiguous)
    ASC_HOST_DEVICE constexpr int GetRequiredSizeImpl() const {
      int max_offset = 0;
      for (int i = 0; i < Base::GetRank(); ++i) {
        if (shape_.GetExtent(i) > 1) {
          max_offset += (shape_.GetExtent(i) - 1) *
                        (strides_[i] >= 0 ? strides_[i] : -strides_[i]);
        }
      }
      return max_offset + 1;
    }

    /// @brief Check if contiguous
    ASC_HOST_DEVICE constexpr bool IsContiguousImpl() const {
      // Check column-major contiguous
      {
        int expected_stride = 1;
        bool is_col_major = true;
        for (int i = 0; i < Base::GetRank(); ++i) {
          if (strides_[i] != expected_stride) {
            is_col_major = false;
            break;
          }
          expected_stride *= shape_.GetExtent(i);
        }
        if (is_col_major) return true;
      }

      // Check row-major contiguous
      {
        int expected_stride = 1;
        bool is_row_major = true;
        for (int i = Base::GetRank() - 1; i >= 0; --i) {
          if (strides_[i] != expected_stride) {
            is_row_major = false;
            break;
          }
          expected_stride *= shape_.GetExtent(i);
        }
        if (is_row_major) return true;
      }

      return false;
    }
  };
};

template <typename T>
struct IsLayout : std::false_type {};

template <>
struct IsLayout<LayoutLeft> : std::true_type {};

template <>
struct IsLayout<LayoutRight> : std::true_type {};

template <>
struct IsLayout<LayoutStride> : std::true_type {};

#ifdef ASC_USE_ROW_MAJOR
using DefaultLayout = LayoutRight;
#else
using DefaultLayout = LayoutLeft;
#endif

template <typename L>
concept NonStrideLayout =
    std::is_same_v<L, LayoutLeft> || std::is_same_v<L, LayoutRight>;

template <typename L>
concept StrideLayout = std::is_same_v<L, LayoutStride>;

template <typename L>
concept ColumnMajorLayout = std::is_same_v<L, LayoutLeft>;

template <typename L>
concept RowMajorLayout = std::is_same_v<L, LayoutRight>;

}  // namespace asc

#endif  // ASC_MLAYOUT_H_
