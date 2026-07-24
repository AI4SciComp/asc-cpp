// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/memory.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MEMORY_H_
#define ASC_MEMORY_H_

/// @file memory.h
/// @brief Unified memory management for heterogeneous computing (CPU/GPU)
///
/// This module provides a comprehensive memory abstraction that transparently
/// handles both host (CPU) and device (GPU) memory allocations with automatic
/// data synchronization. Key features include:
/// - Unified Memory<T> class supporting host and device memory
/// - Automatic synchronization between host and device
/// - Support for various memory types (host, device, managed, pinned)
/// - Memory aliasing for sub-array views without copying
/// - Ownership tracking and automatic deallocation
/// - Integration with CUDA for GPU computing
///
/// @par Memory Types:
/// - MemoryType::kHost: Standard host memory (new/delete)
/// - MemoryType::kHost32: 32-byte aligned host memory
/// - MemoryType::kHost64: 64-byte aligned host memory
/// - MemoryType::kManaged: CUDA unified memory (accessible from both host and
/// device)
/// - MemoryType::kDevice: CUDA device memory (GPU only)
///
/// @par Usage Examples:
/// @code
/// // Basic host memory allocation
/// asc::Memory<double> host_data(100);  // 100 doubles on host
/// double* ptr = host_data;                 // Implicit conversion
/// host_data[0] = 3.14;                     // Direct access
///
/// // Device memory with synchronization
/// asc::Memory<real_t> gpu_data(1000, asc::MemoryType::kDevice);
/// auto* d_ptr = gpu_data.Write(asc::MemoryClass::kDevice, 1000);
/// // Use d_ptr in CUDA kernel...
/// auto* h_ptr = gpu_data.Read(asc::MemoryClass::kHost, 1000);
///
/// // Managed (unified) memory
/// asc::Memory<int> unified(500, asc::MemoryType::kManaged);
/// // Accessible from both host and device automatically
///
/// // Memory aliasing (zero-copy subviews)
/// asc::Memory<float> base(1000);
/// asc::Memory<float> alias(base, 100, 200);  // Elements 100-299
///
/// // Wrap external pointer
/// double* external = new double[50];
/// asc::Memory<double> wrapped;
/// wrapped.Wrap(external, 50, true);  // Takes ownership
///
/// // Copy operations
/// asc::Memory<int> src(100), dst(100);
/// dst.CopyFrom(src, 100);  // Automatic host/device handling
/// @endcode

#include <cstring>
#include <type_traits>
#include <cstddef>

#include "asc/core/globals.h"
#include "asc/core/error.h"
#include "asc/core/casts.h"

