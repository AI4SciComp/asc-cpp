// ============================================================================
// Copyright (C) 2025, Xiamen University, School of Mathematical Sciences.
// All rights reserved. See files LICENSE for details.
//
// File: asc/dsmarray.h
// Author: Yi Cai
// ============================================================================

#ifndef ASC_DSMARRAY_H_
#define ASC_DSMARRAY_H_

/// @file dsmarray.h
/// @brief Multi-dimensional array with heterogeneous memory support
///
/// This module provides the DenseMArray<T, Shape, Layout> class template, an
/// owning container for multi-dimensional data with compile-time or runtime
/// shape. It integrates with ASC's heterogeneous memory system for automatic
/// host/device synchronization.
///
/// @par Key Features:
/// - Compile-time or runtime dimensionality via Shape parameter
/// - Support for both static and dynamic extents (e.g., MShape<3> vs
/// MShape<kDynamicExtent>)
/// - Heterogeneous computing: CPU/GPU memory with automatic synchronization
/// - Flexible memory layouts: column-major (LayoutLeft) or row-major
/// (LayoutRight)
/// - Element-wise arithmetic operations (+, -, *, / with arrays and scalars)
/// - Multi-dimensional indexing: operator()(i, j, k) or linear indexing:
/// operator[](idx)
/// - Bounds checking in debug builds (ASC_ASSERT)
/// - Integration with Memory<T> for advanced memory management
/// - Serialization support (Save/Load)
///
/// @par Memory Management:
/// DenseMArray uses Memory<T> internally for allocation and host/device sync:
/// - Automatic synchronization between host and device via Read/Write/ReadWrite
/// - Multiple memory types (kHost, kDevice, kManaged, kHost32, kHost64)
/// - Ownership tracking (owns data vs. wraps external pointers via MakeRef)
/// - Capacity management (Reserve for pre-allocation, avoids reallocation)
///
/// @par Shape and Layout:
/// - Shape: MShape<N1, N2, ...> where Ni is extent (kDynamicExtent for runtime)
/// - Layout: LayoutLeft (column-major, Fortran order) or LayoutRight
/// (row-major, C order)
/// - Example: DenseMArray<double, MShape<3, 4>, LayoutLeft> is a 3x4
/// column-major matrix
///
/// @par Type Requirements:
/// Element type T must be trivial (checked at compile-time via static_assert).
/// This ensures safe copying between host and device memory.
///
/// @par Common Type Aliases:
/// - VectorXr, MatrixXr: Dynamic-size double vectors/matrices
/// - Vector3r, Matrix3r: Static-size 3D double vectors/3x3 matrices
/// - VectorXi, MatrixXi: Dynamic-size int vectors/matrices
///
/// @par Usage Examples:
/// @code
/// // Dynamic-size 1D array (vector)
/// asc::VectorXr vec(100);
/// vec = 0.0;                          // Set all elements to 0
/// vec[10] = 3.14;                     // Linear indexing
///
/// // Dynamic-size 2D array (matrix)
/// asc::MatrixXr mat(3, 4);         // 3 rows, 4 columns
/// mat(1, 2) = 2.71;                   // Multi-dimensional indexing
///
/// // Static-size 3D vector
/// asc::Vector3r v;
/// v = {1.0, 2.0, 3.0};
///
/// // Static-size 3x3 matrix
/// asc::Matrix3r m;
/// m(0, 0) = 1.0;
///
/// // Arithmetic operations
/// asc::VectorXr a(10), b(10);
/// a = 1.0;
/// b = 2.0;
/// asc::VectorXr c = a + b;         // Element-wise addition
/// a *= 3.0;                           // Scalar multiplication
///
/// // GPU memory (CUDA)
/// asc::VectorXr gpu_vec(asc::MemoryType::kDevice, 1000);
/// gpu_vec.UseDevice(true);
/// const double* dev_ptr = gpu_vec.Read(true);  // Device pointer
///
/// // Non-owning reference
/// double data[10];
/// asc::VectorXr ref;
/// ref.MakeRef(data, asc::MShape<10>(), false);
///
/// // Serialization
/// mat.Save(std::cout);                // Save to stream
/// mat.Load(std::cin);                 // Load from stream
/// @endcode
///
/// @see Layout::Map for shape and stride management
/// @see Memory for underlying memory management
/// @see MShape for shape specification

#include <cmath>
#include <iostream>
#include <type_traits>
#include <utility>
#include <algorithm>

#include "asc/core/error.h"
#include "asc/core/globals.h"
#include "asc/core/operators.h"
#include "asc/core/forall.h"
#include "asc/core/memory.h"
#include "asc/array/mshape.h"
#include "asc/array/mlayout.h"
#include "asc/array/mindex.h"
#include "asc/array/miterator.h"
#include "asc/array/mobject.h"
#include "asc/array/forwards.h"

