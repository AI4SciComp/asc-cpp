// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/carray.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_CARRAY_H_
#define ASC_CARRAY_H_

/// @file carray.h
/// @brief C-style multi-dimensional array utilities with arbitrary indexing
///
/// This module provides utilities for working with C-style nested pointers
/// and arrays, supporting:
/// - Multi-dimensional arrays with arbitrary index ranges (including negative
/// indices)
/// - Automatic memory management through New/Delete functions
/// - Type-safe pointer nesting through CPointer template
/// - Static multi-dimensional arrays through CArray template
/// - Pretty printing for arrays
///
/// @par Example - Dynamic 2D array with custom indices:
/// @code
/// double** A = nullptr;
/// asc::New<double>(A, -2, 2, 0, 4);  // Array from [-2,2] × [0,4]
/// A[-1][2] = 3.14;  // Negative indexing supported
/// asc::Print(std::cout, A, -2, 2, 0, 4);
/// asc::Delete<double>(A, -2, 2, 0, 4);
/// @endcode
///
/// @par Example - Static 2D array:
/// @code
/// asc::CArray<double, 3, 4> B;  // 3 × 4 array
/// B[1][2] = 2.71;
/// asc::Print(std::cout, B);
/// @endcode

#include <utility>
#include <string>
#include <tuple>
#include <type_traits>

#include "asc/core/error.h"
#include "asc/core/globals.h"

namespace asc {

template <typename T, int N>
struct CPointerTree;

template <typename T, int N = 1>
using CPointer = typename CPointerTree<T, N>::ElementType;

template <typename T, int... Extents>
struct CArrayTree;

template <typename T, int... Extents>
using CArray = typename CArrayTree<T, Extents...>::ContainerType;

/// @brief Nested C pointer type builder
///
/// CPointerTree recursively builds nested pointer types.
///
/// @tparam T Base element type
/// @tparam N Nesting depth (number of pointer levels)
template <typename T, int N>
struct CPointerTree {
  ASC_STATIC_ASSERT(N >= 0, "CPointerBase depth N must be >= 0.");
  static constexpr int kDepth = N;
  using ElementType = typename CPointerTree<T, kDepth - 1>::ElementType*;

  ASC_STATIC_ASSERT_LOWER_BOUND(kDepth, 1);
};

template <typename T>
struct CPointerTree<T, 1> {
  static constexpr int kDepth = 1;
  using ElementType = T*;
};

template <typename T>
struct CPointerTree<T, 0> {
  static constexpr int kDepth = 0;
  using ElementType = T;
};

/// @brief Nested C array type builder
///
/// CArrayTree recursively builds nested array types.
///
/// @tparam T Base element type
/// @tparam Extents Dimensions of the array (variadic)
template <typename T, int HeadExtent, int... TailExtents>
struct CArrayTree<T, HeadExtent, (TailExtents)...> {
  using ElementType = typename CArrayTree<T, (TailExtents)...>::ContainerType;
  using ContainerType = ElementType[HeadExtent];
};

template <typename T>
struct CArrayTree<T> {
  using ContainerType = T;
};

template <typename T, int Size>
struct CArrayTree<T, Size> {
  using ContainerType = T[Size];
};

namespace details {

template <typename T, int N = kDynamicExtent>
struct CVectorTraits {
  ASC_STATIC_ASSERT(N >= 0 || N == kDynamicExtent,
                       "N must be >= 0 or kDynamicExtent (-1).");
  using ContainerType =
      std::conditional_t<(N >= 0), CArray<T, 1, N>, CPointer<T, 1>>;
};

template <typename T, int M = kDynamicExtent, int N = kDynamicExtent>
struct CMatrixTraits {
  ASC_STATIC_ASSERT((M >= 0 || M == kDynamicExtent),
                       "Row extent must be >= 0 or kDynamicExtent (-1).");
  ASC_STATIC_ASSERT((N >= 0 || N == kDynamicExtent),
                       "Column extent must be >= 0 or kDynamicExtent (-1).");
  using ContainerType = std::conditional_t<(M >= 0 && N >= 0),
                                           CArray<T, 2, M, N>, CPointer<T, 2>>;
};

}  // namespace details

template <typename T, int N = kDynamicExtent>
using CVector = typename details::CVectorTraits<T, N>::ContainerType;

template <typename T, int M = kDynamicExtent, int N = kDynamicExtent>
using CMatrix = typename details::CMatrixTraits<T, M, N>::ContainerType;

/// @brief Implementation details for C array allocation and printing
///
/// This namespace contains helper structures for:
/// - Extracting extent pairs from parameter packs
/// - Managing dynamic allocation/deallocation of nested pointers
/// - Pretty printing multi-dimensional C arrays
namespace details {

// Helper to extract pairs of (low, high) extents from parameter pack
template <int Idx, typename... Args>
struct ExtentGetter;

template <int Idx, typename First, typename Second, typename... Rest>
struct ExtentGetter<Idx, First, Second, Rest...> {
  static std::pair<int, int> Get(First low, Second high, Rest... rest) {
    if constexpr (Idx == 0) {
      return std::make_pair(static_cast<int>(low), static_cast<int>(high));
    } else {
      return ExtentGetter<Idx - 1, Rest...>::Get(rest...);
    }
  }
};

template <typename T, int N>
struct CPointerManager {
  // Get the (low, high) extents for dimension N
  template <typename... Extents>
  static std::pair<int, int> GetLastExtent(Extents... extents) {
    constexpr int M = sizeof...(Extents) / 2;
    constexpr int Idx = M - N;
    return ExtentGetter<Idx, Extents...>::Get(extents...);
  }

