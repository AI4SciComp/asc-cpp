cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PRODUCER_BINARY_DIR OR PRODUCER_BINARY_DIR STREQUAL "")
  message(FATAL_ERROR "PRODUCER_BINARY_DIR is required.")
endif()

set(_cache "${PRODUCER_BINARY_DIR}/CMakeCache.txt")
if(NOT EXISTS "${_cache}")
  message(FATAL_ERROR "Producer CMake cache does not exist: ${_cache}")
endif()

file(READ "${_cache}" _contents)
foreach(_forbidden IN ITEMS
    "CMAKE_CUDA_COMPILER:"
    "CMAKE_CUDA_HOST_COMPILER:"
    "CUDAToolkit_"
    "_CUDA_NVCC_EXECUTABLE:"
)
  if(_contents MATCHES "${_forbidden}")
    message(FATAL_ERROR
      "CUDA-disabled configuration leaked discovery state matching "
      "'${_forbidden}'."
    )
  endif()
endforeach()