namespace asc {

template <typename T, typename Shape>
using MArrayView = DenseMArray<T, Shape, LayoutStride>;

/// @brief Swap the contents of two DenseMArray instances
/// @tparam T Element type
/// @tparam Shape Shape type
/// @tparam Layout Layout type
/// @param a First array
/// @param b Second array
template <typename T, typename Shape, typename Layout>
void Swap(DenseMArray<T, Shape, Layout>&, DenseMArray<T, Shape, Layout>&);

/// @brief Equality comparison for DenseMArray
/// @tparam Tensor Tensor type (must be DenseMArray)
/// @tparam T Element type
/// @param lhs Left-hand side array
/// @param rhs Right-hand side array
/// @return True if arrays have same shape and all elements are equal
template <typename T, typename Shape, typename Layout>
inline bool operator==(const DenseMArray<T, Shape, Layout>& lhs,
                       const DenseMArray<T, Shape, Layout>& rhs);

/// @brief Inequality comparison for DenseMArray
/// @tparam Tensor Tensor type (must be DenseMArray)
/// @tparam T Element type
/// @param lhs Left-hand side array
/// @param rhs Right-hand side array
/// @return True if arrays differ in shape or any element
template <typename T, typename Shape, typename Layout>
inline bool operator!=(const DenseMArray<T, Shape, Layout>& lhs,
                       const DenseMArray<T, Shape, Layout>& rhs) {
  return !(lhs == rhs);
}

/// @brief Multi-dimensional array with heterogeneous memory support
///
/// DenseMArray<T, Shape, Layout> is an owning container for multi-dimensional
/// data that integrates with ASC's Memory system, providing automatic
/// host/device synchronization. It supports both compile-time and runtime shape
/// specification.
///
/// @tparam T Element type (must be trivial/POD type)
/// @tparam Shape Shape type (e.g., MShape<3, 4> or MShape<kDynamicExtent>)
/// @tparam Layout Memory layout (LayoutLeft for column-major, LayoutRight for
/// row-major)
///
/// @par Growth Strategy:
/// When capacity is exceeded during SetShape(), the array grows to
/// max(requested_size, 2*capacity). Use Reserve() to preallocate and avoid
/// repeated reallocations.
///
/// @par Ownership:
/// - Owning: DenseMArray manages memory lifetime (default constructors)
/// - Non-owning: Wraps external memory via MakeRef()
///
/// @par Thread Safety:
/// DenseMArray is NOT thread-safe. Use separate instances per thread.
///
/// @see Layout::Map for shape and stride management
/// @see Memory for underlying memory management
template <typename T, typename Shape, typename Layout = DefaultLayout>
class DenseMArray : public MObject<DenseMArray<T, Shape, Layout>> {
 public:
  /// Type aliases
  using MapType = typename Layout::template Map<Shape>;
  using ShapeType = Shape;
  using LayoutType = Layout;
  using ElementType = T;
  using ValueType = std::remove_cv_t<T>;

  static constexpr bool SupportsDeviceExpression =
      std::is_same_v<Layout, LayoutLeft>;

  /// Get the rank (number of dimensions) of the array
  static constexpr int GetRank() { return Shape::GetRank(); }

  /// Friend declaration for MObject to access private members
  friend class MObject<DenseMArray<T, Shape, Layout>>;

  /// Swap the contents of two DenseMArray instances
  friend void Swap<T, Shape, Layout>(DenseMArray& a, DenseMArray& b);

  /// Friend declaration for View to access private members across template
  /// instantiations
  template <typename U, typename S, typename L>
  friend class DenseMArray;

  // ========== Constructors and Destructor ==========

  /// @brief Default constructor - creates array based on shape type
  /// @note For static shapes, allocates memory; for dynamic shapes, creates
  /// empty array
  DenseMArray();

  /// @brief Construct empty array with specified memory type
  /// @param mt Memory type for allocation (kHost, kDevice, kManaged, etc.)
  explicit inline DenseMArray(MemoryType mt);

  /// @brief Construct array with specified shape
  /// @param shape Array shape
  explicit inline DenseMArray(const Shape& shape);

  /// @brief Construct array with specified shape and memory type
  /// @param shape Array shape
  /// @param mt Memory type for allocation
  inline DenseMArray(const Shape& shape, MemoryType mt);

  /// @brief Construct array with variadic extents
  /// @param extents Extent for each dimension
  template <Integral... Extents>
  explicit inline DenseMArray(Extents... extents);

  /// @brief Construct array with memory type and variadic extents
  /// @param mt Memory type for allocation
  /// @param extents Extent for each dimension
  template <Integral... Extents>
  inline DenseMArray(MemoryType mt, Extents... extents);

  /// @brief Copy constructor - deep copy
  /// @param other Source array to copy from
  inline DenseMArray(const DenseMArray& other);

  /// @brief Move constructor
  /// @param other Source array (will be reset after move)
  inline DenseMArray(DenseMArray&& other) noexcept : DenseMArray() {
    Swap(*this, other);
  }

