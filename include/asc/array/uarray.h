// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/uarray.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_UARRAY_H_
#define ASC_UARRAY_H_

/// @file uarray.h
/// @brief Dynamic array container with heterogeneous memory support
///
/// This module provides the UArray<T> class, a dynamically-sized array
/// container that integrates with ASC's heterogeneous memory system.
/// It combines features of std::vector with automatic host/device memory
/// management.
///
/// @par Key Features:
/// - Dynamic resizing with automatic capacity management
/// - Integration with Memory<T> for host/device synchronization
/// - Support for different memory types (host, device, managed)
/// - Reference semantics via MakeRef (non-owning views)
/// - STL-compatible interface (iterators, push_back, etc.)
/// - Utility methods: Sort, Unique, PartialSum, Find, etc.
/// - Serialization support (Save/Load)
///
/// @par Memory Management:
/// UArray uses Memory<T> internally, inheriting all its capabilities:
/// - Automatic synchronization between host and device
/// - Multiple memory types (kHost, kDevice, kManaged)
/// - Ownership tracking (owns data vs. wraps external pointers)
///
/// @par Type Requirements:
/// Element type T must be trivial (checked at compile-time via static_assert).
/// This ensures safe copying between host and device memory.
///
/// @par Usage Examples:
/// @code
/// // Basic usage
/// asc::UArray<int> arr(10);     // Allocate 10 elements
/// for (int i = 0; i < arr.Size(); i++) {
///   arr[i] = i * 2;
/// }
///
/// // Initializer list
/// asc::UArray<double> values = {1.0, 2.0, 3.0, 4.0};
///
/// // Dynamic operations
/// arr.Append(42);                  // Add element
/// arr.Sort();                      // Sort in-place
/// int idx = arr.Find(42);          // Find element
///
/// // Device memory (CUDA)
/// asc::UArray<real_t> gpu_data(100, asc::MemoryType::kDevice);
/// auto* dev_ptr = gpu_data.Write(true);  // Get device pointer
///
/// // Non-owning reference
/// asc::UArray<int> ref;
/// ref.MakeRef(arr);                // Reference arr's data
///
/// // Algorithm usage
/// arr.Sort();                      // Sort
/// arr.Unique();                    // Remove duplicates
/// int sum = arr.Sum();             // Compute sum
/// @endcode

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <type_traits>
#include <initializer_list>
#include <utility>

#include "asc/core/globals.h"
#include "asc/core/error.h"
#include "asc/core/memory.h"
#include "asc/core/device.h"

namespace asc {

template <typename T>
class UArray;

template <typename T>
void Swap(UArray<T>&, UArray<T>&);

/// @brief Dynamic array container with heterogeneous memory support
///
/// UArray<T> is a resizable array that integrates with ASC's Memory
/// system, providing automatic host/device synchronization. It offers
/// an STL-like interface while supporting GPU computing.
///
/// @tparam T Element type (must be trivial/POD type)
///
/// @par Growth Strategy:
/// When capacity is exceeded, the array grows to max(requested_size,
/// 2*capacity). Use Reserve() to preallocate and avoid repeated reallocations.
///
/// @par Ownership:
/// - Owning: UArray manages memory lifetime (default constructors)
/// - Non-owning: Wraps external memory via MakeRef() or constructor
///
/// @par Thread Safety:
/// UArray is NOT thread-safe. Use separate instances per thread.
///
/// @see Memory for underlying memory management
template <typename T>
class UArray {
 public:
  using ValueType = T;

  static constexpr bool SupportsDeviceExpression = true;

  // ========== Friend Functions ==========

  friend void Swap<T>(UArray<T>&, UArray<T>&);

  // ========== Constructors and Destructor ==========

  /// @brief Default constructor - creates an empty array
  inline UArray() : size_(0) {}

  /// @brief Construct empty array with specified memory type
  /// @param mt Memory type for allocation (kHost, kDevice, kManaged, etc.)
  inline explicit UArray(MemoryType mt) : data_(mt), size_(0) {}

  /// @brief Construct array with specified size (host memory by default)
  /// @param asize Number of elements to allocate
  explicit inline UArray(int asize) : size_(asize) {
    if (asize > 0) {
      data_.New(asize);
    }
  }

