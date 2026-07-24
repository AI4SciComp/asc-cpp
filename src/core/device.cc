// ============================================================================
// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.
//
// File: asc/device.cc
// Author: Yi Cai
// E-mail: yicaim@stu.xmu.edu.cn
// ============================================================================

#include <unordered_map>
#include <map>
#include "asc/core/device.h"
#include "asc/core/forall.h"

namespace asc {

// Place the following variables in the asc::internal namespace, so that they
// will not be included in the doxygen documentation.
namespace internal {

// Backends listed by priority, high to low:
static const unsigned backend_list[Backend::kNBackends] = {
    Backend::kCuda, Backend::kOmp, Backend::kCpu};

// Backend names listed by priority, high to low:
static const char *backend_name[Backend::kNBackends] = {"cuda", "omp", "cpu"};

}  // namespace internal

// Initialize the unique global Device variable.
Device Device::device_singleton;
bool Device::device_env = false;
bool Device::mem_host_env = false;
bool Device::mem_device_env = false;
bool Device::mem_types_set = false;

Device::Device() {
  if (std::getenv("ASC_MEMORY") && !mem_host_env && !mem_device_env) {
    std::string mem_backend(std::getenv("ASC_MEMORY"));
    if (mem_backend == "host") {
      mem_host_env = true;
      host_mem_type_ = MemoryType::kHost;
      device_mem_type_ = MemoryType::kHost;
    } else if (mem_backend == "host32") {
      mem_host_env = true;
      host_mem_type_ = MemoryType::kHost32;
      device_mem_type_ = MemoryType::kHost32;
    } else if (mem_backend == "host64") {
      mem_host_env = true;
      host_mem_type_ = MemoryType::kHost64;
      device_mem_type_ = MemoryType::kHost64;
    } else if (false
#ifdef ASC_USE_CUDA
               || mem_backend == "cuda"
#endif
    ) {
      mem_host_env = true;
      host_mem_type_ = MemoryType::kHost;
      mem_device_env = true;
      device_mem_type_ = MemoryType::kDevice;
    } else if (mem_backend == "uvm") {
      mem_host_env = true;
      mem_device_env = true;
      host_mem_type_ = MemoryType::kManaged;
      device_mem_type_ = MemoryType::kManaged;
    } else {
      ASC_ABORT("Unknown memory backend!");
    }
    mm.Configure(host_mem_type_, device_mem_type_);
  }

  if (std::getenv("ASC_DEVICE")) {
    std::string device(std::getenv("ASC_DEVICE"));
    Configure(device);
    device_env = true;
  }
}

Device::~Device() {
  if (device_env && !destroy_mm_) {
    return;
  }
  if (!device_env && destroy_mm_ && !mem_host_env) {
    mm.Destroy();
  }
  Get().ngpu_ = -1;
  Get().backends_ = Backend::kCpu;
  Get().host_mem_type_ = MemoryType::kHost;
  Get().host_mem_class_ = MemoryClass::kHost;
  Get().device_mem_type_ = MemoryType::kHost;
  Get().device_mem_class_ = MemoryClass::kHost;
}

void Device::Configure(const std::string &device, const int device_id) {
  // If a device was configured via the environment, skip the configuration,
  // and avoid the 'singleton_device' to destroy the mm.
  if (device_env) {
    std::memcpy(reinterpret_cast<void *>(this), &Get(), sizeof(Device));
    Get().destroy_mm_ = false;
    return;
  }

  std::map<std::string, unsigned> bmap;
  for (int i = 0; i < Backend::kNBackends; i++) {
    bmap[internal::backend_name[i]] = internal::backend_list[i];
  }
  // auto-detect GPU configurations
  // assumes only CUDA are available
#ifdef ASC_USE_CUDA
  bmap["gpu"] = Backend::kCuda;
#endif
  std::string device_option;
  std::string::size_type beg = 0, end;
  while (1) {
    end = device.find(',', beg);
    end = (end != std::string::npos) ? end : device.size();
    const std::string bname = device.substr(beg, end - beg);
    const auto option = bname.find(':');
    const std::string backend =
        (option != std::string::npos) ? bname.substr(0, option) : bname;
    const auto it = bmap.find(backend);
    ASC_VERIFY(it != bmap.end(), "Invalid backend name: '" << backend << '\'');
    Get().MarkBackend(it->second);
    if (option != std::string::npos) {
      device_option += bname.substr(option);
    }
    if (end == device.size()) {
      break;
    }
    beg = end + 1;
  }

  // Perform setup.
  Get().Setup(device_option, device_id);

  // Configure the host/device MemoryType/MemoryClass.
  Get().UpdateMemoryTypeAndClass(device_option);

  // Copy all data members from the global 'singleton_device' into '*this'.
  if (this != &Get()) {
    std::memcpy(reinterpret_cast<void *>(this), &Get(), sizeof(Device));
  }

  // Only '*this' will call the MemoryManager::Destroy() method.
  destroy_mm_ = true;
}

// static method
void Device::SetMemoryTypes(MemoryType h_mt, MemoryType d_mt) {
  // If the device and/or the MemoryTypes are configured through the
  // environment (variables 'ASC_DEVICE', 'ASC_MEMORY'), ignore calls to this
  // method.
  if (mem_host_env || mem_device_env || device_env) {
    return;
  }

  ASC_VERIFY(!IsConfigured(),
             "the default MemoryTypes can only be set before"
             " Device construction and configuration");
  ASC_VERIFY(IsHostMemory(h_mt),
             "invalid host MemoryType, h_mt = " << (int)h_mt);
  ASC_VERIFY(IsDeviceMemory(d_mt) || d_mt == h_mt,
             "invalid device MemoryType, d_mt = " << (int)d_mt << " (h_mt = "
                                                  << (int)h_mt << ')');

  Get().host_mem_type_ = h_mt;
  Get().device_mem_type_ = d_mt;
  mem_types_set = true;

  // h_mt and d_mt will be set as dual to each other during configuration by
  // the call mm.Configure(...) in UpdateMemoryTypeAndClass()
}

void Device::Print(std::ostream &os) {
  os << "Device configuration: ";
  bool add_comma = false;
  for (int i = 0; i < Backend::kNBackends; i++) {
    if (backends_ & internal::backend_list[i]) {
      if (add_comma) {
        os << ',';
      }
      add_comma = true;
      os << internal::backend_name[i];
    }
  }
  os << '\n';
  os << "Memory configuration: "
     << kMemoryTypeName[static_cast<int>(host_mem_type_)];
  if (Device::Allows(Backend::kCuda)) {
    os << ',' << kMemoryTypeName[static_cast<int>(device_mem_type_)];
  }
  os << std::endl;
}

void Device::UpdateMemoryTypeAndClass(const std::string &device_option) {
  const bool device = Device::Allows(Backend::kCuda);

  // Enable the device memory type
  if (device) {
    if (!mem_device_env) {
      device_mem_type_ = MemoryType::kDevice;
    }
    device_mem_class_ = MemoryClass::kDevice;
  }

  // Enable the UVM shortcut when requested
  if (device && device_option.find(":uvm") != std::string::npos) {
    host_mem_type_ = MemoryType::kManaged;
    device_mem_type_ = MemoryType::kManaged;
  }

  ASC_VERIFY(!device || IsDeviceMemory(device_mem_type_),
             "invalid device memory configuration!");

  // Update the memory manager with the new settings
  mm.Configure(host_mem_type_, device_mem_type_);
}

// static method
int Device::GetDeviceCount() {
  if (Get().ngpu_ >= 0) {
    return Get().ngpu_;
  }
#if defined(ASC_USE_CUDA)
  return CuGetDeviceCount();
#else
  ASC_ABORT(
      "Unable to query number of available devices without ASC_USE_CUDA!");
  return -1;
#endif
}

static void CudaDeviceSetup(const int dev, int &ngpu) {
#ifdef ASC_USE_CUDA
  ngpu = CuGetDeviceCount();
  ASC_VERIFY(ngpu > 0, "No kCuda device found!");
  ASC_GPU_CHECK(cudaSetDevice(dev));
#else
  ASC_CONTRACT_VAR(dev);
  ASC_CONTRACT_VAR(ngpu);
#endif
}

void Device::Setup(const std::string &device_option, const int device_id) {
  static_cast<void>(device_option);
  ASC_VERIFY(ngpu_ == -1, "the asc::Device is already configured!");

  ngpu_ = 0;
  dev_ = device_id;
#ifndef ASC_USE_CUDA
  ASC_VERIFY(!Allows(Backend::kCuda),
             "the CUDA backends require ASC built with ASC_USE_CUDA=YES");
#endif
#ifndef ASC_USE_OPENMP
  ASC_VERIFY(!Allows(Backend::kOmp),
             "the OpenMP backends require ASC built with"
             " ASC_USE_OPENMP=YES");
#endif
  if (Allows(Backend::kCuda)) {
    CudaDeviceSetup(dev_, ngpu_);
  }
}

MemoryType Device::QueryMemoryType(void *ptr) {
  // from HYPRE's hypre_GetPointerLocation
  MemoryType res = MemoryType::kHost;
#if defined(ASC_USE_CUDA)
  struct cudaPointerAttributes attr;

#if (kCudaRT_VERSION >= 11000)
  ASC_GPU_CHECK(cudaPointerGetAttributes(&attr, ptr));
#else
  cudaError_t err = cudaPointerGetAttributes(&attr, ptr);
  if (err != cudaSuccess) {
    /* clear the error */
    cudaGetLastError();
  }
#endif
  switch (attr.type) {
    case cudaMemoryTypeUnregistered:
      // host
      break;
    case cudaMemoryTypeDevice:
      res = MemoryType::kDevice;
      break;
    case cudaMemoryTypeManaged:
      res = MemoryType::kManaged;
      break;
  }

#else
  ASC_CONTRACT_VAR(ptr);
#endif
  return res;
}

void Device::DeviceMem(size_t *free, size_t *total) {
#if defined(ASC_USE_CUDA)
  cudaMemGetInfo(free, total);
#else
  // not compiled with GPU support
  if (free) {
    *free = 0;
  }
  if (*total) {
    *total = 0;
  }
#endif
}

int Device::NumMultiprocessors(int dev) {
#if defined(ASC_USE_CUDA)
  int res;
  cudaDeviceGetAttribute(&res, cudaDevAttrMultiProcessorCount, dev);
  return res;
#else
  // not compiled with GPU support
  ASC_CONTRACT_VAR(dev);
  return 0;
#endif
}

int Device::NumMultiprocessors() {
  int dev = 0;
#if defined(ASC_USE_CUDA)
  cudaGetDevice(&dev);
#endif
  return NumMultiprocessors(dev);
}

int Device::WarpSize(int dev) {
#if defined(ASC_USE_CUDA)
  int res;
  cudaDeviceGetAttribute(&res, cudaDevAttrWarpSize, dev);
  return res;
#else
  // not compiled with GPU support
  ASC_CONTRACT_VAR(dev);
  return 0;
#endif
}

int Device::WarpSize() {
  int dev = 0;
#if defined(ASC_USE_CUDA)
  cudaGetDevice(&dev);
#endif
  return WarpSize(dev);
}

}  // namespace asc