  /// @brief Destructor - deletes owned memory
  ~DenseMArray() { data_.Delete(); }

  // ========== Assignment Operators ==========

  /// @brief Copy assignment operator
  /// @param other Source array to copy from
  /// @return Reference to this array
  inline DenseMArray& operator=(const DenseMArray& other) {
    if (this != &other) {
      other.CopyTo(*this);
    }
    return *this;
  }

  /// @brief Move assignment operator
  /// @param other Source array to move from
  /// @return Reference to this array
  inline DenseMArray& operator=(DenseMArray&& other) noexcept {
    if (this != &other) {
      Swap(*this, other);
    }
    return *this;
  }

  /// @brief Assign same scalar value to all elements
  /// @param value Value to assign
  /// @return Reference to this array
  template <Arithmetic U>
    requires std::convertible_to<U, T>
  inline DenseMArray& operator=(const U& value) {
    SetConstants(static_cast<T>(value));
    return *this;
  }

  /// @brief Assignment from another array with broadcasting support
  /// @tparam A Array type (can be any DenseMArray instantiation)
  /// @param other Source array to assign from
  /// @return Reference to this array
  ///
  /// Supports broadcasting when shapes are compatible:
  /// - Same size: element-wise copy
  /// - Vector to matrix: broadcasts vector along first dimension
  /// - General broadcasting: dimensions must be equal or one is 1
  template <typename Other>
    requires(!Arithmetic<std::decay_t<Other>>)
  inline DenseMArray& operator=(const Other& other);

  // ========== Compound Arithmetic Operators ==========

  /// Element-wise addition with another array (supports broadcasting)
  template <typename Other>
    requires(!Arithmetic<std::decay_t<Other>>)
  inline DenseMArray& operator+=(const Other& other);
  /// Element-wise subtraction with another array (supports broadcasting)
  template <typename Other>
    requires(!Arithmetic<std::decay_t<Other>>)
  inline DenseMArray& operator-=(const Other& other);
  /// Element-wise multiplication with another array (supports broadcasting)
  template <typename Other>
    requires(!Arithmetic<std::decay_t<Other>>)
  inline DenseMArray& operator*=(const Other& other);
  /// Element-wise division with another array (supports broadcasting)
  template <typename Other>
    requires(!Arithmetic<std::decay_t<Other>>)
  inline DenseMArray& operator/=(const Other& other);

  /// Add scalar to all elements
  template <Arithmetic U>
  inline DenseMArray& operator+=(const U& value);
  /// Subtract scalar from all elements
  template <Arithmetic U>
  inline DenseMArray& operator-=(const U& value);
  /// Multiply all elements by scalar
  template <Arithmetic U>
  inline DenseMArray& operator*=(const U& value);
  /// Divide all elements by scalar
  template <Arithmetic U>
  inline DenseMArray& operator/=(const U& value);

  // ========== Unary Operators ==========

  /// @brief Unary negation operator
  /// @return New array with negated elements
  inline DenseMArray operator-() const;

  // ========== Shape and Size Accessors ==========

  /// @brief Get the shape object
  /// @return Reference to the shape
  inline const Shape& GetShape() const { return map_.GetShape(); }

  /// @brief Change the shape, reallocating if necessary
  /// @param shape New shape
  inline void SetShape(const Shape& shape) {
    if (shape.GetSize() > data_.GetCapacity()) {
      GrowMemory(shape.GetSize());
    }
    map_ = MapType(shape);
  }

  /// @brief Change the shape and memory type, reallocating if necessary
  /// @param shape New shape
  /// @param mt New memory type (may trigger reallocation)
  inline void SetShape(const Shape& shape, MemoryType mt);

  /// @brief Resize and initialize new elements with a value
  /// @param shape New shape
  /// @param val Value to assign to new elements (if growing)
  inline void SetShape(const Shape& shape, const T& val);

  /// @brief Reserve space for at least the specified capacity
  /// @param capacity Minimum capacity to reserve
  /// @note Does not change GetSize(), only ensures capacity
  inline void Reserve(const Shape& capacity) {
    if (capacity.GetSize() > data_.GetCapacity()) {
      GrowMemory(capacity.GetSize());
    }
  }

  /// @brief Get the layout map
  /// @return Reference to the layout map
  inline const MapType& GetMap() const { return map_; }

  /// @brief Check if array uses LayoutLeft (column-major)
  /// @return True if layout is LayoutLeft
  inline bool IsLayoutLeft() const {
    return std::is_same<Layout, LayoutLeft>::value;
  }

  /// @brief Check if array uses LayoutRight (row-major)
  /// @return True if layout is LayoutRight
  inline bool IsLayoutRight() const {
    return std::is_same<Layout, LayoutRight>::value;
  }

  /// @brief Check if array uses LayoutStride (custom strides)
  /// @return True if layout is LayoutStride
  inline bool IsLayoutStride() const {
    return std::is_same<Layout, LayoutStride>::value;
  }