  /// @brief Construct array with specified size and memory type
  /// @param asize Number of elements to allocate
  /// @param mt Memory type for allocation
  inline UArray(int asize, MemoryType mt) : data_(mt), size_(asize) {
    if (asize > 0) {
      data_.New(asize, mt);
    }
  }

  /// @brief Construct non-owning array wrapping external data
  /// @param data Pointer to external data
  /// @param asize Number of elements in the array
  /// @param own_data If true, UArray takes ownership and will delete data
  inline UArray(T* data, int asize, bool own_data = false) {
    data_.Wrap(data, asize, own_data);
    size_ = asize;
  }

  /// @brief Copy constructor - deep copy
  /// @param src Source array to copy from
  inline UArray(const UArray& src);

  /// @brief Type-converting copy constructor
  /// @tparam CT Source element type (must be convertible to T)
  /// @param src Source array to copy from
  template <typename CT>
  inline UArray(const UArray<CT>& src);

  /// @brief Construct from C-style array
  /// @tparam CT Element type of source array
  /// @tparam N Size of source array
  /// @param values C-style array to copy from
  template <typename CT, int N>
  explicit inline UArray(const CT (&values)[N]);

  /// @brief Construct from initializer list
  /// @tparam CT Element type (must be convertible to T)
  /// @param values Initializer list, e.g., {1, 2, 3}
  template <typename CT>
    requires ConvertibleTo<CT, T>
  explicit inline UArray(std::initializer_list<CT> values);

  /// @brief Move constructor
  /// @param src Source array (will be reset after move)
  inline UArray(UArray<T>&& src) : UArray() { Swap(src, *this); }

  /// @brief Destructor - deletes owned memory
  inline ~UArray() { data_.Delete(); }

  // ========== Assignment Operators ==========

  /// @brief Copy assignment operator
  /// @param src Source array to copy from
  /// @return Reference to this array
  UArray<T>& operator=(const UArray<T>& src) {
    src.Copy(*this);
    return *this;
  }

  /// @brief Move assignment operator
  /// @param src Source array to move from
  /// @return Reference to this array
  UArray<T>& operator=(UArray<T>&& src) {
    Swap(src, *this);
    return *this;
  }

  /// @brief Type-converting assignment operator
  /// @tparam CT Source element type
  /// @param src Source array to copy from
  /// @return Reference to this array
  template <typename CT>
  inline UArray& operator=(const UArray<CT>& src);

  /// @brief Implicit conversion to raw pointer
  /// @return Pointer to the underlying data
  inline operator T*() { return HostReadWrite(); }

  /// @brief Implicit conversion to const raw pointer
  /// @return Const pointer to the underlying data
  inline operator const T*() const { return HostRead(); }

  /// ========== Data Access and Management ==========

  /// @brief Get pointer to underlying data
  /// @return Pointer to the data array
  inline T* GetData() { return HostReadWrite(); }

  /// @brief Get const pointer to underlying data
  /// @return Const pointer to the data array
  inline const T* GetData() const { return HostRead(); }

  /// @brief Access the underlying Memory object
  /// @return Reference to the Memory<T> object
  Memory<T>& GetMemory() { return data_; }

  /// @brief Access the underlying Memory object (const)
  /// @return Const reference to the Memory<T> object
  const Memory<T>& GetMemory() const { return data_; }

  /// @brief Check if device memory is being used
  /// @return True if device operations are enabled
  inline bool UseDevice() const { return data_.UseDevice(); }

  /// @brief Set whether to use device memory
  /// @param use_dev True to enable device memory operations, false otherwise
  inline void UseDevice(bool use_dev) { data_.UseDevice(use_dev); }

  /// @brief Check if this array owns its data
  /// @return True if the array owns and will delete its data
  inline bool OwnsData() const { return data_.OwnsHostPtr(); }

  /// @brief Transfer ownership of data to caller and reset this array
  /// @param p Output pointer that will receive the data pointer
  /// @warning Caller is responsible for deallocating the memory
  inline void StealData(T** p) {
    *p = HostReadWrite();
    data_.Reset();
    size_ = 0;
  }

