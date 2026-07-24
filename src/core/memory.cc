// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/memory.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <list>
#include <cstring>  // std::memcpy, std::memcmp
#include <unordered_map>
#include <algorithm>  // std::max
#include <cstdint>

#include "asc/core/memory.h"
#include "asc/core/cuda.h"

// Uncomment to try _WIN32 platform
// #define _WIN32
// #define _aligned_malloc(s,a) malloc(s)

#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#define ASC_MEMALIGN(p, a, s) posix_memalign(p, a, s)
#define ASC_ALIGNED_FREE free
#else
#define ASC_MEMALIGN(p, a, s) \
  (((*(p)) = _aligned_malloc((s), (a))), *(p) ? 0 : errno)
#define ASC_ALIGNED_FREE _aligned_free
#endif

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

namespace asc {

MemoryType GetMemoryType(MemoryClass mc) {
  switch (mc) {
    case MemoryClass::kHost:
      return mm.GetHostMemoryType();
    case MemoryClass::kHost32:
      return MemoryType::kHost32;
    case MemoryClass::kHost64:
      return MemoryType::kHost64;
    case MemoryClass::kDevice:
      return mm.GetDeviceMemoryType();
    case MemoryClass::kManaged:
      return MemoryType::kManaged;
  }
  ASC_VERIFY(false, "");
  return MemoryType::kHost;
}

bool MemoryClassContainsType(MemoryClass mc, MemoryType mt) {
  switch (mc) {
    case MemoryClass::kHost:
      return IsHostMemory(mt);
    case MemoryClass::kHost32:
      return (mt == MemoryType::kHost32 || mt == MemoryType::kHost64);
    case MemoryClass::kHost64:
      return (mt == MemoryType::kHost64);
    case MemoryClass::kDevice:
      return IsDeviceMemory(mt);
    case MemoryClass::kManaged:
      return (mt == MemoryType::kManaged);
  }
  ASC_ABORT("invalid MemoryClass");
  return false;
}

static void ASC_VERIFY_TYPES(const MemoryType h_mt, const MemoryType d_mt) {
  ASC_VERIFY(IsHostMemory(h_mt), "h_mt = " << (int)h_mt);
  ASC_VERIFY(IsDeviceMemory(d_mt) || d_mt == MemoryType::kDefault,
             "d_mt = " << (int)d_mt);
}

MemoryClass operator*(MemoryClass mc1, MemoryClass mc2) {
  //           | kHost     kHost32   kHost64   kDevice   kManaged
  // ----------+-------------------------------------------------
  //  kHost    | kHost     kHost32   kHost64   kDevice   kManaged
  //  kHost32  | kHost32   kHost32   kHost64   kDevice   kManaged
  //  kHost64  | kHost64   kHost64   kHost64   kDevice   kManaged
  //  kDevice  | kDevice   kDevice   kDevice   kDevice   kManaged
  //  kManaged | kManaged  kManaged  kManaged  kManaged  kManaged

  // Using the enumeration ordering:
  //    kHost < kHost32 < kHost64 < kDevice < kManaged,
  // the above table is simply: a*b = max(a,b).

  return std::max(mc1, mc2);
}

// Instantiate Memory<T>::PrintFlags for T = int and T = real_t.
template void Memory<int>::PrintFlags() const;
template void Memory<real_t>::PrintFlags() const;

// Instantiate Memory<T>::CompareHostAndDevice for T = int and T = real_t.
template int Memory<int>::CompareHostAndDevice(int size) const;
template int Memory<real_t>::CompareHostAndDevice(int size) const;

namespace internal {

// Memory class that holds:
//   - the host and the device pointer
//   - the size in bytes of this memory region
//   - the host and device type of this memory region
struct Memory {
  void *const h_ptr;
  void *d_ptr;
  const size_t bytes;
  const MemoryType h_mt;
  MemoryType d_mt;
  mutable bool h_rw, d_rw;
  Memory(void *p, size_t b, MemoryType h, MemoryType d)
      : h_ptr(p),
        d_ptr(nullptr),
        bytes(b),
        h_mt(h),
        d_mt(d),
        h_rw(true),
        d_rw(true) {}
};

// Alias class that holds the base memory region and the offset
struct Alias {
  Memory *mem;
  size_t offset;
  size_t counter;
  // 'h_mt' is already stored in 'mem', however, we use this field for type
  // checking since the alias may be dangling, i.e. 'mem' may be invalid.
  MemoryType h_mt;
};

// Maps for the Memory and the Alias classes
typedef std::unordered_map<const void *, Memory> MemoryMap;
typedef std::unordered_map<const void *, Alias> AliasMap;

struct Maps {
  MemoryMap memories;
  AliasMap aliases;
};

}  // namespace internal

static internal::Maps *maps;

namespace internal {

// The host memory space base abstract class
class HostMemorySpace {
 public:
  virtual ~HostMemorySpace() {}
  virtual void Alloc(void **ptr, size_t bytes) { *ptr = std::malloc(bytes); }
  virtual void Dealloc(void *ptr) { std::free(ptr); }
  virtual void Protect(const Memory &, size_t) {}
  virtual void Unprotect(const Memory &, size_t) {}
  virtual void AliasProtect(const void *, size_t) {}
  virtual void AliasUnprotect(const void *, size_t) {}
};

// The device memory space base abstract class
class DeviceMemorySpace {
 public:
  virtual ~DeviceMemorySpace() {}
  virtual void Alloc(Memory &base) { base.d_ptr = std::malloc(base.bytes); }
  virtual void Dealloc(Memory &base) { std::free(base.d_ptr); }
  virtual void Protect(const Memory &) {}
  virtual void Unprotect(const Memory &) {}
  virtual void AliasProtect(const void *, size_t) {}
  virtual void AliasUnprotect(const void *, size_t) {}
  virtual void *HtoD(void *dst, const void *src, size_t bytes) {
    return std::memcpy(dst, src, bytes);
  }
  virtual void *DtoD(void *dst, const void *src, size_t bytes) {
    return std::memcpy(dst, src, bytes);
  }
  virtual void *DtoH(void *dst, const void *src, size_t bytes) {
    return std::memcpy(dst, src, bytes);
  }
};

// The default std:: host memory space
class StdHostMemorySpace : public HostMemorySpace {};

// The No host memory space
struct NoHostMemorySpace : public HostMemorySpace {
  void Alloc(void **, const size_t) override { Error("! Host Alloc error"); }
};

// The aligned 32 host memory space
class Aligned32HostMemorySpace : public HostMemorySpace {
 public:
  Aligned32HostMemorySpace() : HostMemorySpace() {}
  void Alloc(void **ptr, size_t bytes) override {
    static_cast<void>(ptr);
    static_cast<void>(bytes);
    if (ASC_MEMALIGN(ptr, 32, bytes) != 0) {
      throw ::std::bad_alloc();
    }
  }
  void Dealloc(void *ptr) override { ASC_ALIGNED_FREE(ptr); }
};

// The aligned 64 host memory space
class Aligned64HostMemorySpace : public HostMemorySpace {
 public:
  Aligned64HostMemorySpace() : HostMemorySpace() {}
  void Alloc(void **ptr, size_t bytes) override {
    if (ASC_MEMALIGN(ptr, 64, bytes) != 0) {
      throw ::std::bad_alloc();
    }
  }
  void Dealloc(void *ptr) override { ASC_ALIGNED_FREE(ptr); }
};

#ifndef _WIN32
static uintptr_t pagesize = 0;
static uintptr_t pagemask = 0;

static struct sigaction old_segv_action;
static struct sigaction old_bus_action;

// Returns the restricted base address of the DEBUG segment
inline const void *MmuAddrR(const void *ptr) {
  const uintptr_t addr = (uintptr_t)ptr;
  return (addr & pagemask)
             ? reinterpret_cast<void *>(((addr + pagesize) & ~pagemask))
             : ptr;
}

// Returns the prolongated base address of the MMU segment
inline const void *MmuAddrP(const void *ptr) {
  const uintptr_t addr = (uintptr_t)ptr;
  return reinterpret_cast<void *>(addr & ~pagemask);
}

// Compute the restricted length for the MMU segment
inline uintptr_t MmuLengthR(const void *ptr, const size_t bytes) {
  // a ---->A:|    |:B<---- b
  const uintptr_t a = (uintptr_t)ptr;
  const uintptr_t A = (uintptr_t)MmuAddrR(ptr);
  ASC_ASSERT(a <= A, "");
  const uintptr_t b = a + bytes;
  const uintptr_t B = b & ~pagemask;
  ASC_ASSERT(B <= b, "");
  const uintptr_t length = B > A ? B - A : 0;
  ASC_ASSERT(length % pagesize == 0, "");
  return length;
}

// Compute the prolongated length for the MMU segment
inline uintptr_t MmuLengthP(const void *ptr, const size_t bytes) {
  // |:A<----a |    |  b---->B:|
  const uintptr_t a = (uintptr_t)ptr;
  const uintptr_t A = (uintptr_t)MmuAddrP(ptr);
  ASC_ASSERT(A <= a, "");
  const uintptr_t b = a + bytes;
  const uintptr_t B = b & pagemask ? (b + pagesize) & ~pagemask : b;
  ASC_ASSERT(b <= B, "");
  ASC_ASSERT(B >= A, "");
  const uintptr_t length = B - A;
  ASC_ASSERT(length % pagesize == 0, "");
  return length;
}

// The protected access error, used for the host
static void MmuError(int sig, siginfo_t *si, void *context) {
  constexpr size_t kBufSize = 64;
  fflush(0);
  char str[kBufSize];
  const void *ptr = si->si_addr;
  snprintf(str, kBufSize, "Error while accessing address %p!", ptr);
  asc::mout << std::endl << "An illegal memory access was made!";
  asc::mout << std::endl
            << "Caught signal " << sig << ", code " << si->si_code << " at "
            << ptr << std::endl;
  // chain to previous handler
  struct sigaction *old_action =
      (sig == SIGSEGV) ? &old_segv_action : &old_bus_action;
  if (old_action->sa_flags & SA_SIGINFO && old_action->sa_sigaction) {
    // old action uses three argument handler.
    old_action->sa_sigaction(sig, si, context);
  } else if (old_action->sa_handler == SIG_DFL) {
    // reinstall and raise the default handler.
    sigaction(sig, old_action, NULL);
    raise(sig);
  }
  ASC_ABORT(str);
}

// MMU initialization, setting SIGBUS & SIGSEGV signals to MmuError
static void MmuInit() {
  if (pagesize > 0) {
    return;
  }
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = MmuError;
  if (sigaction(SIGBUS, &sa, &old_bus_action) == -1) {
    Error("SIGBUS");
  }
  if (sigaction(SIGSEGV, &sa, &old_segv_action) == -1) {
    Error("SIGSEGV");
  }
  pagesize = (uintptr_t)sysconf(_SC_PAGE_SIZE);
  ASC_ASSERT(pagesize > 0, "pagesize must not be less than 1");
  pagemask = pagesize - 1;
}

// MMU allocation, through ::mmap
inline void MmuAlloc(void **ptr, const size_t bytes) {
  const size_t length = bytes == 0 ? 8 : bytes;
  const int prot = PROT_READ | PROT_WRITE;
  const int flags = MAP_ANONYMOUS | MAP_PRIVATE;
  *ptr = ::mmap(NULL, length, prot, flags, -1, 0);
  if (*ptr == MAP_FAILED) {
    throw ::std::bad_alloc();
  }
}

// MMU deallocation, through ::munmap
inline void MmuDealloc(void *ptr, const size_t bytes) {
  const size_t length = bytes == 0 ? 8 : bytes;
  if (::munmap(ptr, length) == -1) {
    Error("Dealloc error!");
  }
}

// MMU protection, through ::mprotect with no read/write accesses
inline void MmuProtect(const void *ptr, const size_t bytes) {
  static const bool mmu_protect_error = std::getenv("ASC_MMU_PROTECT_ERROR");
  if (!::mprotect(const_cast<void *>(ptr), bytes, PROT_NONE)) {
    return;
  }
  if (mmu_protect_error) {
    Error("MMU protection (NONE) error");
  }
}

// MMU un-protection, through ::mprotect with read/write accesses
inline void MmuAllow(const void *ptr, const size_t bytes) {
  const int RW = PROT_READ | PROT_WRITE;
  static const bool mmu_protect_error = std::getenv("ASC_MMU_PROTECT_ERROR");
  if (!::mprotect(const_cast<void *>(ptr), bytes, RW)) {
    return;
  }
  if (mmu_protect_error) {
    Error("MMU protection (R/W) error");
  }
}
#else
inline void MmuInit() {}
inline void MmuAlloc(void **ptr, const size_t bytes) {
  *ptr = std::malloc(bytes);
}
inline void MmuDealloc(void *ptr, const size_t) { std::free(ptr); }
inline void MmuProtect(const void *, const size_t) {}
inline void MmuAllow(const void *, const size_t) {}
inline const void *MmuAddrR(const void *a) { return a; }
inline const void *MmuAddrP(const void *a) { return a; }
inline uintptr_t MmuLengthR(const void *, const size_t) { return 0; }
inline uintptr_t MmuLengthP(const void *, const size_t) { return 0; }
#endif

// The MMU host memory space
class MmuHostMemorySpace : public HostMemorySpace {
 public:
  MmuHostMemorySpace() : HostMemorySpace() { MmuInit(); }
  void Alloc(void **ptr, size_t bytes) override { MmuAlloc(ptr, bytes); }
  void Dealloc(void *ptr) override {
    static_cast<void>(ptr);
    MmuDealloc(ptr, maps->memories.at(ptr).bytes);
  }
  void Protect(const Memory &mem, size_t bytes) override {
    if (mem.h_rw) {
      mem.h_rw = false;
      MmuProtect(mem.h_ptr, bytes);
    }
  }
  void Unprotect(const Memory &mem, size_t bytes) override {
    if (!mem.h_rw) {
      mem.h_rw = true;
      MmuAllow(mem.h_ptr, bytes);
    }
  }
  // Aliases need to be restricted during protection
  void AliasProtect(const void *ptr, size_t bytes) override {
    MmuProtect(MmuAddrR(ptr), MmuLengthR(ptr, bytes));
  }
  // Aliases need to be prolongated for un-protection
  void AliasUnprotect(const void *ptr, size_t bytes) override {
    MmuAllow(MmuAddrP(ptr), MmuLengthP(ptr, bytes));
  }
};

// The UVM host memory space
class UvmHostMemorySpace : public HostMemorySpace {
 public:
  UvmHostMemorySpace() : HostMemorySpace() {}