  /// @brief Get the current number of elements
  /// @return Number of elements in the array
  inline int GetSize() const { return map_.GetSize(); }

  /// @brief Get extent (size) along a specific dimension
  /// @param dim Dimension index
  /// @return Extent along dimension i
  inline int GetExtent(int dim) const { return map_.GetExtent(dim); }

  /// @brief Check if array is empty
  /// @return True if GetSize() == 0
  inline bool IsEmpty() const { return GetSize() == 0; }

  /// @brief Create an MIterator instance for this array
  /// @return MIterator for traversing the array
  inline MIterator<Shape, Layout> GetIterator() const {
    return MIterator<Shape, Layout>(map_);
  }

  /// @brief Delete all elements and free memory
  inline void Delete() {
    const bool use_dev = data_.UseDevice();
    data_.Delete();
    data_.Reset();
    map_ = MapType();
    data_.UseDevice(use_dev);
  }

  // ========== Element Access ==========

  /// @brief Access element by multi-dimensional indices (non-const)
  /// @param indices Index for each dimension
  /// @return Reference to element
  template <typename... Indices>
  inline T& operator()(Indices... indices) {
    ASC_ASSERT(sizeof...(indices) == GetRank(),
                  "incorrect number of indices");
    CheckBounds(indices...);
    T* ptr = HostReadWrite();
    return ptr[map_(indices...)];
  }

  /// @brief Access element by multi-dimensional indices (const)
  /// @param indices Index for each dimension
  /// @return Const reference to element
  template <typename... Indices>
  inline const T& operator()(Indices... indices) const {
    ASC_ASSERT(sizeof...(indices) == GetRank(),
                  "incorrect number of indices");
    CheckBounds(indices...);
    const T* ptr = HostRead();
    return ptr[map_(indices...)];
  }

  /// @brief Element access with bounds checking (debug builds)
  /// @param i Linear index (must be in [0, GetSize()))
  /// @return Reference to element at index
  inline T& operator[](int i) {
    CheckBounds(i);
    T* ptr = HostReadWrite();
    return ptr[i];
  }

  /// @brief Element access with bounds checking (const, debug builds)
  /// @param i Linear index (must be in [0, GetSize()))
  /// @return Const reference to element at index
  inline const T& operator[](int i) const {
    CheckBounds(i);
    const T* ptr = HostRead();
    return ptr[i];
  }

  /// @brief Element access using MIndex with function-call syntax (non-const)
  /// @param idx Multi-dimensional index object
  /// @return Reference to element at the specified index
  inline T& operator()(const MIndex<Shape, Layout>& idx) {
    T* ptr = HostReadWrite();
    return ptr[idx.GetOffset()];
  }

  /// @brief Element access using MIndex with function-call syntax (const)
  /// @param idx Multi-dimensional index object
  /// @return Const reference to element at the specified index
  inline const T& operator()(const MIndex<Shape, Layout>& idx) const {
    const T* ptr = HostRead();
    return ptr[idx.GetOffset()];
  }

  // ========== Data Pointers and Memory Management ==========

  /// @brief Get pointer to underlying data
  /// @return Pointer to the data array
  inline T* GetData() { return HostReadWrite(); }

  /// @brief Get const pointer to underlying data
  /// @return Const pointer to the data array
  inline const T* GetData() const { return HostRead(); }

  /// @brief Access the underlying Memory object
  /// @return Reference to the Memory<T> object
  inline Memory<T>& GetMemory() { return data_; }

  /// @brief Access the underlying Memory object (const)
  /// @return Const reference to the Memory<T> object
  inline const Memory<T>& GetMemory() const { return data_; }

  /// @brief Check if this array owns its data
  /// @return True if the array owns and will delete its data
  inline bool OwnsData() const { return data_.OwnsHostPtr(); }

  /// @brief Check if device memory is being used
  /// @return True if device operations are enabled
  inline bool UseDevice() const { return data_.UseDevice(); }

  /// @brief Set whether to use device memory
  /// @param use_dev True to enable device memory operations, false otherwise
  inline void UseDevice(bool use_dev) { return data_.UseDevice(use_dev); }

  /// @brief Get read-only pointer (synchronizes data if needed)
  /// @param on_dev If true, get device pointer; else get host pointer
  /// @return Const pointer to data
  inline const T* Read(bool on_dev = true) const {
    return asc::Read(data_, map_.GetSize(), on_dev);
  }

  /// @brief Get read-only host pointer
  /// @return Const pointer to host data
  inline const T* HostRead() const {
    return asc::Read(data_, map_.GetSize(), false);
  }

  /// @brief Get write-only pointer (invalidates other copy)
  /// @param on_dev If true, get device pointer; else get host pointer
  /// @return Pointer to data
  inline T* Write(bool on_dev = true) {
    return asc::Write(data_, map_.GetSize(), on_dev);
  }

  /// @brief Get write-only host pointer
  /// @return Pointer to host data
  inline T* HostWrite() { return asc::Write(data_, map_.GetSize(), false); }

