// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/memory_impl.h
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#ifndef ASC_MEMORY_IMPL_H_
#define ASC_MEMORY_IMPL_H_

#include "asc/core/memory.h"

namespace asc {

template <typename T>
inline void Memory<T>::Reset() {
  h_ptr_ = nullptr;
  h_mt_ = MemoryManager::GetHostMemoryType();
  capacity_ = 0;
  flags_ = 0;
}

template <typename T>
inline void Memory<T>::Reset(MemoryType mt) {
  h_ptr_ = nullptr;
  capacity_ = 0;
  if (mt == MemoryType::kDefault) {
    h_mt_ = MemoryManager::GetHostMemoryType();
    flags_ = 0;
  } else if (IsHostMemory(mt)) {
    h_mt_ = mt;
    flags_ = 0;
  } else {
    ASC_VERIFY(IsDeviceMemory(mt),
                  "invalid memory type for reset: " << static_cast<int>(mt));
    h_mt_ = MemoryManager::GetDualMemoryType(mt);
    flags_ = kDeviceIntent;
  }
}

template <typename T>
inline void Memory<T>::New(int size) {
  capacity_ = size;
  flags_ = kOwnsHost | kValidHost;
  h_mt_ = MemoryManager::GetHostMemoryType();
  h_ptr_ = (h_mt_ == MemoryType::kHost)
               ? NewHost(size)
               : static_cast<T*>(MemoryManager::New_(nullptr, size * sizeof(T),
                                                     h_mt_, flags_));
}

template <typename T>
inline void Memory<T>::New(int size, MemoryType mt) {
  capacity_ = size;
  const size_t bytes = size * sizeof(T);
  const bool mt_host = mt == MemoryType::kHost;
  if (mt_host) {
    flags_ = kOwnsHost | kValidHost;
  }
  h_mt_ = IsHostMemory(mt) ? mt : MemoryManager::GetDualMemoryType(mt);
  T* h_tmp = (h_mt_ == MemoryType::kHost) ? NewHost(size) : nullptr;
  h_ptr_ = (mt_host)
               ? h_tmp
               : static_cast<T*>(MemoryManager::New_(h_tmp, bytes, mt, flags_));
}

template <typename T>
inline void Memory<T>::New(int size, MemoryType host_mt, MemoryType device_mt) {
  capacity_ = size;
  const size_t bytes = size * sizeof(T);
  this->h_mt_ = host_mt;
  T* h_tmp = (host_mt == MemoryType::kHost) ? NewHost(size) : nullptr;
  h_ptr_ = static_cast<T*>(MemoryManager::New_(h_tmp, bytes, host_mt, device_mt,
                                               kValidHost, flags_));
}

template <typename T>
inline void Memory<T>::Wrap(T* ptr, int size, bool own) {
  h_ptr_ = ptr;
  capacity_ = size;
  flags_ = (own ? kOwnsHost : 0U) | kValidHost;
  h_mt_ = MemoryManager::GetHostMemoryType();
  if (own && h_mt_ != MemoryType::kHost) {
    const size_t bytes = size * sizeof(T);
    MemoryManager::Register_(ptr, ptr, bytes, h_mt_, own, false, flags_);
  }
}

template <typename T>
inline void Memory<T>::Wrap(T* ptr, int size, MemoryType mt, bool own) {
  capacity_ = size;
  if (IsHostMemory(mt)) {
    h_mt_ = mt;
    h_ptr_ = ptr;
    if (mt == MemoryType::kHost || !own) {
      flags_ = (own ? kOwnsHost : 0U) | kValidHost;
      return;
    }
  } else {
    h_mt_ = MemoryManager::GetDualMemoryType(mt);
    h_ptr_ = (h_mt_ == MemoryType::kHost) ? NewHost(size) : nullptr;
  }
  flags_ = 0;
  h_ptr_ = static_cast<T*>(MemoryManager::Register_(
      ptr, h_ptr_, size * sizeof(T), mt, own, false, flags_));
}

template <typename T>
inline void Memory<T>::Wrap(T* h_ptr, T* d_ptr, int size, MemoryType h_mt,
                            bool own, bool valid_host, bool valid_device) {
  h_mt_ = h_mt;
  flags_ = 0;
  h_ptr_ = h_ptr;
  capacity_ = size;
  ASC_VERIFY(IsHostMemory(h_mt), "");
  ASC_VERIFY(valid_host || valid_device, "");
  const size_t bytes = size * sizeof(T);
  const MemoryType d_mt = MemoryManager::GetDualMemoryType(h_mt);
  MemoryManager::Register2_(
      h_ptr, d_ptr, bytes, h_mt, d_mt, own, false, flags_,
      valid_host * kValidHost | valid_device * kValidDevice);
}

template <typename T>
inline void Memory<T>::MakeAlias(const Memory& base, int offset, int size) {
  ASC_VERIFY(0 <= offset, "invalid offset = " << offset);
  ASC_VERIFY(0 <= size, "invalid size = " << size);
  ASC_VERIFY(
      offset + size <= base.capacity_,
      "invalid offset + size = " << offset + size
                                 << " > base capacity = " << base.capacity_);
  capacity_ = size;
  h_mt_ = base.h_mt_;
  h_ptr_ = base.h_ptr_ + offset;
  if (!(base.flags_ & kRegistered)) {
    if (MemoryManager::Exists()) {
      MemoryManager::Register_(base.h_ptr_, nullptr, base.capacity_ * sizeof(T),
                               base.h_mt_, base.flags_ & kOwnsHost,
                               base.flags_ & kAlias, base.flags_);
    } else {
      flags_ = (base.flags_ | kAlias) & ~(kOwnsHost | kOwnsDevice);
      return;
    }
  }
  const size_t s_bytes = size * sizeof(T);
  const size_t o_bytes = offset * sizeof(T);
  MemoryManager::Alias_(base.h_ptr_, o_bytes, s_bytes, base.flags_, flags_);
}

template <typename T>
inline void Memory<T>::SetDeviceMemoryType(MemoryType d_mt) {
  if (!IsDeviceMemory(d_mt)) {
    return;
  }
  if (!(flags_ & kRegistered)) {
    MemoryManager::Register_(h_ptr_, nullptr, capacity_ * sizeof(T), h_mt_,
                             flags_ & kOwnsHost, flags_ & kAlias, flags_);
  }
  MemoryManager::SetDeviceMemoryType_(h_ptr_, flags_, d_mt);
}

template <typename T>
inline void Memory<T>::Delete() {
  const bool registered = flags_ & kRegistered;
  const bool mt_host = h_mt_ == MemoryType::kHost;
  const bool std_delete = !registered && mt_host;

  if (!std_delete) {
    MemoryManager::Delete_(static_cast<void*>(h_ptr_), h_mt_, flags_);
  }

  if (mt_host) {
    if (flags_ & kOwnsHost) {
      delete[] h_ptr_;
    }
  }
  Reset(h_mt_);
}

template <typename T>
inline void Memory<T>::DeleteDevice(bool copy_to_host) {
  if (flags_ & kRegistered) {
    if (copy_to_host) {
      Read(MemoryClass::kHost, capacity_);
    }
    MemoryManager::DeleteDevice_(static_cast<void*>(h_ptr_), flags_);
  }
}

template <typename T>
inline T& Memory<T>::operator[](int idx) {
  ASC_VERIFY((flags_ & kValidHost) && !(flags_ & kValidDevice),
                "invalid host pointer access");
  return h_ptr_[idx];
}

template <typename T>
inline const T& Memory<T>::operator[](int idx) const {
  ASC_VERIFY((flags_ & kValidHost), "invalid host pointer access");
  return h_ptr_[idx];
}

template <typename T>
inline Memory<T>::operator T*() {
  ASC_VERIFY(
      IsEmpty() || ((flags_ & kValidHost) &&
                    (std::is_const<T>::value || !(flags_ & kValidDevice))),
      "invalid host pointer access");
  return h_ptr_;
}

template <typename T>
inline Memory<T>::operator const T*() const {
  ASC_VERIFY(IsEmpty() || (flags_ & kValidHost),
                "invalid host pointer access");
  return h_ptr_;
}

template <typename T>
template <typename U>
inline Memory<T>::operator U*() {
  ASC_VERIFY(
      IsEmpty() || ((flags_ & kValidHost) &&
                    (std::is_const<U>::value || !(flags_ & kValidDevice))),
      "invalid host pointer access");
  return reinterpret_cast<U*>(h_ptr_);
}

template <typename T>
template <typename U>
inline Memory<T>::operator const U*() const {
  ASC_VERIFY(IsEmpty() || (flags_ & kValidHost),
                "invalid host pointer access");
  return reinterpret_cast<U*>(h_ptr_);
}

template <typename T>
inline T* Memory<T>::ReadWrite(MemoryClass mc, int size) {
  const size_t bytes = size * sizeof(T);
  if (!(flags_ & kRegistered)) {
    if (mc == MemoryClass::kHost) {
      return h_ptr_;
    }
    MemoryManager::Register_(h_ptr_, nullptr, capacity_ * sizeof(T), h_mt_,
                             flags_ & kOwnsHost, flags_ & kAlias, flags_);
  }
  return static_cast<T*>(
      MemoryManager::ReadWrite_(h_ptr_, h_mt_, mc, bytes, flags_));
}

template <typename T>
inline const T* Memory<T>::Read(MemoryClass mc, int size) const {
  const size_t bytes = size * sizeof(T);
  if (!(flags_ & kRegistered)) {
    if (mc == MemoryClass::kHost) {
      return h_ptr_;
    }
    MemoryManager::Register_(h_ptr_, nullptr, capacity_ * sizeof(T), h_mt_,
                             flags_ & kOwnsHost, flags_ & kAlias, flags_);
  }
  return (const T*)MemoryManager::Read_(h_ptr_, h_mt_, mc, bytes, flags_);
}

template <typename T>
inline T* Memory<T>::Write(MemoryClass mc, int size) {
  const size_t bytes = size * sizeof(T);
  if (!(flags_ & kRegistered)) {
    if (mc == MemoryClass::kHost) {
      return h_ptr_;
    }
    MemoryManager::Register_(h_ptr_, nullptr, capacity_ * sizeof(T), h_mt_,
                             flags_ & kOwnsHost, flags_ & kAlias, flags_);
  }
  return static_cast<T*>(
      MemoryManager::Write_(h_ptr_, h_mt_, mc, bytes, flags_));
}

template <typename T>
inline void Memory<T>::Sync(const Memory& other) const {
  if (!(flags_ & kRegistered) && (other.flags_ & kRegistered)) {
    ASC_VERIFY(
        h_ptr_ == other.h_ptr_ && (flags_ & kAlias) == (other.flags_ & kAlias),
        "invalid input");
    flags_ = (flags_ | kRegistered) & ~(kOwnsDevice | kOwnsInternal);
  }
  flags_ = (flags_ & ~(kValidHost | kValidDevice)) |
           (other.flags_ & (kValidHost | kValidDevice));
}

template <typename T>
inline void Memory<T>::SyncAlias(const Memory& base, int alias_size) const {
  ASC_VERIFY(!(flags_ & kRegistered) || (base.flags_ & kRegistered),
                "invalid base state");
  if (!(base.flags_ & kRegistered)) {
    return;
  }
  MemoryManager::SyncAlias_(base.h_ptr_, h_ptr_, alias_size * sizeof(T),
                            base.flags_, flags_);
}

template <typename T>
inline MemoryType Memory<T>::GetMemoryType() const {
  if (h_ptr_ == nullptr) {
    if (flags_ & kDeviceIntent) {
      return MemoryManager::GetDualMemoryType(h_mt_);
    }
    return h_mt_;
  }
  if (!(flags_ & kValidDevice)) {
    return h_mt_;
  }
  return MemoryManager::GetDeviceMemoryType_(h_ptr_, flags_ & kAlias);
}

template <typename T>
inline MemoryType Memory<T>::GetDeviceMemoryType() const {
  if (!(flags_ & kRegistered)) {
    return MemoryType::kHost;
  }
  return MemoryManager::GetDeviceMemoryType_(h_ptr_, flags_ & kAlias);
}

template <typename T>
inline bool Memory<T>::HostIsValid() const {
  return flags_ & kValidHost ? true : false;
}

template <typename T>
inline bool Memory<T>::DeviceIsValid() const {
  return flags_ & kValidDevice ? true : false;
}

template <typename T>
inline void Memory<T>::CopyFrom(const Memory& src, int size) {
  ASC_VERIFY(src.capacity_ >= size && capacity_ >= size, "Incorrect size");
  if (size <= 0) {
    return;
  }
  if (!(flags_ & kRegistered) && !(src.flags_ & kRegistered)) {
    if (h_ptr_ != src.h_ptr_) {
      ASC_VERIFY(h_ptr_ + size <= src.h_ptr_ || src.h_ptr_ + size <= h_ptr_,
                    "data overlaps!");
      std::memcpy(h_ptr_, src, size * sizeof(T));
    }
  } else {
    MemoryManager::Copy_(h_ptr_, src.h_ptr_, size * sizeof(T), src.flags_,
                         flags_);
  }
}

template <typename T>
inline void Memory<T>::CopyFromHost(const T* src, int size) {
  ASC_VERIFY(capacity_ >= size, "Incorrect size");
  if (size <= 0) {
    return;
  }
  if (!(flags_ & kRegistered)) {
    if (h_ptr_ != src) {
      ASC_VERIFY(h_ptr_ + size <= src || src + size <= h_ptr_,
                    "data overlaps!");
      std::memcpy(h_ptr_, src, size * sizeof(T));
    }
  } else {
    MemoryManager::CopyFromHost_(h_ptr_, src, size * sizeof(T), flags_);
  }
}

template <typename T>
inline void Memory<T>::CopyTo(Memory& dest, int size) const {
  dest.CopyFrom(*this, size);
}

template <typename T>
inline void Memory<T>::CopyToHost(T* dest, int size) const {
  ASC_VERIFY(capacity_ >= size, "Incorrect size");
  if (size <= 0) {
    return;
  }
  if (!(flags_ & kRegistered)) {
    if (h_ptr_ != dest) {
      ASC_VERIFY(h_ptr_ + size <= dest || dest + size <= h_ptr_,
                    "data overlaps!");
      std::memcpy(dest, h_ptr_, size * sizeof(T));
    }
  } else {
    MemoryManager::CopyToHost_(dest, h_ptr_, size * sizeof(T), flags_);
  }
}

/// @brief Print memory validity/ownership flags for debugging
/// @param flags Flags bitmask to print
/// @note Outputs human-readable flag names (e.g., "kValidHost|kOwnsDevice")
extern void MemoryPrintFlags(unsigned flags);

template <typename T>
inline void Memory<T>::PrintFlags() const {
  MemoryPrintFlags(flags_);
}

template <typename T>
inline int Memory<T>::CompareHostAndDevice(int size) const {
  if (!(flags_ & kValidHost) || !(flags_ & kValidDevice)) {
    return 0;
  }
  return MemoryManager::CompareHostAndDevice_(h_ptr_, size * sizeof(T), flags_);
}

}  // namespace asc

#endif