  void Alloc(void **ptr, size_t bytes) override {
    static_cast<void>(ptr);
    static_cast<void>(bytes);
#ifdef ASC_USE_CUDA
    CuMallocManaged(ptr, bytes == 0 ? 8 : bytes);
#endif
  }

  void Dealloc(void *ptr) override {
    static_cast<void>(ptr);
#ifdef ASC_USE_CUDA
    CuMemFree(ptr);
#endif
#ifdef ASC_USE_HIP
    HipMemFree(ptr);
#endif
  }
};

// The 'No' device memory space
class NoDeviceMemorySpace : public DeviceMemorySpace {
 public:
  void Alloc(internal::Memory &) override { Error("! Device Alloc"); }
  void Dealloc(Memory &) override { Error("! Device Dealloc"); }
  void *HtoD(void *, const void *, size_t) override {
    Error("!HtoD");
    return nullptr;
  }
  void *DtoD(void *, const void *, size_t) override {
    Error("!DtoD");
    return nullptr;
  }
  void *DtoH(void *, const void *, size_t) override {
    Error("!DtoH");
    return nullptr;
  }
};

// The std:: device memory space, used with the 'debug' device
class StdDeviceMemorySpace : public DeviceMemorySpace {};

// The CUDA device memory space
class CudaDeviceMemorySpace : public DeviceMemorySpace {
 public:
  CudaDeviceMemorySpace() : DeviceMemorySpace() {}
  void Alloc(Memory &base) override { CuMemAlloc(&base.d_ptr, base.bytes); }
  void Dealloc(Memory &base) override { CuMemFree(base.d_ptr); }
  void *HtoD(void *dst, const void *src, size_t bytes) override {
    return CuMemcpyHtoD(dst, src, bytes);
  }
  void *DtoD(void *dst, const void *src, size_t bytes) override {
    return CuMemcpyDtoD(dst, src, bytes);
  }
  void *DtoH(void *dst, const void *src, size_t bytes) override {
    return CuMemcpyDtoH(dst, src, bytes);
  }
};

// The UVM device memory space.
class UvmCudaMemorySpace : public DeviceMemorySpace {
 public:
  void Alloc(Memory &base) override { base.d_ptr = base.h_ptr; }
  void Dealloc(Memory &) override {}
  void *HtoD(void *dst, const void *src, size_t bytes) override {
    if (dst == src) {
      ASC_STREAM_SYNC;
      return dst;
    }
    return CuMemcpyHtoD(dst, src, bytes);
  }
  void *DtoD(void *dst, const void *src, size_t bytes) override {
    return CuMemcpyDtoD(dst, src, bytes);
  }
  void *DtoH(void *dst, const void *src, size_t bytes) override {
    if (dst == src) {
      ASC_STREAM_SYNC;
      return dst;
    }
    return CuMemcpyDtoH(dst, src, bytes);
  }
};

// The MMU device memory space
class MmuDeviceMemorySpace : public DeviceMemorySpace {
 public:
  MmuDeviceMemorySpace() : DeviceMemorySpace() {}
  void Alloc(Memory &m) override { MmuAlloc(&m.d_ptr, m.bytes); }
  void Dealloc(Memory &m) override { MmuDealloc(m.d_ptr, m.bytes); }
  void Protect(const Memory &m) override {
    if (m.d_rw) {
      m.d_rw = false;
      MmuProtect(m.d_ptr, m.bytes);
    }
  }
  void Unprotect(const Memory &m) override {
    if (!m.d_rw) {
      m.d_rw = true;
      MmuAllow(m.d_ptr, m.bytes);
    }
  }
  // Aliases need to be restricted during protection
  void AliasProtect(const void *ptr, size_t bytes) override {
    MmuProtect(MmuAddrR(ptr), MmuLengthR(ptr, bytes));
  }
  // Aliases need to be prolongated for un-protection
  void AliasUnprotect(const void *ptr, size_t bytes) override {
    MmuAllow(MmuAddrP(ptr), MmuLengthP(ptr, bytes));
  }
  void *HtoD(void *dst, const void *src, size_t bytes) override {
    return std::memcpy(dst, src, bytes);
  }
  void *DtoD(void *dst, const void *src, size_t bytes) override {
    return std::memcpy(dst, src, bytes);
  }
  void *DtoH(void *dst, const void *src, size_t bytes) override {
    return std::memcpy(dst, src, bytes);
  }
};

// Memory space controller class
class Ctrl {
  typedef MemoryType MT;

