// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/device.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_DEVICE_H_
#define ASC_DEVICE_H_

/// @file device.h
/// @brief Device configuration and backend management for heterogeneous
/// computing
///
/// This module provides the Device class for configuring and managing
/// computational backends (CPU, OpenMP, CUDA) in ASC. It serves as the
/// central configuration point for heterogeneous computing, managing:
/// - Backend selection (CPU, OpenMP, CUDA)
/// - Device memory type configuration
/// - GPU device selection and properties
/// - Integration with the Memory system
///
/// @par Supported Backends:
/// - Backend::kCpu: Sequential CPU execution (always available)
/// - Backend::kOmp: OpenMP parallel CPU execution (requires ASC_USE_OPENMP)
/// - Backend::kCuda: CUDA GPU execution (requires ASC_USE_CUDA)

#include <string>

#include "asc/core/globals.h"
#include "asc/core/memory.h"

namespace asc {

/// @brief Backend type flags for computational execution
///
/// Bit flags representing available execution backends. Multiple backends
/// can be combined using bitwise OR. The Device class tracks which backends
/// are enabled and selects the appropriate one for kernel execution.
struct Backend {
  enum : unsigned {
    kCpu = 1 << 0,  ///< [host] Sequential CPU backend (always available)
    kOmp = 1 << 1,  ///< [host] OpenMP parallel backend (ASC_USE_OPENMP)
    kCuda = 1 << 2  ///< [device] CUDA GPU backend (ASC_USE_CUDA)
  };

  /// @brief Number of distinct backends
  static constexpr int kNBackends = 3;
};

/// @brief Device configuration and management singleton
///
/// Device is a singleton class that manages the computational backend
/// configuration for the entire ASC library. It controls:
/// - Which execution backend is active (CPU, OpenMP, CUDA)
/// - Which GPU device to use (for CUDA backend)
/// - Memory type defaults for host and device allocations
/// - Integration between Memory and device capabilities
///
/// @par Singleton Pattern:
/// Device uses a singleton pattern - there is one global instance accessed
/// via static methods. Configuration persists for the lifetime of the program.
///
/// @par Thread Safety:
/// Device configuration is NOT thread-safe. Configure before spawning threads.
class Device {
 public:
  Device();

  explicit Device(const std::string& device, const int device_id = 0) {
    Configure(device, device_id);
  }

  ~Device();

  void Configure(const std::string& device, const int device_id = 0);

  static void SetMemoryTypes(MemoryType h_mt, MemoryType d_mt);

  void Print(std::ostream& os = asc::mout);

  static inline bool IsConfigured() { return Get().ngpu_ >= 0; }

  static inline bool IsAvailable() { return Get().ngpu_ > 0; }

  static inline bool IsEnabled() { return Get().backends_ & ~(Backend::kCpu); }

  static inline bool IsDisabled() { return !IsEnabled(); }

  static inline int GetId() { return Get().dev_; }

  static int GetDeviceCount();

  static inline bool Allows(unsigned b_mask) {
    return Get().backends_ & b_mask;
  }

  static inline MemoryType GetHostMemoryType() { return Get().host_mem_type_; }

  static inline MemoryClass GetHostMemoryClass() {
    return Get().host_mem_class_;
  }

  static inline MemoryType GetDeviceMemoryType() {
    return Get().device_mem_type_;
  }

  static inline MemoryType GetMemoryType() { return Get().device_mem_type_; }

  static inline MemoryClass GetDeviceMemoryClass() {
    return Get().device_mem_class_;
  }

  static inline MemoryClass GetMemoryClass() { return Get().device_mem_class_; }

  static MemoryType QueryMemoryType(void* ptr);

  static int NumMultiprocessors(int device_id);

  static int NumMultiprocessors();

  static int WarpSize(int device_id);

  static int WarpSize();

  static void DeviceMem(size_t* free, size_t* total);

 private:
  friend class MemoryManager;

  static bool device_env, mem_host_env, mem_device_env, mem_types_set;
  static ASC_EXPORT Device device_singleton;

  int dev_ = 0;    //< Device ID of the configured device.
  int ngpu_ = -1;  //< Number of detected devices; -1: not initialized.

  unsigned backends_ = Backend::kCpu;

  bool destroy_mm_ = false;

  MemoryType host_mem_type_ = MemoryType::kHost;
  MemoryClass host_mem_class_ = MemoryClass::kHost;

  MemoryType device_mem_type_ = MemoryType::kHost;
  MemoryClass device_mem_class_ = MemoryClass::kHost;

  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  static Device& Get() { return device_singleton; }

  void Setup(const std::string& device_option, const int device_id);

  void UpdateMemoryTypeAndClass(const std::string& device_option);

  void MarkBackend(unsigned b) { backends_ |= b; }
};

template <typename T>
inline MemoryClass GetMemoryClass(const Memory<T>& mem, bool on_dev) {
  if (!on_dev) {
    return Device::GetHostMemoryClass();
  } else {
    mem.UseDevice(true);
    return Device::GetDeviceMemoryClass();
  }
}

template <typename T>
inline const T* Read(const Memory<T>& mem, int size, bool on_dev = true) {
  return mem.Read(GetMemoryClass(mem, on_dev), size);
}

template <typename T>
inline const T* HostRead(const Memory<T>& mem, int size) {
  return asc::Read(mem, size, false);
}

template <typename T>
inline T* Write(Memory<T>& mem, int size, bool on_dev = true) {
  return mem.Write(GetMemoryClass(mem, on_dev), size);
}

template <typename T>
inline T* HostWrite(Memory<T>& mem, int size) {
  return asc::Write(mem, size, false);
}

template <typename T>
inline T* ReadWrite(Memory<T>& mem, int size, bool on_dev = true) {
  return mem.ReadWrite(GetMemoryClass(mem, on_dev), size);
}

template <typename T>
inline T* HostReadWrite(Memory<T>& mem, int size) {
  return asc::ReadWrite(mem, size, false);
}

}  // namespace asc

#endif  // ASC_DEVICE_H_