namespace asc {

/// @brief Memory allocation type specifier
///
/// Specifies where and how memory should be allocated in a heterogeneous
/// computing environment. Different memory types have different performance
/// characteristics and access patterns.
///
/// @note Performance Notes:
/// - kHost: Fastest for CPU access, not accessible from GPU
/// - kDevice: Fastest for GPU access, requires explicit copy to/from host
/// - kManaged: Accessible from both CPU and GPU, automatic migration
/// (CUDA 6.0+)
/// - kHost32/kHost64: Aligned memory for vectorization (AVX, AVX-512)
enum class MemoryType {
  kHost = 0,  ///< Standard host memory (new/delete)
  kHost32,    ///< 32-byte aligned host memory
  kHost64,    ///< 64-byte aligned host memory
  kManaged,   ///< CUDA unified/managed memory
  kDevice,    ///< CUDA device memory
  kSize,      ///< Sentinel value (number of memory types)
  kDefault    ///< Use default memory type from MemoryManager
};

constexpr int kMemoryTypeSize = static_cast<int>(MemoryType::kSize);
constexpr int kHostMemoryType = static_cast<int>(MemoryType::kHost);
constexpr int kHostMemoryTypeSize = static_cast<int>(MemoryType::kDevice);
constexpr int kDeviceMemoryType = static_cast<int>(MemoryType::kManaged);
constexpr int kDeviceMemoryTypeSize = kMemoryTypeSize - kDeviceMemoryType;

/// @brief Array of memory type names for debugging/logging
extern ASC_EXPORT const char* kMemoryTypeName[kMemoryTypeSize];

/// @brief Memory class categorization for access pattern specification
///
/// Simplified memory classification used when reading/writing data.
/// Automatically maps to the appropriate MemoryType based on context.
enum class MemoryClass { kHost, kHost32, kHost64, kDevice, kManaged };

/// @brief Check if memory type is host-accessible
///
/// @param mt Memory type to check
/// @return true if @a mt is host memory (includes kManaged)
inline bool IsHostMemory(MemoryType mt) { return mt <= MemoryType::kManaged; }

/// @brief Check if memory type is device-accessible
///
/// @param mt Memory type to check
/// @return true if @a mt is device memory (includes kManaged)
inline bool IsDeviceMemory(MemoryType mt) {
  return mt >= MemoryType::kManaged && mt < MemoryType::kSize;
}

/// @brief Get MemoryType corresponding to MemoryClass
/// @param mc Memory class
/// @return memory type
MemoryType GetMemoryType(MemoryClass mc);

/// @brief Check if MemoryClass contains a specific MemoryType
/// @param mc Memory class
/// @param mt Memory type
/// @return true if @a mc contains @a mt
bool MemoryClassContainsType(MemoryClass mc, MemoryType mt);

/// @brief Combine two MemoryClass values (intersection)
/// @param mc1 First memory class
/// @param mc2 Second memory class
/// @return Combined memory class
/// @note This operation is commutative, i.e. a*b = b*a, associative, i.e.
/// (a*b)*c = a*(b*c), and has an identity element: MemoryClass::kHost.
///
/// Currently, the operation is defined as a*b := max(a,b) where the max
/// operation is based on the enumeration ordering:
///
/// kHost < kHost32 < kHost64 < kDevice < kManaged.
MemoryClass operator*(MemoryClass mc1, MemoryClass mc2);

/// @brief Unified memory container with automatic host/device synchronization
///
/// Memory<T> is the core memory abstraction in ASC, providing transparent
/// management of data that may reside on CPU (host), GPU (device), or both.
/// It handles allocation, deallocation, ownership, and automatic data movement
/// between host and device memory spaces.
///
/// @tparam T Element type (must be trivially copyable for device memory)
///
/// @par Key Features:
/// - Automatic synchronization between host and device
/// - Multiple memory types (host, device, managed, pinned)
/// - Ownership tracking (owns vs. wraps external memory)
/// - Memory aliasing for zero-copy sub-views
/// - Validity flags to minimize unnecessary data transfers
/// - Integration with MemoryManager for advanced features
///
/// @par Memory States:
/// Each Memory object tracks two validity flags:
/// - kValidHost: Data in host memory is up-to-date
/// - kValidDevice: Data in device memory is up-to-date
/// Read/Write operations automatically synchronize data as needed.
///
/// @par Ownership:
/// Memory can either own its data (will delete on destruction) or wrap
/// external pointers (caller manages lifetime). Use Wrap() for external data.
///
/// @par Thread Safety:
/// Memory is NOT thread-safe. Use separate instances per thread or add
/// external synchronization.
template <typename T>
class Memory {
 protected:
  friend class MemoryManager;
  friend void MemoryPrintFlags(unsigned flags);

  enum FlagMask : unsigned {
    kRegistered = 1 << 0,
    kOwnsHost = 1 << 1,
    kOwnsDevice = 1 << 2,
    kOwnsInternal = 1 << 3,
    kValidHost = 1 << 4,
    kValidDevice = 1 << 5,
    kUseDevice = 1 << 6,
    kAlias = 1 << 7,
    kDeviceIntent = 1 << 8
  };

  int capacity_;
  T* h_ptr_;
  MemoryType h_mt_;
  mutable unsigned flags_;