 public:
  HostMemorySpace *host[kHostMemoryTypeSize];
  DeviceMemorySpace *device[kDeviceMemoryTypeSize];

 public:
  Ctrl() : host{nullptr}, device{nullptr} {}

  void Configure() {
    if (host[kHostMemoryType]) {
      Error("Memory backends have already been configured!");
    }

    // Filling the host memory backends
    // kHost, kHost32 & kHost64 are always ready
    // ASC_USE_UMPIRE will set either [No/Umpire] HostMemorySpace
    host[static_cast<int>(MT::kHost)] = new StdHostMemorySpace();
    host[static_cast<int>(MT::kHost32)] = new Aligned32HostMemorySpace();
    host[static_cast<int>(MT::kHost64)] = new Aligned64HostMemorySpace();
    host[static_cast<int>(MT::kManaged)] = new UvmHostMemorySpace();

    // Filling the device memory backends, shifting with the device size
    constexpr int shift = kDeviceMemoryType;
#if defined(ASC_USE_CUDA)
    device[static_cast<int>(MT::kManaged) - shift] = new UvmCudaMemorySpace();
#endif
    device[static_cast<int>(MT::kDevice) - shift] = nullptr;
  }

  HostMemorySpace *Host(const MemoryType mt) {
    const int mt_i = static_cast<int>(mt);
    // Delayed host controllers initialization
    if (!host[mt_i]) {
      host[mt_i] = NewHostCtrl(mt);
    }
    ASC_ASSERT(host[mt_i], "Host memory controller is not configured!");
    return host[mt_i];
  }

  DeviceMemorySpace *Device(const MemoryType mt) {
    const int mt_i = static_cast<int>(mt) - kDeviceMemoryType;
    ASC_ASSERT(mt_i >= 0, "");
    // Lazy device controller initializations
    if (!device[mt_i]) {
      device[mt_i] = NewDeviceCtrl(mt);
    }
    ASC_ASSERT(device[mt_i], "Memory manager has not been configured!");
    return device[mt_i];
  }

  ~Ctrl() {
    constexpr int mt_h = kHostMemoryType;
    constexpr int mt_d = kDeviceMemoryType;
    for (int mt = mt_h; mt < kHostMemoryTypeSize; mt++) {
      delete host[mt];
    }
    for (int mt = mt_d; mt < kMemoryTypeSize; mt++) {
      delete device[mt - mt_d];
    }
  }

 private:
  HostMemorySpace *NewHostCtrl(const MemoryType mt) {
    static_cast<void>(mt);
    ASC_ABORT("Unknown host memory controller!");
    return nullptr;
  }