  /// @brief Get read-write pointer (synchronizes data if needed)
  /// @param on_dev If true, get device pointer; else get host pointer
  /// @return Pointer to data
  inline T* ReadWrite(bool on_dev = true) {
    return asc::ReadWrite(data_, map_.GetSize(), on_dev);
  }

  /// @brief Get read-write host pointer
  /// @return Pointer to host data
  inline T* HostReadWrite() {
    return asc::ReadWrite(data_, map_.GetSize(), false);
  }

  // ========== Copy Operations ==========

  /// @brief Copy data from another DenseMArray
  /// @param src Source array (must have compatible size)
  inline void CopyFrom(const DenseMArray& src);

  /// @brief Copy data from host pointer
  /// @param src Source host pointer (must have at least GetSize() elements)
  inline void CopyFromHost(const T* src) {
    if (map_.GetSize() == 0) return;
    data_.CopyFromHost(src, map_.GetSize());
  }

  /// @brief Copy data to another DenseMArray (resizes destination if needed)
  /// @param dest Destination array
  inline void CopyTo(DenseMArray& dest) const;

  /// @brief Copy data to host pointer
  /// @param dest Destination host pointer (must have at least GetSize()
  /// elements)
  inline void CopyToHost(T* dest) const {
    if (map_.GetSize() == 0) return;
    data_.CopyToHost(dest, map_.GetSize());
  }

  /// @brief Copy data to external pointer with type conversion
  /// @tparam U Destination element type
  /// @param dest Destination pointer
  template <typename U>
  inline void CopyTo(U* dest) const;

  /// @brief Copy data from external pointer with type conversion
  /// @tparam U Source element type
  /// @param src Source pointer
  template <typename U>
  inline void CopyFrom(const U* src);

  // ========== Reference Management ==========

  /// @brief Make this a non-owning reference to external data
  /// @param data Pointer to external data
  /// @param shape Shape of the data
  /// @param own_data If true, take ownership of the data
  inline void MakeRef(T* data, const Shape& shape, bool own_data = false) {
    data_.Delete();
    map_ = MapType(shape);
    data_.Wrap(data, map_.GetSize(), own_data);
  }

  /// @brief Make this a non-owning reference with specific memory type
  /// @param data Pointer to external data
  /// @param shape Shape of the data
  /// @param mt Memory type of the data
  /// @param own_data If true, take ownership of the data
  inline void MakeRef(T* data, const Shape& shape, MemoryType mt,
                      bool own_data) {
    data_.Delete();
    map_ = MapType(shape);
    data_.Wrap(data, map_.GetSize(), mt, own_data);
  }

  /// @brief Make this a reference (alias) to another DenseMArray
  /// @param master Source array to reference
  inline void MakeRef(const DenseMArray& master) {
    data_.Delete();
    map_ = master.map_;
    data_.MakeAlias(master.GetMemory(), 0, map_.GetSize());
  }

  // ========== Cross-Rank View Methods ==========

  /// @brief Create a different-rank view of this array's data (zero-copy)
  /// @tparam ResultShape Shape type for the target array
  /// @param shape Target shape object
  /// @return New DenseMArray with different rank sharing the same underlying
  /// data
  ///
  /// This creates a zero-copy view of the current array's data with a
  /// different rank. The total size must match. The returned array shares
  /// the same memory, so modifications to either array affect both.
  ///
  /// Example:
  /// @code
  /// DVector<int> vec(12);
  /// auto mat = vec.View<DShape<2>>(DShape<2>(3, 4));  // 1D -> 2D view
  /// mat(1, 2) = 42;  // Modifies vec as well
  /// @endcode
  template <typename ResultShape>
  inline DenseMArray<T, ResultShape, Layout> View(
      const ResultShape& shape) const {
    ASC_VERIFY(
        shape.GetSize() == GetSize(),
        "View: total size must match (current=" + std::to_string(GetSize()) +
            ", target=" + std::to_string(shape.GetSize()) + ")");

    DenseMArray<T, ResultShape, Layout> result;
    // A default-constructed static-shape array owns storage. Release that
    // storage before rebinding the result as a non-owning view.
    result.data_.Delete();
    result.data_.MakeAlias(data_, 0, GetSize());
    result.map_ = typename Layout::template Map<ResultShape>(shape);
    return result;
  }

  /// @brief Create a different-rank view with variadic extents (zero-copy)
  /// @tparam ResultShape Shape type for the target array
  /// @param extents Variadic extent values for the target shape
  /// @return New DenseMArray with different rank sharing the same underlying
  /// data
  ///
  /// Convenience overload that constructs the target shape from extents.
  /// For static shapes, pass dummy values (they are ignored).
  ///
  /// Example:
  /// @code
  /// DVector<int> vec(12);
  /// auto mat = vec.View<DShape<2>>(3, 4);  // 1D -> 2D view (3x4)
  /// // Static shape example:
  /// SVector<int, 12> svec;
  /// auto smat = svec.View<MShape<3, 4>>(0, 0);  // Pass dummy values
  /// @endcode
  template <typename ResultShape, Integral... Extents>
  inline DenseMArray<T, ResultShape, Layout> View(Extents... extents) const {
    ResultShape shape;
    if constexpr (ResultShape::GetDynamicRank() > 0) {
      // Dynamic shape: construct with extents
      shape = ResultShape(extents...);
    }
    // Static shape: use default constructor (extents are compile-time)
    return View(shape);
  }