 public:
  /// @brief Default constructor (zero-size memory)
  Memory() { Reset(); }

  /// @brief Copy constructor
  Memory(const Memory& orig) = default;

  /// @brief Move constructor
  Memory(Memory&& orig) {
    *this = orig;
    orig.Reset();
  }

  /// @brief Copy assignment
  Memory& operator=(const Memory& orig) = default;

  /// @brief Move assignment
  Memory& operator=(Memory&& orig) {
    if (this == &orig) {
      return *this;
    }
    *this = orig;
    orig.Reset();
    return *this;
  }

  /// @brief Construct with size (allocates memory)
  /// @param size Number of elements to allocate
  explicit Memory(int size) { New(size); }

  /// @brief Construct with specific memory type
  /// @param mt Memory type to use
  explicit Memory(MemoryType mt) { Reset(mt); }

  /// @brief Construct with size and memory type
  /// @param size Number of elements
  /// @param mt Memory type
  Memory(int size, MemoryType mt) { New(size, mt); }

  /// @brief Construct with separate host and device memory types
  /// @param size Number of elements
  /// @param h_mt Host memory type
  /// @param d_mt Device memory type
  Memory(int size, MemoryType h_mt, MemoryType d_mt) { New(size, h_mt, d_mt); }

  /// @brief Wrap existing pointer (optionally taking ownership)
  /// @param ptr External pointer
  /// @param size Number of elements
  /// @param own Take ownership (true) or just wrap (false)
  explicit Memory(T* ptr, int size, bool own) { Wrap(ptr, size, own); }

  /// @brief Wrap pointer with specific memory type
  /// @param ptr External pointer
  /// @param size Number of elements
  /// @param mt Memory type
  /// @param own Take ownership
  Memory(T* ptr, int size, MemoryType mt, bool own) {
    Wrap(ptr, size, mt, own);
  }

  /// @brief Create alias (view) of another Memory object
  /// @param base Base memory to alias
  /// @param offset Starting element offset
  /// @param size Number of elements in view
  Memory(const Memory& base, int offset, int size) {
    MakeAlias(base, offset, size);
  }

  /// @brief Destructor (does NOT free memory - call Delete() explicitly)
  ~Memory() = default;

  /// @brief Check if this Memory object owns the host pointer
  /// @return true if memory will be freed on destruction
  bool OwnsHostPtr() const { return flags_ & kOwnsHost; }

  /// @brief Set host pointer ownership flag
  /// @param own true to take ownership, false to release ownership
  void SetHostPtrOwner(bool own) const {
    flags_ = own ? (flags_ | kOwnsHost) : (flags_ & ~kOwnsHost);
  }

  /// @brief Check if this Memory object owns the device pointer
  /// @return true if device memory will be freed on destruction
  bool OwnsDevicePtr() const { return flags_ & kOwnsDevice; }

  /// @brief Set device pointer ownership flag
  /// @param own true to take ownership, false to release ownership
  void SetDevicePtrOwner(bool own) const {
    flags_ = own ? (flags_ | kOwnsDevice) : (flags_ & ~kOwnsDevice);
  }

  /// @brief Clear all ownership flags (host, device, internal)
  void ClearOwnerFlags() const {
    flags_ = flags_ & ~(kOwnsHost | kOwnsDevice | kOwnsInternal);
  }

  /// @brief Check if device execution is preferred
  /// @return true if device should be used for operations
  bool UseDevice() const { return flags_ & kUseDevice; }

  /// @brief Set device usage preference
  /// @param use_dev true to prefer device operations, false for host
  void UseDevice(bool use_dev) const {
    flags_ = use_dev ? (flags_ | kUseDevice) : (flags_ & ~kUseDevice);
  }

  /// @brief Get allocated capacity in number of elements
  /// @return Number of elements that can be stored without reallocation
  int GetCapacity() const { return capacity_; }