  /// @brief Release ownership without deleting data
  /// @warning Memory leak will occur if data is not tracked elsewhere
  inline void LoseData() {
    data_.Reset();
    size_ = 0;
  }

  /// @brief Make this array take ownership of its current data
  inline void MakeDataOwner() { data_.SetHostPtrOwner(true); }

  /// @brief Get the current number of elements
  /// @return Number of elements in the array
  inline int GetSize() const { return size_; }

  /// @brief Resize the array (reallocates if needed)
  /// @param nsize New size (must be non-negative)
  inline void SetSize(int nsize);

  /// @brief Resize and initialize new elements with a value
  /// @param nsize New size
  /// @param initval Value to assign to new elements (if growing)
  inline void SetSize(int nsize, const T& initval);

  /// @brief Resize and change memory type
  /// @param nsize New size
  /// @param mt New memory type (may trigger reallocation)
  inline void SetSize(int nsize, MemoryType mt);

  /// @brief Get the current capacity (allocated size)
  /// @return Number of elements that can be stored without reallocation
  inline int Capacity() const { return data_.GetCapacity(); }

  /// @brief Reserve space for at least the specified capacity
  /// @param capacity Minimum capacity to reserve
  /// @note Does not change Size(), only ensures capacity
  inline void Reserve(int capacity) {
    if (capacity > Capacity()) {
      GrowSize(capacity);
    }
  }

  /// @brief Element access with bounds checking (debug builds)
  /// @param i Index (must be in [0, Size()))
  /// @return Reference to element at index i
  inline T& operator[](int i);

  /// @brief Element access with bounds checking (const, debug builds)
  /// @param i Index (must be in [0, Size()))
  /// @return Const reference to element at index i
  inline const T& operator[](int i) const;

  /// @brief Append element to end (grows array if needed)
  /// @param el Element to append
  /// @return New size of the array
  inline int Append(const T& el);

  /// @brief Append multiple elements from raw array
  /// @param els Pointer to elements to append
  /// @param nels Number of elements to append
  /// @return New size of the array
  inline int Append(const T* els, int nels);

  /// @brief Append another UArray
  /// @param els Array to append
  /// @return New size of the array
  inline int Append(const UArray<T>& els) { return Append(els, els.GetSize()); }

  /// @brief Insert element at the beginning
  /// @param el Element to prepend
  /// @return New size of the array
  /// @note O(n) operation - shifts all elements
  inline int Prepend(const T& el);

  /// @brief Access the last element
  /// @return Reference to the last element
  /// @warning Array must not be empty
  inline T& Last();

  /// @brief Access the last element (const)
  /// @return Const reference to the last element
  /// @warning Array must not be empty
  inline const T& Last() const;

  /// @brief Add element if not already present (set union)
  /// @param el Element to add
  /// @return Index of element (existing or newly added)
  /// @note O(n) search - considers duplicates
  inline int Union(const T& el);

  /// @brief Find element in array (linear search)
  /// @param el Element to find
  /// @return Index of first occurrence, or -1 if not found
  inline int Find(const T& el) const;

  /// @brief Find element in sorted array (binary search)
  /// @param el Element to find
  /// @return Index of element, or -1 if not found
  /// @warning Array must be sorted
  inline int FindSorted(const T& el) const;

  /// @brief Remove the last element (if any)
  inline void DeleteLast() {
    if (size_ > 0) {
      size_--;
    }
  }

  /// @brief Remove first occurrence of element
  /// @param el Element to remove
  /// @note O(n) operation - shifts elements
  inline void DeleteFirst(const T& el);

  /// @brief Delete all elements and free memory
  inline void DeleteAll();

  /// @brief Reduce capacity to match size (free unused memory)
  inline void ShrinkToFit();

  /// @brief Deep copy this array to another
  /// @param copy Destination array
  inline void Copy(UArray& copy) const;

  /// @brief Make this a non-owning reference to external data
  /// @param data Pointer to external data
  /// @param size Number of elements
  /// @param own_data If true, take ownership of the data
  inline void MakeRef(T* data, int size, bool own_data = false);

  /// @brief Make this a non-owning reference with specific memory type
  /// @param data Pointer to external data
  /// @param size Number of elements
  /// @param mt Memory type of the data
  /// @param own_data If true, take ownership of the data
  inline void MakeRef(T* data, int size, MemoryType mt, bool own_data);