  // ========== Transpose and Permute Methods ==========
 public:
  /// @brief Transpose array with reversed dimensions
  /// @return Transposed view with reversed shape
  inline MArrayView<T, TransposedShape<Shape>> Transpose() const;

  /// @brief Permute array dimensions (static shapes, compile-time permutation)
  /// @tparam Perm Permutation indices as template parameters
  /// @return Permuted view with permuted compile-time shape
  ///
  /// For static shapes, specify permutation at compile time for maximum
  /// efficiency. For runtime permutation on any shape type. Always returns
  /// dynamic shape.
  ///
  /// Example:
  /// @code
  /// STensor<int, 2, 3, 4> tensor;  // Shape: (2, 3, 4)
  /// auto permuted = tensor.Permute<2, 0, 1>();  // Shape: (4, 2, 3)
  /// @endcode
  /// @code
  /// DTensor<int, 3> tensor(2, 3, 4);  // Shape: (2, 3, 4)
  /// auto permuted = tensor.Permute<2, 0, 1>();  // Shape: (4, 2, 3)
  /// @endcode
  template <size_t... Is>
  inline MArrayView<T, PermutedShape<Shape, Is...>> Permute() const;

  // ========== Slicing and Subarray Views ==========

  /// @brief Extract a slice along a specific dimension (zero-copy)
  /// @param dim Dimension to slice along
  /// @param index Index along that dimension
  /// @return View with rank reduced by 1
  ///
  /// Creates a zero-copy view by fixing one dimension at a specific index,
  /// reducing the rank by 1. The resulting view shares memory with the
  /// original array.
  ///
  /// @par Example:
  /// @code
  /// DTensor<double, 3> tensor(2, 3, 4);  // 2x3x4 tensor
  /// auto slice = tensor.Slice(1, 2);     // Fix dimension 1 at index 2
  /// // Result is 2x4 matrix (dimensions 0 and 2)
  /// slice(0, 3) = 42.0;  // Modifies tensor(0, 2, 3)
  ///
  /// DMatrix<double> mat(3, 4);  // 3x4 matrix
  /// auto col = mat.Slice(1, 2); // Extract column 2 (3rd column)
  /// // Result is a 3-element vector
  /// @endcode
  template <HighRankShape S = Shape>
  inline MArrayView<T, DShape<S::GetRank() - 1>> Slice(int dim,
                                                       int index) const;

  // ========== Iterators and Utility ==========

  /// @brief Get total memory usage in bytes
  /// @return Capacity() * sizeof(T)
  inline std::size_t MemoryUsage() const {
    return data_.GetCapacity() * sizeof(T);
  }

  /// @brief Get iterator to beginning
  /// @return Pointer to first element
  inline T* begin() { return HostReadWrite(); }

  /// @brief Get iterator to end
  /// @return Pointer to one past last element
  inline T* end() { return HostReadWrite() + GetSize(); }

  /// @brief Get const iterator to beginning
  /// @return Const pointer to first element
  inline const T* begin() const { return HostRead(); }

  /// @brief Get const iterator to end
  /// @return Const pointer to one past last element
  inline const T* end() const { return HostRead() + GetSize(); }

  // ========== I/O Operations ==========

  /// @brief Print array to output stream
  /// @param os Output stream (default: asc::mout)
  void Print(std::ostream& os = asc::mout) const { PrintImpl(os, 0, 0, 0); }

  /// @brief Save array to output stream
  /// @param os Output stream
  /// @param fmt Format flag (0: include shape, 1: data only)
  void Save(std::ostream& os, int fmt = 0) const;

  /// @brief Load array from input stream
  /// @param in Input stream
  /// @param fmt Format flag (0: read shape first, 1: data only)
  void Load(std::istream& in, int fmt = 0);

  // ========== Initialization Methods ==========

  /// @brief Set all elements to zero
  inline void SetZeros();

  /// @brief Set all elements to one
  inline void SetOnes();

  /// @brief Set all elements to a constant value
  /// @param value Constant value to set
  inline void SetConstants(const T& value);

  /// @brief Set tensor to identity (all diagonal elements to 1, others to 0)
  /// @note Sets element to 1 if all indices are equal, 0 otherwise
  /// @note For 2D: standard identity matrix; for higher dims: generalized
  /// identity tensor
  inline void SetIdentity();

  // ==========================================================================
  // Reduction Operations (DenseMArray-specific, uses MIterator for
  // layout-awareness)
  // ==========================================================================