  /// @brief Reset to empty state with default memory type
  void Reset();

  /// @brief Reset to empty state with a preferred allocation type
  /// @param mt Preferred memory type to use for future allocations
  void Reset(MemoryType mt);

  /// @brief Check if memory is empty (null pointer)
  /// @return true if no memory is allocated
  bool IsEmpty() const { return h_ptr_ == nullptr; }

  /// @brief Allocate new memory with default type
  /// @param size Number of elements to allocate
  void New(int size);

  /// @brief Allocate new memory with specific type
  /// @param size Number of elements to allocate
  /// @param mt Memory type (host or device)
  void New(int size, MemoryType mt);

  /// @brief Allocate new memory with separate host and device types
  /// @param size Number of elements to allocate
  /// @param h_mt Host memory type
  /// @param d_mt Device memory type
  void New(int size, MemoryType h_mt, MemoryType d_mt);

  /// @brief Wrap existing pointer (optionally taking ownership)
  /// @param ptr Pointer to wrap
  /// @param size Number of elements
  /// @param own true to take ownership, false to just wrap
  void Wrap(T* ptr, int size, bool own);

  /// @brief Wrap existing pointer with specific memory type
  /// @param ptr Pointer to wrap
  /// @param size Number of elements
  /// @param mt Memory type
  /// @param own true to take ownership, false to just wrap
  void Wrap(T* ptr, int size, MemoryType mt, bool own);

  /// @brief Wrap both host and device pointers
  /// @param h_ptr Host pointer
  /// @param d_ptr Device pointer
  /// @param size Number of elements
  /// @param h_mt Host memory type
  /// @param own true to take ownership
  /// @param valid_host true if host data is valid
  /// @param valid_device true if device data is valid
  void Wrap(T* h_ptr, T* d_ptr, int size, MemoryType h_mt, bool own,
            bool valid_host = false, bool valid_device = true);

  /// @brief Create an alias (non-owning view) of another Memory object
  /// @param base Base Memory object to create alias from
  /// @param offset Starting element offset in base
  /// @param size Number of elements in alias view
  void MakeAlias(const Memory& base, int offset, int size);

  /// @brief Set or change device memory type
  /// @param d_mt New device memory type
  void SetDeviceMemoryType(MemoryType d_mt);

  /// @brief Delete allocated memory (both host and device)
  void Delete();

  /// @brief Delete only device memory
  /// @param copy_to_host true to copy data to host before deleting
  void DeleteDevice(bool copy_to_host = true);

  /// @brief Access element by index (mutable, host-only)
  /// @param idx Element index
  /// @return Reference to element
  T& operator[](int idx);

  /// @brief Access element by index (const, host-only)
  /// @param idx Element index
  /// @return Const reference to element
  const T& operator[](int idx) const;

  /// @brief Implicit conversion to mutable pointer (host)
  /// @return Host pointer
  operator T*();

  /// @brief Implicit conversion to const pointer (host)
  /// @return Const host pointer
  operator const T*() const;

  /// @brief Explicit conversion to mutable pointer of different type
  /// @tparam U Target pointer type
  /// @return Pointer cast to type U*
  template <typename U>
  explicit operator U*();

  /// @brief Explicit conversion to const pointer of different type
  /// @tparam U Target pointer type
  /// @return Pointer cast to type const U*
  template <typename U>
  explicit operator const U*() const;

  /// @brief Get pointer for read-write access with automatic synchronization
  /// @param mc Memory class (host or device)
  /// @param size Number of elements to access
  /// @return Pointer for read-write access
  T* ReadWrite(MemoryClass mc, int size);

  /// @brief Get const pointer for read-only access with automatic
  /// synchronization
  /// @param mc Memory class (host or device)
  /// @param size Number of elements to access
  /// @return Const pointer for read access
  const T* Read(MemoryClass mc, int size) const;