  DeviceMemorySpace *NewDeviceCtrl(const MemoryType mt) {
    switch (mt) {
      case MT::kDevice: {
#if defined(ASC_USE_CUDA)
        return new CudaDeviceMemorySpace();
#else
        ASC_ABORT("No device memory controller!");
        break;
#endif
      }
      default:
        ASC_ABORT("Unknown device memory controller!");
    }
    return nullptr;
  }
};

}  // namespace internal

static internal::Ctrl *ctrl;

void *MemoryManager::New_(void *h_tmp, size_t bytes, MemoryType mt,
                          unsigned &flags) {
  ASC_ASSERT(exists_, "Internal error!");
  if (IsHostMemory(mt)) {
    ASC_ASSERT(mt != MemoryType::kHost && h_tmp == nullptr, "Internal error!");
    // d_mt = MemoryType::kDefault means d_mt = GetDualMemoryType(h_mt),
    // evaluated at the time when the device pointer is allocated, see
    // GetDevicePtr() and GetAliasDevicePtr()
    const MemoryType d_mt = MemoryType::kDefault;
    // We rely on the next call using lazy dev alloc
    return New_(h_tmp, bytes, mt, d_mt, Mem::kValidHost, flags);
  } else {
    const MemoryType h_mt = GetDualMemoryType(mt);
    return New_(h_tmp, bytes, h_mt, mt, Mem::kValidDevice, flags);
  }
}

void *MemoryManager::New_(void *h_tmp, size_t bytes, MemoryType h_mt,
                          MemoryType d_mt, unsigned valid_flags,
                          unsigned &flags) {
  ASC_ASSERT(exists_, "Internal error!");
  ASC_ASSERT(IsHostMemory(h_mt), "h_mt must be host type");
  ASC_ASSERT(
      IsDeviceMemory(d_mt) || d_mt == h_mt || d_mt == MemoryType::kDefault,
      "d_mt must be device type, the same is h_mt, or kDefault");
  ASC_ASSERT((h_mt != MemoryType::kHost || h_tmp != nullptr) &&
                 (h_mt == MemoryType::kHost || h_tmp == nullptr),
             "Internal error");
  ASC_ASSERT((valid_flags & ~(Mem::kValidHost | Mem::kValidDevice)) == 0,
             "Internal error");
  void *h_ptr;
  if (h_tmp == nullptr) {
    ctrl->Host(h_mt)->Alloc(&h_ptr, bytes);
  } else {
    h_ptr = h_tmp;
  }
  flags = Mem::kRegistered | Mem::kOwnsInternal | Mem::kOwnsHost |
          Mem::kOwnsDevice | valid_flags;
  // The other New_() method relies on this lazy allocation behavior.
  mm.Insert(h_ptr, bytes, h_mt, d_mt);  // lazy dev alloc
  // mm.InsertDevice(nullptr, h_ptr, bytes, h_mt, d_mt); // non-lazy dev alloc

  // ASC_VERIFY_TYPES(h_mt, mt); // done by mm.Insert() above
  CheckHostMemoryType_(h_mt, h_ptr, false);

  return h_ptr;
}

void *MemoryManager::Register_(void *ptr, void *h_tmp, size_t bytes,
                               MemoryType mt, bool own, bool alias,
                               unsigned &flags) {
  ASC_ASSERT(exists_, "Internal error!");
  const bool is_host_mem = IsHostMemory(mt);
  const MemType h_mt = is_host_mem ? mt : GetDualMemoryType(mt);
  const MemType d_mt = is_host_mem ? MemoryType::kDefault : mt;
  // d_mt = MemoryType::kDefault means d_mt = GetDualMemoryType(h_mt),
  // evaluated at the time when the device pointer is allocated, see
  // GetDevicePtr() and GetAliasDevicePtr()

  ASC_VERIFY_TYPES(h_mt, d_mt);

  if (ptr == nullptr && h_tmp == nullptr) {
    ASC_VERIFY(bytes == 0, "internal error");
    return nullptr;
  }

  ASC_VERIFY(!alias, "Cannot register an alias!");

  flags |= Mem::kRegistered | Mem::kOwnsInternal;
  void *h_ptr;

  if (is_host_mem) {  // HOST TYPES + kManaged
    h_ptr = ptr;
    mm.Insert(h_ptr, bytes, h_mt, d_mt);
    flags = (own ? flags | Mem::kOwnsHost : flags & ~Mem::kOwnsHost) |
            Mem::kOwnsDevice | Mem::kValidHost;
  } else {  // DEVICE TYPES
    ASC_VERIFY(ptr || bytes == 0,
               "cannot register NULL device pointer with bytes = " << bytes);
    if (h_tmp == nullptr) {
      ctrl->Host(h_mt)->Alloc(&h_ptr, bytes);
    } else {
      h_ptr = h_tmp;
    }
    mm.InsertDevice(ptr, h_ptr, bytes, h_mt, d_mt);
    flags = own ? flags | Mem::kOwnsDevice : flags & ~Mem::kOwnsDevice;
    flags |= (Mem::kOwnsHost | Mem::kValidDevice);
  }
  CheckHostMemoryType_(h_mt, h_ptr, alias);
  return h_ptr;
}

void MemoryManager::Register2_(void *h_ptr, void *d_ptr, size_t bytes,
                               MemoryType h_mt, MemoryType d_mt, bool own,
                               bool alias, unsigned &flags,
                               unsigned valid_flags) {
  ASC_CONTRACT_VAR(alias);
  ASC_ASSERT(exists_, "Internal error!");
  ASC_ASSERT(!alias, "Cannot register an alias!");
  ASC_VERIFY_TYPES(h_mt, d_mt);

  if (h_ptr == nullptr && d_ptr == nullptr) {
    ASC_VERIFY(bytes == 0, "internal error");
    return;
  }

  flags |= Mem::kRegistered | Mem::kOwnsInternal;

  ASC_VERIFY(d_ptr || bytes == 0,
             "cannot register NULL device pointer with bytes = " << bytes);
  mm.InsertDevice(d_ptr, h_ptr, bytes, h_mt, d_mt);
  flags = (own ? flags | (Mem::kOwnsHost | Mem::kOwnsDevice)
               : flags & ~(Mem::kOwnsHost | Mem::kOwnsDevice)) |
          valid_flags;

  CheckHostMemoryType_(h_mt, h_ptr, alias);
}

void MemoryManager::Alias_(void *base_h_ptr, size_t offset, size_t bytes,
                           unsigned base_flags, unsigned &flags) {
  mm.InsertAlias(base_h_ptr, reinterpret_cast<char *>(base_h_ptr) + offset,
                 bytes, base_flags & Mem::kAlias);
  flags = (base_flags | Mem::kAlias) & ~(Mem::kOwnsHost | Mem::kOwnsDevice);
  if (base_h_ptr) {
    flags |= Mem::kOwnsInternal;
  }
}

void MemoryManager::SetDeviceMemoryType_(void *h_ptr, unsigned flags,
                                         MemoryType d_mt) {
  ASC_VERIFY(h_ptr, "cannot set the device memory type: Memory is empty!");
  if (!(flags & Mem::kAlias)) {
    auto mem_iter = maps->memories.find(h_ptr);
    ASC_VERIFY(mem_iter != maps->memories.end(), "internal error");
    internal::Memory &mem = mem_iter->second;
    if (mem.d_mt == d_mt) {
      return;
    }
    ASC_VERIFY(mem.d_ptr == nullptr,
               "cannot set the device memory type:"
               " device memory is allocated!");
    mem.d_mt = d_mt;
  } else {
    auto alias_iter = maps->aliases.find(h_ptr);
    ASC_VERIFY(alias_iter != maps->aliases.end(), "internal error");
    internal::Alias &alias = alias_iter->second;
    internal::Memory &base_mem = *alias.mem;
    if (base_mem.d_mt == d_mt) {
      return;
    }
    ASC_VERIFY(base_mem.d_ptr == nullptr,
               "cannot set the device memory type:"
               " alias' base device memory is allocated!");
    base_mem.d_mt = d_mt;
  }
}

void MemoryManager::Delete_(void *h_ptr, MemoryType h_mt, unsigned flags) {
  const bool alias = flags & Mem::kAlias;
  const bool registered = flags & Mem::kRegistered;
  const bool owns_host = flags & Mem::kOwnsHost;
  const bool owns_device = flags & Mem::kOwnsDevice;
  const bool owns_internal = flags & Mem::kOwnsInternal;
  ASC_ASSERT(IsHostMemory(h_mt), "invalid h_mt = " << (int)h_mt);
  // ASC_ASSERT(registered || IsHostMemory(h_mt),"");
  ASC_ASSERT(!owns_device || owns_internal, "invalid Memory state");
  // If at least one of the 'own_*' flags is true then 'registered' must be
  // true too. An acceptable exception is the special case when 'h_ptr' is
  // NULL, and both 'own_device' and 'own_internal' are false -- this case is
  // an exception only when 'own_host' is true and 'registered' is false.
  ASC_ASSERT(registered || !(owns_host || owns_device || owns_internal) ||
                 (!(owns_device || owns_internal) && h_ptr == nullptr),
             "invalid Memory state");
  if (!mm.exists_ || !registered) {
    return;
  }
  if (alias) {
    if (owns_internal) {
      ASC_ASSERT(mm.IsAlias(h_ptr), "");
      ASC_ASSERT(h_mt == maps->aliases.at(h_ptr).h_mt, "");
      mm.EraseAlias(h_ptr);
    }
  } else {  // Known
    if (owns_host && (h_mt != MemoryType::kHost)) {
      ctrl->Host(h_mt)->Dealloc(h_ptr);
    }
    if (owns_internal) {
      ASC_ASSERT(mm.IsKnown(h_ptr), "");
      ASC_ASSERT(h_mt == maps->memories.at(h_ptr).h_mt, "");
      mm.Erase(h_ptr, owns_device);
    }
  }
}

void MemoryManager::DeleteDevice_(void *h_ptr, unsigned &flags) {
  const bool owns_device = flags & Mem::kOwnsDevice;
  if (owns_device) {
    mm.EraseDevice(h_ptr);
    flags = (flags | Mem::kValidHost) & ~Mem::kValidDevice;
  }
}

bool MemoryManager::MemoryClassCheck_(MemoryClass mc, void *h_ptr,
                                      MemoryType h_mt, size_t bytes,
                                      unsigned flags) {
  if (!h_ptr) {
    ASC_VERIFY(bytes == 0, "Trying to access NULL with size " << bytes);
    return true;
  }
  MemoryType d_mt;
  if (!(flags & Mem::kAlias)) {
    auto iter = maps->memories.find(h_ptr);
    ASC_VERIFY(iter != maps->memories.end(), "internal error");
    d_mt = iter->second.d_mt;
  } else {
    auto iter = maps->aliases.find(h_ptr);
    ASC_VERIFY(iter != maps->aliases.end(), "internal error");
    d_mt = iter->second.mem->d_mt;
  }
  if (d_mt == MemoryType::kDefault) {
    d_mt = GetDualMemoryType(h_mt);
  }
  switch (mc) {
    case MemoryClass::kHost32: {
      ASC_VERIFY(h_mt == MemoryType::kHost32 || h_mt == MemoryType::kHost64,
                 "");
      return true;
    }
    case MemoryClass::kHost64: {
      ASC_VERIFY(h_mt == MemoryType::kHost64, "");
      return true;
    }
    case MemoryClass::kDevice: {
      ASC_VERIFY(d_mt == MemoryType::kDevice || d_mt == MemoryType::kManaged,
                 "");
      return true;
    }
    case MemoryClass::kManaged: {
      ASC_VERIFY((h_mt == MemoryType::kManaged && d_mt == MemoryType::kManaged),
                 "");
      return true;
    }
    default:
      break;
  }
  return true;
}

void *MemoryManager::ReadWrite_(void *h_ptr, MemoryType h_mt, MemoryClass mc,
                                size_t bytes, unsigned &flags) {
  if (h_ptr) {
    CheckHostMemoryType_(h_mt, h_ptr, flags & Mem::kAlias);
  }
  if (bytes > 0) {
    ASC_VERIFY(flags & Mem::kRegistered, "");
  }
  ASC_ASSERT(MemoryClassCheck_(mc, h_ptr, h_mt, bytes, flags), "");
  if (IsHostMemory(GetMemoryType(mc)) && mc < MemoryClass::kDevice) {
    const bool copy = !(flags & Mem::kValidHost);
    flags = (flags | Mem::kValidHost) & ~Mem::kValidDevice;
    if (flags & Mem::kAlias) {
      return mm.GetAliasHostPtr(h_ptr, bytes, copy);
    } else {
      return mm.GetHostPtr(h_ptr, bytes, copy);
    }
  } else {
    const bool copy = !(flags & Mem::kValidDevice);
    flags = (flags | Mem::kValidDevice) & ~Mem::kValidHost;
    if (flags & Mem::kAlias) {
      return mm.GetAliasDevicePtr(h_ptr, bytes, copy);
    } else {
      return mm.GetDevicePtr(h_ptr, bytes, copy);
    }
  }
}

const void *MemoryManager::Read_(void *h_ptr, MemoryType h_mt, MemoryClass mc,
                                 size_t bytes, unsigned &flags) {
  if (h_ptr) {
    CheckHostMemoryType_(h_mt, h_ptr, flags & Mem::kAlias);
  }
  if (bytes > 0) {
    ASC_VERIFY(flags & Mem::kRegistered, "");
  }
  ASC_ASSERT(MemoryClassCheck_(mc, h_ptr, h_mt, bytes, flags), "");
  if (IsHostMemory(GetMemoryType(mc)) && mc < MemoryClass::kDevice) {
    const bool copy = !(flags & Mem::kValidHost);
    flags |= Mem::kValidHost;
    if (flags & Mem::kAlias) {
      return mm.GetAliasHostPtr(h_ptr, bytes, copy);
    } else {
      return mm.GetHostPtr(h_ptr, bytes, copy);
    }
  } else {
    const bool copy = !(flags & Mem::kValidDevice);
    flags |= Mem::kValidDevice;
    if (flags & Mem::kAlias) {
      return mm.GetAliasDevicePtr(h_ptr, bytes, copy);
    } else {
      return mm.GetDevicePtr(h_ptr, bytes, copy);
    }
  }
}

void *MemoryManager::Write_(void *h_ptr, MemoryType h_mt, MemoryClass mc,
                            size_t bytes, unsigned &flags) {
  if (h_ptr) {
    CheckHostMemoryType_(h_mt, h_ptr, flags & Mem::kAlias);
  }
  if (bytes > 0) {
    ASC_VERIFY(flags & Mem::kRegistered, "");
  }
  ASC_ASSERT(MemoryClassCheck_(mc, h_ptr, h_mt, bytes, flags), "");
  if (IsHostMemory(GetMemoryType(mc)) && mc < MemoryClass::kDevice) {
    flags = (flags | Mem::kValidHost) & ~Mem::kValidDevice;
    if (flags & Mem::kAlias) {
      return mm.GetAliasHostPtr(h_ptr, bytes, false);
    } else {
      return mm.GetHostPtr(h_ptr, bytes, false);
    }
  } else {
    flags = (flags | Mem::kValidDevice) & ~Mem::kValidHost;
    if (flags & Mem::kAlias) {
      return mm.GetAliasDevicePtr(h_ptr, bytes, false);
    } else {
      return mm.GetDevicePtr(h_ptr, bytes, false);
    }
  }
}

void MemoryManager::SyncAlias_(const void *base_h_ptr, void *alias_h_ptr,
                               size_t alias_bytes, unsigned base_flags,
                               unsigned &alias_flags) {
  // This is called only when (base_flags & Mem::kRegistered) is true.
  // Note that (alias_flags & kRegistered) may not be true.
  ASC_ASSERT(alias_flags & Mem::kAlias, "not an alias");
  if ((base_flags & Mem::kValidHost) && !(alias_flags & Mem::kValidHost)) {
    mm.GetAliasHostPtr(alias_h_ptr, alias_bytes, true);
  }
  if ((base_flags & Mem::kValidDevice) && !(alias_flags & Mem::kValidDevice)) {
    if (!(alias_flags & Mem::kRegistered)) {
      mm.InsertAlias(base_h_ptr, alias_h_ptr, alias_bytes,
                     base_flags & Mem::kAlias);
      alias_flags = (alias_flags | Mem::kRegistered | Mem::kOwnsInternal) &
                    ~(Mem::kOwnsHost | Mem::kOwnsDevice);
    }
    mm.GetAliasDevicePtr(alias_h_ptr, alias_bytes, true);
  }
  alias_flags = (alias_flags & ~(Mem::kValidHost | Mem::kValidDevice)) |
                (base_flags & (Mem::kValidHost | Mem::kValidDevice));
}

MemoryType MemoryManager::GetDeviceMemoryType_(void *h_ptr, bool alias) {
  if (mm.exists_) {
    if (!alias) {
      auto iter = maps->memories.find(h_ptr);
      ASC_ASSERT(iter != maps->memories.end(), "internal error");
      return iter->second.d_mt;
    }
    // alias == true
    auto iter = maps->aliases.find(h_ptr);
    ASC_ASSERT(iter != maps->aliases.end(), "internal error");
    return iter->second.mem->d_mt;
  }
  ASC_ABORT("internal error");
  return MemoryManager::host_mem_type_;
}

MemoryType MemoryManager::GetHostMemoryType_(void *h_ptr) {
  if (!mm.exists_) {
    return MemoryManager::host_mem_type_;
  }
  if (mm.IsKnown(h_ptr)) {
    return maps->memories.at(h_ptr).h_mt;
  }
  if (mm.IsAlias(h_ptr)) {
    return maps->aliases.at(h_ptr).h_mt;
  }
  return MemoryManager::host_mem_type_;
}

void MemoryManager::Copy_(void *dst_h_ptr, const void *src_h_ptr, size_t bytes,
                          unsigned src_flags, unsigned &dst_flags) {
  // Type of copy to use based on the src and dest validity flags:
  //            |       src
  //            |  h  |  d  |  hd
  // -----------+-----+-----+------
  //         h  | h2h   d2h   h2h
  //  dest   d  | h2d   d2d   d2d
  //        hd  | h2h   d2d   d2d

  ASC_ASSERT(bytes != 0, "this method should not be called with bytes = 0");
  ASC_ASSERT(dst_h_ptr != nullptr, "invalid dst_h_ptr = nullptr");
  ASC_ASSERT(src_h_ptr != nullptr, "invalid src_h_ptr = nullptr");

  const bool dst_on_host =
      (dst_flags & Mem::kValidHost) &&
      (!(dst_flags & Mem::kValidDevice) ||
       ((src_flags & Mem::kValidHost) && !(src_flags & Mem::kValidDevice)));

  dst_flags = dst_flags & ~(dst_on_host ? Mem::kValidDevice : Mem::kValidHost);

  const bool src_on_host =
      (src_flags & Mem::kValidHost) &&
      (!(src_flags & Mem::kValidDevice) ||
       ((dst_flags & Mem::kValidHost) && !(dst_flags & Mem::kValidDevice)));

  const void *src_d_ptr =
      src_on_host ? NULL
                  : ((src_flags & Mem::kAlias)
                         ? mm.GetAliasDevicePtr(src_h_ptr, bytes, false)
                         : mm.GetDevicePtr(src_h_ptr, bytes, false));

  if (dst_on_host) {
    if (src_on_host) {
      if (dst_h_ptr != src_h_ptr && bytes != 0) {
        ASC_ASSERT((const char *)dst_h_ptr + bytes <= src_h_ptr ||
                       (const char *)src_h_ptr + bytes <= dst_h_ptr,
                   "data overlaps!");
        std::memcpy(dst_h_ptr, src_h_ptr, bytes);
      }
    } else {
      if (dst_h_ptr != src_d_ptr && bytes != 0) {
        MemoryType src_d_mt = (src_flags & Mem::kAlias)
                                  ? maps->aliases.at(src_h_ptr).mem->d_mt
                                  : maps->memories.at(src_h_ptr).d_mt;
        ctrl->Device(src_d_mt)->DtoH(dst_h_ptr, src_d_ptr, bytes);
      }
    }
  } else {
    void *dest_d_ptr = (dst_flags & Mem::kAlias)
                           ? mm.GetAliasDevicePtr(dst_h_ptr, bytes, false)
                           : mm.GetDevicePtr(dst_h_ptr, bytes, false);
    if (src_on_host) {
      const bool known = mm.IsKnown(dst_h_ptr);
      const bool alias = dst_flags & Mem::kAlias;
      ASC_VERIFY(alias || known, "");
      const MemoryType d_mt = known ? maps->memories.at(dst_h_ptr).d_mt
                                    : maps->aliases.at(dst_h_ptr).mem->d_mt;
      ctrl->Device(d_mt)->HtoD(dest_d_ptr, src_h_ptr, bytes);
    } else {
      if (dest_d_ptr != src_d_ptr && bytes != 0) {
        const bool known = mm.IsKnown(dst_h_ptr);
        const bool alias = dst_flags & Mem::kAlias;
        ASC_VERIFY(alias || known, "");
        const MemoryType d_mt = known ? maps->memories.at(dst_h_ptr).d_mt
                                      : maps->aliases.at(dst_h_ptr).mem->d_mt;
        ctrl->Device(d_mt)->DtoD(dest_d_ptr, src_d_ptr, bytes);
      }
    }
  }
}

void MemoryManager::CopyToHost_(void *dest_h_ptr, const void *src_h_ptr,
                                size_t bytes, unsigned src_flags) {
  ASC_ASSERT(bytes != 0, "this method should not be called with bytes = 0");
  ASC_ASSERT(dest_h_ptr != nullptr, "invalid dest_h_ptr = nullptr");
  ASC_ASSERT(src_h_ptr != nullptr, "invalid src_h_ptr = nullptr");

  const bool src_on_host = src_flags & Mem::kValidHost;
  if (src_on_host) {
    if (dest_h_ptr != src_h_ptr && bytes != 0) {
      ASC_ASSERT((char *)dest_h_ptr + bytes <= src_h_ptr ||
                     (const char *)src_h_ptr + bytes <= dest_h_ptr,
                 "data overlaps!");
      std::memcpy(dest_h_ptr, src_h_ptr, bytes);
    }
  } else {
    ASC_ASSERT(IsKnown_(src_h_ptr), "internal error");
    const void *src_d_ptr = (src_flags & Mem::kAlias)
                                ? mm.GetAliasDevicePtr(src_h_ptr, bytes, false)
                                : mm.GetDevicePtr(src_h_ptr, bytes, false);
    MemoryType src_d_mt = (src_flags & Mem::kAlias)
                              ? maps->aliases.at(src_h_ptr).mem->d_mt
                              : maps->memories.at(src_h_ptr).d_mt;
    ctrl->Device(src_d_mt)->DtoH(dest_h_ptr, src_d_ptr, bytes);
  }
}

void MemoryManager::CopyFromHost_(void *dest_h_ptr, const void *src_h_ptr,
                                  size_t bytes, unsigned &dest_flags) {
  ASC_ASSERT(bytes != 0, "this method should not be called with bytes = 0");
  ASC_ASSERT(dest_h_ptr != nullptr, "invalid dest_h_ptr = nullptr");
  ASC_ASSERT(src_h_ptr != nullptr, "invalid src_h_ptr = nullptr");

  const bool dest_on_host = dest_flags & Mem::kValidHost;
  if (dest_on_host) {
    if (dest_h_ptr != src_h_ptr && bytes != 0) {
      ASC_ASSERT((char *)dest_h_ptr + bytes <= src_h_ptr ||
                     (const char *)src_h_ptr + bytes <= dest_h_ptr,
                 "data overlaps!");
      std::memcpy(dest_h_ptr, src_h_ptr, bytes);
    }
  } else {
    void *dest_d_ptr = (dest_flags & Mem::kAlias)
                           ? mm.GetAliasDevicePtr(dest_h_ptr, bytes, false)
                           : mm.GetDevicePtr(dest_h_ptr, bytes, false);
    MemoryType dest_d_mt = (dest_flags & Mem::kAlias)
                               ? maps->aliases.at(dest_h_ptr).mem->d_mt
                               : maps->memories.at(dest_h_ptr).d_mt;
    ctrl->Device(dest_d_mt)->HtoD(dest_d_ptr, src_h_ptr, bytes);
  }
  dest_flags =
      dest_flags & ~(dest_on_host ? Mem::kValidDevice : Mem::kValidHost);
}

bool MemoryManager::IsKnown_(const void *h_ptr) {
  return maps->memories.find(h_ptr) != maps->memories.end();
}

bool MemoryManager::IsAlias_(const void *h_ptr) {
  return maps->aliases.find(h_ptr) != maps->aliases.end();
}

void MemoryManager::Insert(void *h_ptr, size_t bytes, MemoryType h_mt,
                           MemoryType d_mt) {
  if (h_ptr == NULL) {
    ASC_VERIFY(bytes == 0, "Trying to add NULL with size " << bytes);
    return;
  }
  ASC_VERIFY_TYPES(h_mt, d_mt);
#ifdef ASC_DEBUG
  auto res =
#endif
      maps->memories.emplace(h_ptr, internal::Memory(h_ptr, bytes, h_mt, d_mt));
#ifdef ASC_DEBUG
  if (res.second == false) {
    auto &m = res.first->second;
    ASC_VERIFY(m.bytes >= bytes && m.h_mt == h_mt &&
                   (m.d_mt == d_mt || (d_mt == MemoryType::kDefault &&
                                       m.d_mt == GetDualMemoryType(h_mt))),
               "Address already present with different attributes!");
  }
#endif
}

void MemoryManager::InsertDevice(void *d_ptr, void *h_ptr, size_t bytes,
                                 MemoryType h_mt, MemoryType d_mt) {
  // ASC_VERIFY_TYPES(h_mt, d_mt); // done by Insert() below
  ASC_ASSERT(h_ptr != NULL, "internal error");
  Insert(h_ptr, bytes, h_mt, d_mt);
  internal::Memory &mem = maps->memories.at(h_ptr);
  if (d_ptr == NULL && bytes != 0) {
    ctrl->Device(d_mt)->Alloc(mem);
  } else {
    mem.d_ptr = d_ptr;
  }
}

void MemoryManager::InsertAlias(const void *base_ptr, void *alias_ptr,
                                const size_t bytes, const bool base_is_alias) {
  size_t offset = static_cast<size_t>(static_cast<const char *>(alias_ptr) -
                                      static_cast<const char *>(base_ptr));
  if (!base_ptr) {
    ASC_VERIFY(offset == 0, "Trying to add alias to NULL at offset " << offset);
    return;
  }
  if (base_is_alias) {
    const internal::Alias &alias = maps->aliases.at(base_ptr);
    ASC_ASSERT(alias.mem, "");
    base_ptr = alias.mem->h_ptr;
    offset += alias.offset;
  }
  internal::Memory &mem = maps->memories.at(base_ptr);
  ASC_VERIFY(offset + bytes <= mem.bytes, "invalid alias");
  auto res = maps->aliases.emplace(alias_ptr,
                                   internal::Alias{&mem, offset, 1, mem.h_mt});
  if (res.second == false) {  // alias_ptr was already in the map
    internal::Alias &alias = res.first->second;
    // Update the alias data in case the existing alias is dangling
    alias.mem = &mem;
    alias.offset = offset;
    alias.h_mt = mem.h_mt;
    alias.counter++;
  }
}

void MemoryManager::Erase(void *h_ptr, bool free_dev_ptr) {
  if (!h_ptr) {
    return;
  }
  auto mem_map_iter = maps->memories.find(h_ptr);
  if (mem_map_iter == maps->memories.end()) {
    Error("Unknown pointer!");
  }
  internal::Memory &mem = mem_map_iter->second;
  if (mem.d_ptr && free_dev_ptr) {
    ctrl->Device(mem.d_mt)->Dealloc(mem);
  }
  maps->memories.erase(mem_map_iter);
}

void MemoryManager::EraseDevice(void *h_ptr) {
  if (!h_ptr) {
    return;
  }
  auto mem_map_iter = maps->memories.find(h_ptr);
  if (mem_map_iter == maps->memories.end()) {
    Error("Unknown pointer!");
  }
  internal::Memory &mem = mem_map_iter->second;
  if (mem.d_ptr) {
    ctrl->Device(mem.d_mt)->Dealloc(mem);
  }
  mem.d_ptr = nullptr;
}

void MemoryManager::EraseAlias(void *alias_ptr) {
  if (!alias_ptr) {
    return;
  }
  auto alias_map_iter = maps->aliases.find(alias_ptr);
  if (alias_map_iter == maps->aliases.end()) {
    Error("Unknown alias!");
  }
  internal::Alias &alias = alias_map_iter->second;
  if (--alias.counter) {
    return;
  }
  maps->aliases.erase(alias_map_iter);
}

void *MemoryManager::GetDevicePtr(const void *h_ptr, size_t bytes,
                                  bool copy_data) {
  if (!h_ptr) {
    ASC_VERIFY(bytes == 0, "Trying to access NULL with size " << bytes);
    return NULL;
  }
  internal::Memory &mem = maps->memories.at(h_ptr);
  const MemoryType &h_mt = mem.h_mt;
  MemoryType &d_mt = mem.d_mt;
  ASC_VERIFY_TYPES(h_mt, d_mt);
  if (!mem.d_ptr) {
    if (d_mt == MemoryType::kDefault) {
      d_mt = GetDualMemoryType(h_mt);
    }
    if (mem.bytes) {
      ctrl->Device(d_mt)->Alloc(mem);
    }
  }
  // Aliases might have done some protections
  if (mem.d_ptr) {
    ctrl->Device(d_mt)->Unprotect(mem);
  }
  if (copy_data) {
    ASC_ASSERT(bytes <= mem.bytes, "invalid copy size");
    if (bytes) {
      ctrl->Device(d_mt)->HtoD(mem.d_ptr, h_ptr, bytes);
    }
  }
  ctrl->Host(h_mt)->Protect(mem, bytes);
  return mem.d_ptr;
}

void *MemoryManager::GetAliasDevicePtr(const void *alias_ptr, size_t bytes,
                                       bool copy) {
  if (!alias_ptr) {
    ASC_VERIFY(bytes == 0, "Trying to access NULL with size " << bytes);
    return NULL;
  }
  auto &alias_map = maps->aliases;
  auto alias_map_iter = alias_map.find(alias_ptr);
  if (alias_map_iter == alias_map.end()) {
    Error("alias not found");
  }
  const internal::Alias &alias = alias_map_iter->second;
  const size_t offset = alias.offset;
  internal::Memory &mem = *alias.mem;
  const MemoryType &h_mt = mem.h_mt;
  MemoryType &d_mt = mem.d_mt;
  ASC_VERIFY_TYPES(h_mt, d_mt);
  if (!mem.d_ptr) {
    if (d_mt == MemoryType::kDefault) {
      d_mt = GetDualMemoryType(h_mt);
    }
    if (mem.bytes) {
      ctrl->Device(d_mt)->Alloc(mem);
    }
  }
  void *alias_h_ptr = static_cast<char *>(mem.h_ptr) + offset;
  void *alias_d_ptr = static_cast<char *>(mem.d_ptr) + offset;
  ASC_ASSERT(alias_h_ptr == alias_ptr, "internal error");
  ASC_ASSERT(offset + bytes <= mem.bytes, "internal error");
  mem.d_rw = mem.h_rw = false;
  if (mem.d_ptr) {
    ctrl->Device(d_mt)->AliasUnprotect(alias_d_ptr, bytes);
  }
  ctrl->Host(h_mt)->AliasUnprotect(alias_ptr, bytes);
  if (copy && mem.d_ptr) {
    ctrl->Device(d_mt)->HtoD(alias_d_ptr, alias_h_ptr, bytes);
  }
  ctrl->Host(h_mt)->AliasProtect(alias_ptr, bytes);
  return alias_d_ptr;
}

void *MemoryManager::GetHostPtr(const void *ptr, size_t bytes, bool copy) {
  const internal::Memory &mem = maps->memories.at(ptr);
  ASC_ASSERT(mem.h_ptr == ptr, "internal error");
  ASC_ASSERT(bytes <= mem.bytes, "internal error")
  const MemoryType &h_mt = mem.h_mt;
  const MemoryType &d_mt = mem.d_mt;
  ASC_VERIFY_TYPES(h_mt, d_mt);
  // Aliases might have done some protections
  ctrl->Host(h_mt)->Unprotect(mem, bytes);
  if (mem.d_ptr) {
    ctrl->Device(d_mt)->Unprotect(mem);
  }
  if (copy && mem.d_ptr) {
    ctrl->Device(d_mt)->DtoH(mem.h_ptr, mem.d_ptr, bytes);
  }
  if (mem.d_ptr) {
    ctrl->Device(d_mt)->Protect(mem);
  }
  return mem.h_ptr;
}

void *MemoryManager::GetAliasHostPtr(const void *ptr, size_t bytes,
                                     bool copy_data) {
  const internal::Alias &alias = maps->aliases.at(ptr);
  const internal::Memory *const mem = alias.mem;
  const MemoryType &h_mt = mem->h_mt;
  const MemoryType &d_mt = mem->d_mt;
  ASC_VERIFY_TYPES(h_mt, d_mt);
  void *alias_h_ptr = static_cast<char *>(mem->h_ptr) + alias.offset;
  void *alias_d_ptr = static_cast<char *>(mem->d_ptr) + alias.offset;
  ASC_ASSERT(alias_h_ptr == ptr, "internal error");
  mem->h_rw = false;
  ctrl->Host(h_mt)->AliasUnprotect(alias_h_ptr, bytes);
  if (mem->d_ptr) {
    ctrl->Device(d_mt)->AliasUnprotect(alias_d_ptr, bytes);
  }
  if (copy_data && mem->d_ptr) {
    ctrl->Device(d_mt)->DtoH(const_cast<void *>(ptr), alias_d_ptr, bytes);
  }
  if (mem->d_ptr) {
    ctrl->Device(d_mt)->AliasProtect(alias_d_ptr, bytes);
  }
  return alias_h_ptr;
}

void MemoryManager::Init() {
  if (exists_) {
    return;
  }
  maps = new internal::Maps();
  ctrl = new internal::Ctrl();
  ctrl->Configure();
  exists_ = true;
}

MemoryManager::MemoryManager() { Init(); }

MemoryManager::~MemoryManager() {
  if (exists_) {
    Destroy();
  }
}

void MemoryManager::SetDualMemoryType(MemoryType mt, MemoryType dual_mt) {
  ASC_VERIFY(!configured_,
             "changing the dual MemoryTypes is not allowed after"
             " MemoryManager configuration!");
  UpdateDualMemoryType(mt, dual_mt);
}

void MemoryManager::UpdateDualMemoryType(MemoryType mt, MemoryType dual_mt) {
  ASC_VERIFY((int)mt < kMemoryTypeSize, "invalid MemoryType, mt = " << (int)mt);
  ASC_VERIFY((int)dual_mt < kMemoryTypeSize,
             "invalid dual MemoryType, dual_mt = " << (int)dual_mt);

  if ((IsHostMemory(mt) && IsDeviceMemory(dual_mt)) ||
      (IsDeviceMemory(mt) && IsHostMemory(dual_mt))) {
    dual_map_[static_cast<int>(mt)] = dual_mt;
  } else {
    // mt + dual_mt is not a pair of host + device types: this is only allowed
    // when mt == dual_mt and mt is a host type; in this case we do not
    // actually update the dual
    ASC_VERIFY(mt == dual_mt && IsHostMemory(mt),
               "invalid (mt, dual_mt) pair: ("
                   << kMemoryTypeName[(int)mt] << ", "
                   << kMemoryTypeName[(int)dual_mt] << ')');
  }
}

void MemoryManager::Configure(const MemoryType host_mt,
                              const MemoryType device_mt) {
  MemoryManager::UpdateDualMemoryType(host_mt, device_mt);
  MemoryManager::UpdateDualMemoryType(device_mt, host_mt);
  Init();
  host_mem_type_ = host_mt;
  device_mem_type_ = device_mt;
  configured_ = true;
}

void MemoryManager::Destroy() {
  ASC_VERIFY(exists_, "MemoryManager has already been destroyed!");
  for (auto &n : maps->memories) {
    internal::Memory &mem = n.second;
    bool mem_h_ptr = mem.h_mt != MemoryType::kHost && mem.h_ptr;
    if (mem_h_ptr) {
      ctrl->Host(mem.h_mt)->Dealloc(mem.h_ptr);
    }
    if (mem.d_ptr) {
      ctrl->Device(mem.d_mt)->Dealloc(mem);
    }
  }
  delete maps;
  maps = nullptr;
  delete ctrl;
  ctrl = nullptr;
  host_mem_type_ = MemoryType::kHost;
  device_mem_type_ = MemoryType::kHost;
  exists_ = false;
  configured_ = false;
}

void MemoryManager::RegisterCheck(void *ptr) {
  if (ptr != NULL) {
    if (!IsKnown(ptr)) {
      Error("Pointer is not registered!");
    }
  }
}

int MemoryManager::PrintPtrs(std::ostream &os) {
  int n_out = 0;
  for (const auto &n : maps->memories) {
    const internal::Memory &mem = n.second;
    os << "\nkey " << n.first << ", "
       << "h_ptr " << mem.h_ptr << ", "
       << "d_ptr " << mem.d_ptr;
    n_out++;
  }
  if (maps->memories.size() > 0) {
    os << std::endl;
  }
  return n_out;
}

int MemoryManager::PrintAliases(std::ostream &os) {
  int n_out = 0;
  for (const auto &n : maps->aliases) {
    const internal::Alias &alias = n.second;
    os << "\nalias: key " << n.first << ", "
       << "h_ptr " << alias.mem->h_ptr << ", "
       << "offset " << alias.offset << ", "
       << "counter " << alias.counter;
    n_out++;
  }
  if (maps->aliases.size() > 0) {
    os << std::endl;
  }
  return n_out;
}

int MemoryManager::CompareHostAndDevice_(void *h_ptr, size_t size,
                                         unsigned flags) {
  void *d_ptr = (flags & Mem::kAlias) ? mm.GetAliasDevicePtr(h_ptr, size, false)
                                      : mm.GetDevicePtr(h_ptr, size, false);
  char *h_buf = new char[size];
#ifdef ASC_USE_CUDA
  CuMemcpyDtoH(h_buf, d_ptr, size);
#else
  std::memcpy(h_buf, d_ptr, size);
#endif
  int res = std::memcmp(h_ptr, h_buf, size);
  delete[] h_buf;
  return res;
}

void MemoryPrintFlags(unsigned flags) {
  typedef Memory<int> Mem;
  asc::mout << "\n   registered    = " << bool(flags & Mem::kRegistered)
            << "\n   owns host     = " << bool(flags & Mem::kOwnsHost)
            << "\n   owns device   = " << bool(flags & Mem::kOwnsDevice)
            << "\n   owns internal = " << bool(flags & Mem::kOwnsInternal)
            << "\n   valid host    = " << bool(flags & Mem::kValidHost)
            << "\n   valid device  = " << bool(flags & Mem::kValidDevice)
            << "\n   device flag   = " << bool(flags & Mem::kUseDevice)
            << "\n   alias         = " << bool(flags & Mem::kAlias)
            << "\n   device intent = " << bool(flags & Mem::kDeviceIntent)
            << std::endl;
}

void MemoryManager::CheckHostMemoryType_(MemoryType h_mt, void *h_ptr,
                                         bool alias) {
  if (!mm.exists_) {
    return;
  }
  if (!alias) {
    auto it = maps->memories.find(h_ptr);
    ASC_VERIFY(it != maps->memories.end(),
               "host pointer is not registered: h_ptr = " << h_ptr);
    ASC_VERIFY(h_mt == it->second.h_mt, "host pointer MemoryType mismatch");
  } else {
    auto it = maps->aliases.find(h_ptr);
    ASC_VERIFY(it != maps->aliases.end(),
               "alias pointer is not registered: h_ptr = " << h_ptr);
    ASC_VERIFY(h_mt == it->second.h_mt, "alias pointer MemoryType mismatch");
  }
}

MemoryManager mm;

bool MemoryManager::exists_ = false;
bool MemoryManager::configured_ = false;

MemoryType MemoryManager::host_mem_type_ = MemoryType::kHost;
MemoryType MemoryManager::device_mem_type_ = MemoryType::kHost;

MemoryType MemoryManager::dual_map_[kMemoryTypeSize] = {
    /* kHost    */ MemoryType::kDevice,
    /* kHost32  */ MemoryType::kDevice,
    /* kHost64  */ MemoryType::kDevice,
    /* kManaged */ MemoryType::kManaged,
    /* kDevice  */ MemoryType::kHost};

const char *kMemoryTypeName[kMemoryTypeSize] = {
    "host-std", "host-32", "host-64",
#if defined(ASC_USE_CUDA)
    "cuda-uvm", "cuda",
#else
    "managed",  "device",
#endif
};

}  // namespace asc