  /// @brief Compute p-norm of the array
  /// @param p Norm type (0: element count, 1: L1, 2: L2, -1: L-infinity)
  /// @return Norm value
  inline T Norm(int p = 2) const;

  /// @brief Compute sum of all elements
  /// @return Sum of all elements
  inline T Sum() const;

  /// @brief Compute product of all elements
  /// @return Product of all elements
  inline T Product() const;

  /// @brief Compute mean (average) of all elements
  /// @return Mean value
  inline T Mean() const;

  /// @brief Find maximum element value
  /// @return Maximum value in the array
  inline T Max() const { return Maximum(); }

  /// @brief Find minimum element value
  /// @return Minimum value in the array
  inline T Min() const { return Minimum(); }

  /// @brief Find maximum element value (implementation)
  /// @return Maximum value in the array
  inline T Maximum() const;

  /// @brief Find minimum element value (implementation)
  /// @return Minimum value in the array
  inline T Minimum() const;

  /// @brief Compute dot product (inner product) with another array
  /// @tparam Other Other array type (must have same ValueType)
  /// @param other The other array to compute dot product with
  /// @return Scalar result of the dot product
  ///
  /// @note Both arrays must have the same size (flattened for computation)
  /// @note For 1D arrays: standard dot product
  /// @note For higher-D arrays: flattens both and computes dot product
  template <typename Other>
  inline T Dot(const Other& other) const;

  // ========== Comma Initialization ==========

  /// @brief Helper class for comma-separated initialization
  ///
  /// Enables Eigen-style comma initialization:
  /// @code
  /// DenseMArray<double, MShape<2, 3>> mat;
  /// mat << 1, 2, 3,
  ///        4, 5, 6;
  /// @endcode
  class CommaInitializer {
   public:
    /// @brief Constructor - starts initialization with first value
    /// @param array Reference to the array being initialized
    /// @param first_value First value to initialize
    inline CommaInitializer(DenseMArray& array, const T& first_value)
        : array_(array), idx_(1) {
      data_ = array_.HostWrite();
      data_[0] = first_value;
    }

    /// @brief Destructor - verify all elements were initialized
    inline ~CommaInitializer() {
      ASC_ASSERT(idx_ == array_.GetSize(),
                    "Comma initialization incomplete: provided " +
                        std::to_string(idx_) + " values, expected " +
                        std::to_string(array_.GetSize()));
    }

    /// @brief Add next value via comma operator
    /// @param value Value to add
    /// @return Reference to this initializer for chaining
    inline CommaInitializer& operator,(const T& value) {
      ASC_ASSERT(idx_ < array_.GetSize(),
                    "Too many values in comma initialization");
      data_[idx_++] = value;
      return *this;
    }

   private:
    DenseMArray& array_;  ///< Reference to array being initialized
    T* data_;             ///< Pointer to array data
    int idx_;             ///< Current initialization index
  };

  /// @brief Start comma-separated initialization
  /// @param value First value
  /// @return CommaInitializer for remaining values
  ///
  /// @par Example:
  /// @code
  /// asc::MatrixNr mat(2, 3);
  /// mat << 1, 2, 3,
  ///        4, 5, 6;
  /// @endcode
  inline CommaInitializer operator<<(const T& value) {
    return CommaInitializer(*this, value);
  }

  /// @brief Apply binary operation with broadcasting support
  /// @tparam A Array type (can be any DenseMArray instantiation)
  /// @tparam BinaryOp Binary operation type (e.g., PlusOp, MinusOp)
  /// @param other Other array to operate with
  /// @param op Binary operation functor
  template <typename Other, typename BinaryOp>
  inline void Broadcast(const Other& other, BinaryOp op);

 private:
  Memory<T> data_;  ///< Underlying memory storage
  MapType map_;     ///< Layout map (shape + strides + mapping logic)

  /// @brief Grow capacity to accommodate at least min_size elements
  /// @param min_size Minimum required capacity
  inline void GrowMemory(int min_size);

  /// @brief Recursive print implementation
  /// @param os Output stream
  /// @param axis Current axis being printed
  /// @param offset Current offset in data
  /// @param indent Current indentation level
  void PrintImpl(std::ostream& os, int axis, int offset, int indent) const;

  /// @brief Check bounds for linear index (debug builds only)
  /// @param offset Linear offset to check
  inline void CheckBounds(int offset) const;

  /// @brief Check bounds for multi-dimensional indices (debug builds only)
  /// @param indices Indices to check
  template <typename... Indices>
  inline void CheckBounds(Indices... indices) const;