  /// @brief Get pointer for write-only access (invalidates other memory)
  /// @param mc Memory class (host or device)
  /// @param size Number of elements to access
  /// @return Pointer for write access
  T* Write(MemoryClass mc, int size);

  /// @brief Synchronize validity flags with another Memory object
  /// @param other Memory object to sync with
  void Sync(const Memory& other) const;

  /// @brief Synchronize alias validity flags with base Memory
  /// @param base Base Memory object
  /// @param alias_size Size of alias in elements
  void SyncAlias(const Memory& base, int alias_size) const;

  /// @brief Get current memory type (host or device based on validity)
  /// @return Current memory type
  MemoryType GetMemoryType() const;

  /// @brief Get host memory type
  /// @return Host memory type
  MemoryType GetHostMemoryType() const { return h_mt_; }

  /// @brief Get device memory type
  /// @return Device memory type or kHost if not registered
  MemoryType GetDeviceMemoryType() const;

  /// @brief Check if host data is valid
  /// @return true if host contains up-to-date data
  bool HostIsValid() const;

  /// @brief Check if device data is valid
  /// @return true if device contains up-to-date data
  bool DeviceIsValid() const;

  /// @brief Copy data from another Memory object
  /// @param src Source Memory object
  /// @param size Number of elements to copy
  void CopyFrom(const Memory& src, int size);

  /// @brief Copy data from host pointer
  /// @param src Source host pointer
  /// @param size Number of elements to copy
  void CopyFromHost(const T* src, int size);

  /// @brief Copy data to another Memory object
  /// @param dest Destination Memory object
  /// @param size Number of elements to copy
  void CopyTo(Memory& dest, int size) const;

  /// @brief Copy data to host pointer
  /// @param dest Destination host pointer
  /// @param size Number of elements to copy
  void CopyToHost(T* dest, int size) const;

  /// @brief Print memory flags for debugging
  void PrintFlags() const;

  /// @brief Compare host and device data for equality
  /// @param size Number of elements to compare
  /// @return 0 if equal, number of differences otherwise
  int CompareHostAndDevice(int size) const;

 private:
  static constexpr std::size_t DefAlignBytes() {
    return alignof(std::max_align_t);
  }
  static constexpr std::size_t kDefAlignBytes = DefAlignBytes();
  static constexpr std::size_t kNewAlignBytes = alignof(T) > kDefAlignBytes
                                                    ? alignof(T)
                                                    : kDefAlignBytes;

  template <std::size_t align_bytes, bool dummy = true>
  struct Alloc {
#if __cplusplus < 201703L
    static inline T* New(std::size_t) {
      ASC_VERIFY(false, "overaligned type cannot use MemoryType::kHost");
      return nullptr;
    }
#else
    static inline T* New(std::size_t size) { return new T[size]; }
#endif
  };

#if __cplusplus < 201703L
  template <bool dummy>
  struct Alloc<kDefAlignBytes, dummy> {
    static inline T* New(std::size_t size) { return new T[size]; }
  };
#endif

  static inline T* NewHost(std::size_t size) {
    return Alloc<kNewAlignBytes>::New(size);
  }
};

/// @brief Global memory manager for heterogeneous computing
///
/// MemoryManager is a singleton that coordinates all memory operations
/// in ASC, providing the infrastructure for automatic host/device
/// synchronization, memory type tracking, and resource management.
///
/// @par Responsibilities:
/// - Track all registered Memory objects (host/device pointer mappings)
/// - Manage host-device data transfers and synchronization
/// - Handle memory aliasing (subviews) relationships
/// - Coordinate with CUDA runtime for device operations
/// - Maintain dual memory type mappings for heterogeneous allocation
///
/// @par Design Pattern:
/// MemoryManager uses the Singleton pattern with a global instance `mm`.
/// Most operations are performed through static methods called by Memory<T>.
///
/// @par Configuration:
/// Use Configure() to set default host and device memory types:
/// @code
/// asc::mm.Configure(asc::MemoryType::kHost64,
///                      asc::MemoryType::kDevice);
/// @endcode
///
/// @par Memory Registration:
/// Memory objects are registered with MemoryManager when:
/// - Device memory is allocated
/// - Non-trivial host memory types are used (kHost32, kHost64, kManaged)
/// - Explicit synchronization is required
/// Simple host-only Memory objects may not be registered for efficiency.
///
/// @par Thread Safety:
/// MemoryManager is NOT thread-safe. All memory operations should be
/// performed from a single thread or externally synchronized.
///
/// @see Memory for the user-facing memory container
class ASC_EXPORT MemoryManager {
 private:
  typedef MemoryType MemType;
  typedef Memory<int> Mem;