  // Allocate with arbitrary index ranges: New(ptr, nl1, nh1, nl2, nh2, ...)
  template <typename... Extents>
  static void New(CPointer<T, N>& ptr, Extents... extents) {
    auto [nl, nh] = GetLastExtent(extents...);
    int size = nh - nl + 1;
    ASC_VERIFY(size >= 0, "Invalid CPointer extent range.");

    // Allocate array with offset for negative indexing
    CPointer<T, N - 1>* raw_ptr = new CPointer<T, N - 1>[size];
    ptr = raw_ptr - nl;  // Shift pointer to support arbitrary indices

    // Recursively allocate inner dimensions
    for (int i = nl; i <= nh; ++i) {
      CPointerManager<T, N - 1>::New(ptr[i], extents...);
    }
  }

  // Deallocate with arbitrary index ranges: Delete(ptr, nl1, nh1, nl2, nh2,
  // ...)
  template <typename... Extents>
  static void Delete(CPointer<T, N>& ptr, Extents... extents) {
    auto [nl, nh] = GetLastExtent(extents...);

    // Recursively deallocate inner dimensions
    for (int i = nl; i <= nh; ++i) {
      CPointerManager<T, N - 1>::Delete(ptr[i], extents...);
    }

    // Restore original pointer and delete
    CPointer<T, N - 1>* raw_ptr = ptr + nl;
    delete[] raw_ptr;
    ptr = nullptr;
  }

  // Print with arbitrary index ranges
  template <typename... Extents>
  static void Print(std::ostream& os, const CPointer<T, N>& ptr,
                    Extents... extents) {
    constexpr int M = sizeof...(Extents) / 2;
    auto [nl, nh] = GetLastExtent(extents...);

    os << '[';
    for (int i = nl; i <= nh; ++i) {
      CPointerManager<T, N - 1>::Print(os, ptr[i], extents...);
      if (i < nh) {
        os << ',' << std::string(N - 1, '\n') << std::string(M - N + 1, ' ');
      }
    }
    os << ']';
    if (M == N) os << '\n';
  }
};

template <typename T>
struct CPointerManager<T, 1> {
  // Get the (low, high) extents for the last dimension
  template <typename... Extents>
  static std::pair<int, int> GetLastExtent(Extents... extents) {
    constexpr int M = sizeof...(Extents) / 2;
    constexpr int Idx = M - 1;
    return ExtentGetter<Idx, Extents...>::Get(extents...);
  }

  // Allocate 1D array with arbitrary index range
  template <typename... Extents>
  static void New(CPointer<T, 1>& ptr, Extents... extents) {
    auto [nl, nh] = GetLastExtent(extents...);
    int size = nh - nl + 1;
    ASC_VERIFY(size >= 0, "Invalid CPointer extent range.");

    // Allocate array with offset for negative indexing
    T* raw_ptr = new T[size];
    ptr = raw_ptr - nl;  // Shift pointer to support arbitrary indices

    // Initialize elements
    for (int i = nl; i <= nh; ++i) {
      ptr[i] = T();
    }
  }

  // Deallocate 1D array
  template <typename... Extents>
  static void Delete(CPointer<T, 1>& ptr, Extents... extents) {
    auto [nl, nh] = GetLastExtent(extents...);

    // Restore original pointer and delete
    T* raw_ptr = ptr + nl;
    delete[] raw_ptr;
    ptr = nullptr;
  }

  // Print 1D array
  template <typename... Extents>
  static void Print(std::ostream& os, const CPointer<T, 1>& ptr,
                    Extents... extents) {
    constexpr int M = sizeof...(Extents) / 2;
    auto [nl, nh] = GetLastExtent(extents...);

    os << '[';
    for (int i = nl; i <= nh; ++i) {
      PPrint<T, false>(os, ptr[i]);
      if (i < nh) os << ", ";
    }
    os << ']';
    if (M == 1) os << '\n';
  }
};

template <typename T, int N, int... Extents>
struct CArrayManager {
  static constexpr int M = sizeof...(Extents);

  template <int HeadExtent, int... TailExtents>
  struct InnerManagerBase {
    using ElementType = CArrayManager<T, N, (TailExtents)...>;
  };
  using ManagerType = typename InnerManagerBase<Extents...>::ElementType;