  ASC_STATIC_ASSERT(Trivial<T>,
                       "DenseMArray element type T must be a trivial type");
  ASC_STATIC_ASSERT(IsMShape<Shape>::value, "Shape must be an MShape type");
  ASC_STATIC_ASSERT(Shape::IsSupported(),
                       "DenseMArray supports only fully static or fully "
                       "dynamic MShape extents.");
  ASC_STATIC_ASSERT(IsLayout<Layout>::value,
                       "Layout must be a valid layout type");
};

template <typename T, typename Shape, typename Layout>
inline void Swap(DenseMArray<T, Shape, Layout>& lhs,
                 DenseMArray<T, Shape, Layout>& rhs) {
  std::swap(lhs.data_, rhs.data_);
  std::swap(lhs.map_, rhs.map_);
}

template <typename T, typename Shape, typename Layout>
inline bool operator==(const DenseMArray<T, Shape, Layout>& lhs,
                       const DenseMArray<T, Shape, Layout>& rhs) {
  if (lhs.GetSize() != rhs.GetSize()) return false;
  if (lhs.GetShape().GetSize() != rhs.GetShape().GetSize()) return false;

  const int rank = lhs.GetShape().GetRank();
  for (int i = 0; i < rank; ++i) {
    if (lhs.GetExtent(i) != rhs.GetExtent(i)) return false;
  }

  const T* lhs_data = lhs.HostRead();
  const T* rhs_data = rhs.HostRead();
  for (int i = 0; i < lhs.GetSize(); ++i) {
    if (lhs_data[i] != rhs_data[i]) return false;
  }
  return true;
}

}  // namespace asc

// Include expression template system before implementation
// This allows DenseMArray::operator= to detect and evaluate expressions
#include "asc/array/expr.h"

#include "asc/array/dsmarray_impl.h"

// ============================================================================
// Common type aliases for DenseMArray
// ============================================================================

namespace asc {

/// Macro for generating dynamic-size vector type aliases
#define ASC_MAKE_DYNAMIC_VECTOR(Type, Suffix)      \
  extern template class DenseMArray<Type, DShape<1>>; \
  using VectorX##Suffix = DenseMArray<Type, DShape<1>>;

/// Macro for generating dynamic-size matrix type aliases
#define ASC_MAKE_DYNAMIC_MATRIX(Type, Suffix)      \
  extern template class DenseMArray<Type, DShape<2>>; \
  using MatrixX##Suffix = DenseMArray<Type, DShape<2>>;

/// Macro for generating static-size vector type aliases
#define ASC_MAKE_STATIC_VECTOR(Type, Suffix, Size)    \
  extern template class DenseMArray<Type, MShape<Size>>; \
  using Vector##Size##Suffix = DenseMArray<Type, MShape<Size>>;

/// Macro for generating static-size matrix type aliases
#define ASC_MAKE_STATIC_MATRIX(Type, Suffix, Size)          \
  extern template class DenseMArray<Type, MShape<Size, Size>>; \
  using Matrix##Size##Suffix = DenseMArray<Type, MShape<Size, Size>>;

// Generate type aliases for dynamic vectors
ASC_MAKE_DYNAMIC_VECTOR(int, i)
ASC_MAKE_DYNAMIC_VECTOR(bool, b)
ASC_MAKE_DYNAMIC_VECTOR(real_t, r)

// Generate type aliases for dynamic matrices
ASC_MAKE_DYNAMIC_MATRIX(int, i)
ASC_MAKE_DYNAMIC_MATRIX(bool, b)
ASC_MAKE_DYNAMIC_MATRIX(real_t, r)

// Generate type aliases for static vectors
ASC_MAKE_STATIC_VECTOR(int, i, 2)
ASC_MAKE_STATIC_VECTOR(int, i, 3)
ASC_MAKE_STATIC_VECTOR(int, i, 4)
ASC_MAKE_STATIC_VECTOR(bool, b, 2)
ASC_MAKE_STATIC_VECTOR(bool, b, 3)
ASC_MAKE_STATIC_VECTOR(bool, b, 4)
ASC_MAKE_STATIC_VECTOR(real_t, r, 2)
ASC_MAKE_STATIC_VECTOR(real_t, r, 3)
ASC_MAKE_STATIC_VECTOR(real_t, r, 4)

// Generate type aliases for static matrices
ASC_MAKE_STATIC_MATRIX(int, i, 2)
ASC_MAKE_STATIC_MATRIX(int, i, 3)
ASC_MAKE_STATIC_MATRIX(int, i, 4)
ASC_MAKE_STATIC_MATRIX(bool, b, 2)
ASC_MAKE_STATIC_MATRIX(bool, b, 3)
ASC_MAKE_STATIC_MATRIX(bool, b, 4)
ASC_MAKE_STATIC_MATRIX(real_t, r, 2)
ASC_MAKE_STATIC_MATRIX(real_t, r, 3)
ASC_MAKE_STATIC_MATRIX(real_t, r, 4)

#undef ASC_MAKE_DYNAMIC_VECTOR
#undef ASC_MAKE_DYNAMIC_MATRIX
#undef ASC_MAKE_STATIC_VECTOR
#undef ASC_MAKE_STATIC_MATRIX

}  // namespace asc

#endif  // ASC_DSMARRAY_H_