  template <typename T>
  friend class Memory;

  static MemoryType host_mem_type_;
  static MemoryType device_mem_type_;
  static bool exists_;
  static MemoryType dual_map_[kMemoryTypeSize];
  static void UpdateDualMemoryType(MemoryType mt, MemoryType dual_mt);
  static bool configured_;

  static bool Exists() { return exists_; }

 private:
  static void* New_(void* h_tmp, size_t bytes, MemoryType mt, unsigned& flags);
  static void* New_(void* h_tmp, size_t bytes, MemoryType h_mt, MemoryType d_mt,
                    unsigned valid_flags, unsigned& flags);
  static void* Register_(void* ptr, void* h_ptr, size_t bytes, MemoryType mt,
                         bool own, bool alias, unsigned& flags);
  static void Register2_(void* h_ptr, void* d_ptr, size_t bytes,
                         MemoryType h_mt, MemoryType d_mt, bool own, bool alias,
                         unsigned& flags, unsigned valid_flags);
  static void Alias_(void* base_h_ptr, size_t offset, size_t bytes,
                     unsigned base_flags, unsigned& flags);
  static void SetDeviceMemoryType_(void* h_ptr, unsigned flags,
                                   MemoryType d_mt);
  static void Delete_(void* h_ptr, MemoryType mt, unsigned flags);
  static void DeleteDevice_(void* h_ptr, unsigned& flags);
  static bool MemoryClassCheck_(MemoryClass mc, void* h_ptr, MemoryType h_mt,
                                size_t bytes, unsigned flags);
  static void* ReadWrite_(void* h_ptr, MemoryType h_mt, MemoryClass mc,
                          size_t bytes, unsigned& flags);
  static const void* Read_(void* h_ptr, MemoryType h_mt, MemoryClass mc,
                           size_t bytes, unsigned& flags);
  static void* Write_(void* h_ptr, MemoryType h_mt, MemoryClass mc,
                      size_t bytes, unsigned& flags);
  static void SyncAlias_(const void* base_h_ptr, void* alias_h_ptr,
                         size_t alias_bytes, unsigned base_flags,
                         unsigned& alias_flags);
  static MemoryType GetDeviceMemoryType_(void* h_ptr, bool alias);
  static MemoryType GetHostMemoryType_(void* h_ptr);
  static void CheckHostMemoryType_(MemoryType h_mt, void* h_ptr, bool alias);
  static void Copy_(void* dest_h_ptr, const void* src_h_ptr, size_t bytes,
                    unsigned src_flags, unsigned& dest_flags);
  static void CopyToHost_(void* dest_h_ptr, const void* src_h_ptr, size_t bytes,
                          unsigned src_flags);
  static void CopyFromHost_(void* dest_h_ptr, const void* src_h_ptr,
                            size_t bytes, unsigned& dest_flags);
  static bool IsKnown_(const void* h_ptr);
  static bool IsAlias_(const void* h_ptr);
  static int CompareHostAndDevice_(void* h_ptr, size_t size, unsigned flags);