  /// @brief Make this a reference (alias) to another UArray
  /// @param master Source array to reference
  inline void MakeRef(const UArray& master);

  /// @brief Reorder elements according to permutation indices
  /// @tparam I Index container type (e.g., UArray<int>)
  /// @param indices Permutation indices (modified during operation)
  template <typename I>
  inline void Permute(I&& indices);

  /// @brief Reorder elements (copy version for lvalue indices)
  /// @tparam I Index container type
  /// @param indices Permutation indices
  template <typename I>
  inline void Permute(const I& indices) {
    Permute(I(indices));
  }

  /// @brief Extract subarray into new array
  /// @param offset Starting index
  /// @param sa_size Number of elements to extract
  /// @param sa Output subarray
  inline void GetSubUArray(int offset, int sa_size, UArray<T>& sa) const;

  /// @brief Print array to output stream
  /// @param out Output stream (default: asc::mout)
  /// @param width Field width for formatting (default: 4)
  void Print(std::ostream& out = asc::mout, int width = 4) const;

  /// @brief Save array to output stream
  /// @param out Output stream
  /// @param fmt Format flag (0: include size, 1: data only)
  void Save(std::ostream& out, int fmt = 0) const;

  /// @brief Load array from input stream
  /// @param in Input stream
  /// @param fmt Format flag (0: read size first, 1: data only)
  void Load(std::istream& in, int fmt = 0);

  /// @brief Load array with known size
  /// @param new_size Size to allocate
  /// @param in Input stream (data only, no size)
  void Load(int new_size, std::istream& in) {
    SetSize(new_size);
    Load(in, 1);
  }

  /// @brief Find maximum element
  /// @return Maximum value in the array
  /// @warning Array must not be empty
  T Max() const;

  /// @brief Find minimum element
  /// @return Minimum value in the array
  /// @warning Array must not be empty
  T Min() const;

  /// @brief Sort array in ascending order
  void Sort() {
    T* data = HostReadWrite();
    std::sort(data, data + size_);
  }

  /// @brief Sort array with custom comparator
  /// @tparam Compare Comparison function type
  /// @param cmp Comparison function
  template <class Compare>
  void Sort(Compare cmp) {
    T* data = HostReadWrite();
    std::sort(data, data + size_, cmp);
  }

  /// @brief Remove consecutive duplicate elements
  /// @note Array should be sorted first for full duplicate removal
  void Unique() {
    T* data = HostReadWrite();
    T* end = std::unique(data, data + size_);
    SetSize(static_cast<int>(end - data));
  }

  /// @brief Check if array is sorted in ascending order
  /// @return 1 if sorted, 0 otherwise
  int IsSorted() const;

  /// @brief Check if array is empty
  /// @return True if Size() == 0
  bool IsEmpty() const { return GetSize() == 0; }

  /// @brief Compute partial (prefix) sum in-place
  /// @note After this, element i contains sum of original elements [0, i]
  void PartialSum();

  /// @brief Take absolute value of all elements in-place
  /// @note Requires T to be arithmetic type
  void Abs();

  /// @brief Compute sum of all elements
  /// @return Sum of all elements
  T Sum() const;

  /// @brief Assign same value to all elements
  /// @param a Value to assign
  inline void operator=(const T& a);

  /// @brief Assign data from external host pointer
  /// @param ptr Source host pointer
  inline void Assign(const T* ptr) { data_.CopyFromHost(ptr, GetSize()); }

  /// @brief Copy data to external host memory pointer
  /// @param dest Destination host pointer
  inline void CopyToHost(T* dest) const { data_.CopyToHost(dest, GetSize()); }

  /// @brief Copy data from external host pointer
  /// @param src Source host pointer
  inline void CopyFromHost(const T* src) {
    if (GetSize() == 0) return;
    data_.CopyFromHost(src, GetSize());
  }

  /// @brief Copy data to external UArray object
  /// @param dest Destination UArray object
  inline void CopyTo(UArray& dest) const {
    dest.SetSize(GetSize(), data_.GetMemoryType());
    data_.CopyTo(dest.data_, GetSize());
    dest.data_.UseDevice(data_.UseDevice());
  }