  static void Print(std::ostream& os, const CArray<T, Extents...>& a) {
    int size =
        std::get<0>(std::forward_as_tuple(std::forward<int>(Extents)...));
    os << '[';
    for (int i = 0; i < size; ++i) {
      ManagerType::Print(os, a[i]);
      if (i < size - 1) {
        os << ',' << std::string((N == M) * (N - 2) + 1, '\n')
           << std::string(N - M + 1, ' ');
      }
    }
    os << ']';
    if (N == M) os << '\n';
  }
};

template <typename T, int N, int Size>
struct CArrayManager<T, N, Size> {
  static void Print(std::ostream& os, const CArray<T, Size>& a) {
    os << '[';
    for (int i = 0; i < Size; ++i) {
      PPrint(os, a[i]);
      if (i < Size - 1) os << ',' << ' ';
    }
    os << ']';
    if (N == 1) os << '\n';
  }
};

}  // namespace details

/// @brief Allocate multi-dimensional C array with arbitrary index ranges
///
/// Allocates a nested pointer structure with support for negative and custom
/// indices. Memory is allocated recursively for each dimension.
///
/// @tparam T Element type
/// @tparam Extents Variadic extents (low1, high1, low2, high2, ...)
/// @param ptr Reference to pointer to be allocated
/// @param extents Pairs of (low, high) bounds for each dimension
///
/// @par Example - 2D array:
/// @code
/// double** A = nullptr;
/// asc::New<double>(A, 0, 2, -1, 1);  // [0,2] × [-1,1] array
/// A[1][-1] = 3.14;  // Access with custom indices
/// @endcode
///
/// @par Example - 3D array with negative indices:
/// @code
/// int*** B = nullptr;
/// asc::New<int>(B, -2, 2, 0, 3, -1, 1);  // [-2,2] × [0,3] × [-1,1]
/// B[-1][2][0] = 42;
/// @endcode
///
/// @note Must be paired with Delete() using the same extents
/// @see Delete
template <typename T, typename... Extents>
void New(CPointer<T, sizeof...(Extents) / 2>& ptr, Extents... extents) {
  ASC_STATIC_ASSERT(sizeof...(Extents) % 2 == 0,
                       "New requires low/high extent pairs.");
  details::CPointerManager<T, sizeof...(Extents) / 2>::New(ptr, extents...);
}

/// @brief Deallocate multi-dimensional C array
///
/// Frees memory allocated by New(). Must use the same extent parameters
/// that were used during allocation.
///
/// @tparam T Element type
/// @tparam Extents Variadic extents (must match those used in New)
/// @param ptr Reference to pointer to be deallocated (set to nullptr after)
/// @param extents Pairs of (low, high) bounds (must match New call)
///
/// @par Example:
/// @code
/// double** A = nullptr;
/// asc::New<double>(A, 0, 2, -1, 1);
/// // ... use array ...
/// asc::Delete<double>(A, 0, 2, -1, 1);  // Same extents as New
/// @endcode
///
/// @note Extents must exactly match those used in New()
/// @see New
template <typename T, typename... Extents>
void Delete(CPointer<T, sizeof...(Extents) / 2>& ptr, Extents... extents) {
  ASC_STATIC_ASSERT(sizeof...(Extents) % 2 == 0,
                       "Delete requires low/high extent pairs.");
  details::CPointerManager<T, sizeof...(Extents) / 2>::Delete(ptr, extents...);
}

/// @brief Print multi-dimensional dynamic array
///
/// Pretty prints a dynamically allocated array to an output stream.
/// Supports arrays with arbitrary index ranges.
///
/// @tparam T Element type
/// @tparam Extents Variadic extents (must match allocation)
/// @param os Output stream
/// @param ptr Pointer to array to print
/// @param extents Pairs of (low, high) bounds (must match New call)
///
/// @par Example:
/// @code
/// double** A = nullptr;
/// asc::New<double>(A, 0, 1, 0, 2);
/// A[0][0] = 1.0; A[0][1] = 2.0; A[0][2] = 3.0;
/// A[1][0] = 4.0; A[1][1] = 5.0; A[1][2] = 6.0;
/// asc::Print(std::cout, A, 0, 1, 0, 2);
/// // Output: [[1, 2, 3],
/// //          [4, 5, 6]]
/// @endcode
template <typename T, typename... Extents>
void Print(std::ostream& os, const CPointer<T, sizeof...(Extents) / 2>& ptr,
           Extents... extents) {
  ASC_STATIC_ASSERT(sizeof...(Extents) % 2 == 0,
                       "Print requires low/high extent pairs.");
  details::CPointerManager<T, sizeof...(Extents) / 2>::Print(os, ptr,
                                                             extents...);
}

/// @brief Print multi-dimensional static array
///
/// Pretty prints a statically sized C array to an output stream.
///
/// @tparam T Element type
/// @tparam Extents Compile-time array dimensions
/// @param os Output stream
/// @param a Array to print
///
/// @par Example:
/// @code
/// asc::CArray<double, 2, 3> B = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
/// asc::Print(std::cout, B);
/// // Output: [[1, 2, 3],
/// //          [4, 5, 6]]
/// @endcode
template <typename T, int... Extents>
void Print(std::ostream& os, const CArray<T, Extents...>& a) {
  constexpr int N = sizeof...(Extents);
  details::CArrayManager<T, N, Extents...>::Print(os, a);
}

}  // namespace asc

#endif  // ASC_CARRAY_H_