 private:
  void Insert(void* h_ptr, size_t bytes, MemoryType h_mt, MemoryType d_mt);
  void InsertDevice(void* d_ptr, void* h_ptr, size_t bytes, MemoryType h_mt,
                    MemoryType d_mt);
  void InsertAlias(const void* base_ptr, void* alias_ptr, const size_t bytes,
                   const bool base_is_alias);
  void Erase(void* h_ptr, bool free_dev_ptr = true);
  void EraseDevice(void* h_ptr);
  void EraseAlias(void* alias_ptr);
  void* GetDevicePtr(const void* h_ptr, size_t bytes, bool copy_data);
  void* GetAliasDevicePtr(const void* alias_ptr, size_t bytes, bool copy_data);
  void* GetHostPtr(const void* d_ptr, size_t bytes, bool copy_data);
  void* GetAliasHostPtr(const void* alias_ptr, size_t bytes, bool copy_data);

 public:
  /// @brief Constructor - initializes memory manager
  MemoryManager();

  /// @brief Destructor - releases all managed resources
  ~MemoryManager();

  /// @brief Initialize memory manager subsystem
  /// @note Called automatically by global mm instance
  void Init();

  /// @brief Get dual (complementary) memory type for given type
  /// @param mt Memory type to query
  /// @return Dual memory type (e.g., kDevice for kHost)
  /// @note Used for automatic allocation of complementary memory spaces
  static inline MemoryType GetDualMemoryType(MemoryType mt) {
    return dual_map_[static_cast<int>(mt)];
  }

  /// @brief Set dual memory type mapping
  /// @param mt Source memory type
  /// @param dual_mt Dual (complementary) memory type
  /// @note Updates bidirectional mapping: mt <-> dual_mt
  static void SetDualMemoryType(MemoryType mt, MemoryType dual_mt);

  /// @brief Configure default memory types for host and device
  /// @param h_mt Default host memory type
  /// @param d_mt Default device memory type
  /// @note Should be called early in program initialization
  void Configure(const MemoryType h_mt, const MemoryType d_mt);

  /// @brief Destroy memory manager and free all tracked resources
  /// @warning All Memory objects should be deleted before calling this
  void Destroy();

  /// @brief Check if pointer is registered with memory manager
  /// @param h_ptr Host pointer to check
  /// @return true if pointer is tracked by MemoryManager
  bool IsKnown(const void* h_ptr) { return IsKnown_(h_ptr); }

  /// @brief Check if pointer is an alias (subview)
  /// @param h_ptr Host pointer to check
  /// @return true if pointer is an alias of another Memory object
  bool IsAlias(const void* h_ptr) { return IsAlias_(h_ptr); }

  /// @brief Verify that pointer is registered (debug check)
  /// @param h_ptr Host pointer to verify
  /// @note Throws error if pointer is not registered
  void RegisterCheck(void* h_ptr);

  /// @brief Print all registered pointers for debugging
  /// @param out Output stream (default: asc::mout)
  /// @return Number of registered pointers
  int PrintPtrs(std::ostream& out = asc::mout);

  /// @brief Print all alias relationships for debugging
  /// @param out Output stream (default: asc::mout)
  /// @return Number of aliases
  int PrintAliases(std::ostream& out = asc::mout);

  /// @brief Get configured default host memory type
  /// @return Default host memory type
  static MemoryType GetHostMemoryType() { return host_mem_type_; }

  /// @brief Get configured default device memory type
  /// @return Default device memory type
  static MemoryType GetDeviceMemoryType() { return device_mem_type_; }
};

/// @brief Global MemoryManager singleton instance
/// @note All Memory<T> objects interact with this global instance
/// @note Initialize with mm.Configure() if custom memory types are needed
///
/// @par Example:
/// @code
/// // Configure memory types at program start
/// asc::mm.Configure(asc::MemoryType::kHost64,
///                      asc::MemoryType::kDevice);
///
/// // Debug: print registered pointers
/// asc::mm.PrintPtrs(std::cout);
/// @endcode
extern ASC_EXPORT MemoryManager mm;

}  // namespace asc

#include "asc/core/memory_impl.h"

#endif  // ASC_MEMORY_H_