  /// @brief Copy data from external UArray object
  /// @param src Source UArray object
  inline void CopyFrom(const UArray& src) {
    if (this == &src) return;
    SetSize(src.GetSize(), src.data_.GetMemoryType());
    data_.CopyFrom(src.data_, GetSize());
    data_.UseDevice(src.data_.UseDevice());
  }

  /// @brief Get iterator to beginning
  /// @return Pointer to first element
  inline T* begin() { return HostReadWrite(); }

  /// @brief Get iterator to end
  /// @return Pointer to one past last element
  inline T* end() { return HostReadWrite() + size_; }

  /// @brief Get const iterator to beginning
  /// @return Const pointer to first element
  inline const T* begin() const { return HostRead(); }

  /// @brief Get const iterator to end
  /// @return Const pointer to one past last element
  inline const T* end() const { return HostRead() + size_; }

  /// @brief Get total memory usage in bytes
  /// @return Capacity() * sizeof(T)
  std::size_t MemoryUsage() const { return Capacity() * sizeof(T); }

  /// @brief Get read-only pointer (synchronizes data if needed)
  /// @param on_dev If true, get device pointer; else get host pointer
  /// @return Const pointer to data
  const T* Read(bool on_dev = true) const {
    return asc::Read(data_, size_, on_dev);
  }

  /// @brief Get read-only host pointer
  /// @return Const pointer to host data
  const T* HostRead() const { return asc::Read(data_, size_, false); }

  /// @brief Get write-only pointer (invalidates other copy)
  /// @param on_dev If true, get device pointer; else get host pointer
  /// @return Pointer to data
  T* Write(bool on_dev = true) { return asc::Write(data_, size_, on_dev); }

  /// @brief Get write-only host pointer
  /// @return Pointer to host data
  T* HostWrite() { return asc::Write(data_, size_, false); }

  /// @brief Get read-write pointer (synchronizes data if needed)
  /// @param on_dev If true, get device pointer; else get host pointer
  /// @return Pointer to data
  T* ReadWrite(bool on_dev = true) {
    return asc::ReadWrite(data_, size_, on_dev);
  }

  /// @brief Get read-write host pointer
  /// @return Pointer to host data
  T* HostReadWrite() { return asc::ReadWrite(data_, size_, false); }

 protected:
  Memory<T> data_;  ///< Underlying memory storage
  int size_;        ///< Current number of elements

  /// @brief Grow capacity to accommodate at least minsize elements
  /// @param minsize Minimum required capacity
  inline void GrowSize(int minsize);

  static_assert(std::is_trivially_copyable<T>::value,
                "type T must be trivially copyable");
};

/// @brief Equality comparison for UArray
/// @tparam T Element type
/// @param LHS Left-hand side array
/// @param RHS Right-hand side array
/// @return True if arrays have same size and all elements are equal
template <typename T>
inline bool operator==(const UArray<T>& LHS, const UArray<T>& RHS) {
  if (LHS.GetSize() != RHS.GetSize()) {
    return false;
  }
  for (int i = 0; i < LHS.GetSize(); i++) {
    if (LHS[i] != RHS[i]) {
      return false;
    }
  }
  return true;
}

/// @brief Inequality comparison for UArray
/// @tparam T Element type
/// @param LHS Left-hand side array
/// @param RHS Right-hand side array
/// @return True if arrays differ in size or any element
template <typename T>
inline bool operator!=(const UArray<T>& LHS, const UArray<T>& RHS) {
  return !(LHS == RHS);
}

/// @brief Return const reference to object
/// @tparam T Object type
/// @param a Object
/// @return Const reference to a
template <typename T>
const T& AsConst(const T& a) {
  return a;
}

/// @brief Swap two objects
/// @tparam T Object type
/// @param a First object
/// @param b Second object
template <typename T>
inline void Swap(T& a, T& b) {
  T c = a;
  a = b;
  b = c;
}

/// @brief Swap two UArray objects (efficient, O(1))
/// @tparam T Element type
/// @param a First array
/// @param b Second array
template <typename T>
inline void Swap(UArray<T>& a, UArray<T>& b) {
  Swap(a.data_, b.data_);
  Swap(a.size_, b.size_);
}

template <typename T>
inline UArray<T>::UArray(const UArray& src) : size_(src.GetSize()) {
  size_ > 0 ? data_.New(size_, src.data_.GetMemoryType()) : data_.Reset();
  data_.CopyFrom(src.data_, size_);
  data_.UseDevice(src.data_.UseDevice());
}

template <typename T>
template <typename CT>
inline UArray<T>::UArray(const UArray<CT>& src) : size_(src.GetSize()) {
  size_ > 0 ? data_.New(size_) : data_.Reset();
  for (int i = 0; i < size_; i++) {
    (*this)[i] = T(src[i]);
  }
}

template <typename T>
template <typename CT>
  requires ConvertibleTo<CT, T>
inline UArray<T>::UArray(std::initializer_list<CT> values)
    : UArray(values.size()) {
  std::copy(values.begin(), values.end(), begin());
}

template <typename T>
template <typename CT, int N>
inline UArray<T>::UArray(const CT (&values)[N]) : UArray(N) {
  std::copy(values, values + N, begin());
}

template <typename T>
inline void UArray<T>::GrowSize(int minsize) {
  const int nsize = std::max(minsize, 2 * data_.GetCapacity());
  Memory<T> p(nsize, data_.GetMemoryType());
  p.CopyFrom(data_, size_);
  p.UseDevice(data_.UseDevice());
  data_.Delete();
  data_ = p;
}

template <typename T>
inline void UArray<T>::ShrinkToFit() {
  if (Capacity() == size_) {
    return;
  }
  Memory<T> p(size_, data_.GetMemoryType());
  p.CopyFrom(data_, size_);
  p.UseDevice(data_.UseDevice());
  data_.Delete();
  data_ = p;
}

template <typename T>
template <typename I>
inline void UArray<T>::Permute(I&& indices) {
  T* data = HostReadWrite();
  for (int i = 0; i < size_; i++) {
    auto current = i;
    while (i != indices[current]) {
      auto next = indices[current];
      std::swap(data[current], data[next]);
      indices[current] = current;
      current = next;
    }
    indices[current] = current;
  }
}

template <typename T>
template <typename CT>
inline UArray<T>& UArray<T>::operator=(const UArray<CT>& src) {
  SetSize(src.GetSize());
  for (int i = 0; i < size_; i++) {
    (*this)[i] = T(src[i]);
  }
  return *this;
}

template <typename T>
inline void UArray<T>::SetSize(int nsize) {
  ASC_VERIFY(nsize >= 0, "Size must be non-negative.  It is " << nsize);
  if (nsize > Capacity()) {
    GrowSize(nsize);
  }
  size_ = nsize;
}

template <typename T>
inline void UArray<T>::SetSize(int nsize, const T& initval) {
  ASC_VERIFY(nsize >= 0, "Size must be non-negative.  It is " << nsize);
  if (nsize > size_) {
    if (nsize > Capacity()) {
      GrowSize(nsize);
    }
    T* data = HostReadWrite();
    for (int i = size_; i < nsize; i++) {
      data[i] = initval;
    }
  }
  size_ = nsize;
}

template <typename T>
inline void UArray<T>::SetSize(int nsize, MemoryType mt) {
  ASC_VERIFY(nsize >= 0, "invalid new size: " << nsize);
  if (mt == data_.GetMemoryType()) {
    if (nsize <= Capacity()) {
      size_ = nsize;
      return;
    }
  }
  const bool use_dev = data_.UseDevice();
  data_.Delete();
  if (nsize > 0) {
    data_.New(nsize, mt);
    size_ = nsize;
  } else {
    data_.Reset();
    size_ = 0;
  }
  data_.UseDevice(use_dev);
}

template <typename T>
inline T& UArray<T>::operator[](int i) {
  ASC_ASSERT(i >= 0 && i < size_,
                "Access element " << i << " of array, size = " << size_);
  return HostReadWrite()[i];
}

template <typename T>
inline const T& UArray<T>::operator[](int i) const {
  ASC_ASSERT(i >= 0 && i < size_,
                "Access element " << i << " of array, size = " << size_);
  return HostRead()[i];
}

template <typename T>
inline int UArray<T>::Append(const T& el) {
  SetSize(size_ + 1);
  HostReadWrite()[size_ - 1] = el;
  return size_;
}

template <typename T>
inline int UArray<T>::Append(const T* els, int nels) {
  ASC_VERIFY(nels >= 0, "Number of appended elements must be non-negative.");
  const int old_size = size_;

  SetSize(size_ + nels);
  T* data = HostReadWrite();
  for (int i = 0; i < nels; i++) {
    data[old_size + i] = els[i];
  }
  return size_;
}

template <typename T>
inline int UArray<T>::Prepend(const T& el) {
  SetSize(size_ + 1);
  T* data = HostReadWrite();
  for (int i = size_ - 1; i > 0; i--) {
    data[i] = data[i - 1];
  }
  data[0] = el;
  return size_;
}

template <typename T>
inline T& UArray<T>::Last() {
  ASC_VERIFY(size_ > 0, "UArray size is zero: " << size_);
  return HostReadWrite()[size_ - 1];
}

template <typename T>
inline const T& UArray<T>::Last() const {
  ASC_VERIFY(size_ > 0, "UArray size is zero: " << size_);
  return HostRead()[size_ - 1];
}

template <typename T>
inline int UArray<T>::Union(const T& el) {
  T* data = HostReadWrite();
  int i = 0;
  while ((i < size_) && (data[i] != el)) {
    i++;
  }
  if (i == size_) {
    Append(el);
  }
  return i;
}

template <typename T>
inline int UArray<T>::Find(const T& el) const {
  const T* data = HostRead();
  for (int i = 0; i < size_; i++) {
    if (data[i] == el) {
      return i;
    }
  }
  return -1;
}

template <typename T>
inline int UArray<T>::FindSorted(const T& el) const {
  const T *begin = HostRead(), *end = begin + size_;
  const T* first = std::lower_bound(begin, end, el);
  if (first == end || !(*first == el)) {
    return -1;
  }
  return static_cast<int>(first - begin);
}

template <typename T>
inline void UArray<T>::DeleteFirst(const T& el) {
  T* data = HostReadWrite();
  for (int i = 0; i < size_; i++) {
    if (data[i] == el) {
      for (i++; i < size_; i++) {
        data[i - 1] = data[i];
      }
      size_--;
      return;
    }
  }
}

template <typename T>
inline void UArray<T>::DeleteAll() {
  const bool use_dev = data_.UseDevice();
  data_.Delete();
  data_.Reset();
  size_ = 0;
  data_.UseDevice(use_dev);
}

template <typename T>
inline void UArray<T>::Copy(UArray& copy) const {
  copy.SetSize(GetSize(), data_.GetMemoryType());
  data_.CopyTo(copy.data_, GetSize());
  copy.data_.UseDevice(data_.UseDevice());
}

template <typename T>
inline void UArray<T>::MakeRef(T* data, int size, bool own_data) {
  data_.Delete();
  data_.Wrap(data, size, own_data);
  size_ = size;
}

template <typename T>
inline void UArray<T>::MakeRef(T* data, int size, MemoryType mt,
                               bool own_data) {
  data_.Delete();
  data_.Wrap(data, size, mt, own_data);
  size_ = size;
}

template <typename T>
inline void UArray<T>::MakeRef(const UArray& master) {
  data_.Delete();
  size_ = master.size_;
  data_.MakeAlias(master.GetMemory(), 0, size_);
}

template <typename T>
inline void UArray<T>::GetSubUArray(int offset, int sa_size,
                                    UArray<T>& sa) const {
  ASC_VERIFY(offset >= 0 && sa_size >= 0 && offset + sa_size <= size_,
                "Subarray range is out of bounds.");
  sa.SetSize(sa_size);
  const T* src = HostRead();
  T* dest = sa.HostWrite();
  for (int i = 0; i < sa_size; i++) {
    dest[i] = src[offset + i];
  }
}

template <typename T>
inline void UArray<T>::operator=(const T& a) {
  T* data = HostReadWrite();
  for (int i = 0; i < size_; i++) {
    data[i] = a;
  }
}

}  // namespace asc

#endif  // ASC_UARRAY_H_
