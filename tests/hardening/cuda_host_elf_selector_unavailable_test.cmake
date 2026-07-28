cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR CMAKE_VERSION_UNDER_TEST)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(CMAKE_VERSION_UNDER_TEST VERSION_GREATER_EQUAL "3.31")
  message(FATAL_ERROR
    "The unavailable-identity regression applies only to CMake before 3.31"
  )
endif()
if(NOT "${CUDA_HOST_COMPILER_ID}" STREQUAL ""
   OR NOT "${CUDA_HOST_COMPILER_VERSION}" STREQUAL "")
  message(FATAL_ERROR
    "CMake ${CMAKE_VERSION_UNDER_TEST} unexpectedly supplied an exact CUDA "
    "host compiler identity"
  )
endif()
if(NOT "${SELECTED_BASELINE}" STREQUAL "")
  message(FATAL_ERROR
    "A CUDA ELF baseline was enforced without an exact host identity: "
    "${SELECTED_BASELINE}"
  )
endif()

if(NOT "${RAW_CUDA_HOST_COMPILER_ID}" STREQUAL ""
   OR NOT "${RAW_CUDA_HOST_COMPILER_VERSION}" STREQUAL "")
  message(STATUS
    "Ignoring untrusted pre-3.31 CUDA host identity cache values: "
    "${RAW_CUDA_HOST_COMPILER_ID} ${RAW_CUDA_HOST_COMPILER_VERSION}"
  )
endif()

include("${SOURCE_DIR}/tests/hardening/SelectElfBaseline.cmake")
asc_cpp_select_elf_baseline(
  OUTPUT_VARIABLE _baseline
  BASELINE_DIRECTORY "${SOURCE_DIR}/abi"
  SYSTEM_NAME Linux
  PROCESSOR x86_64
  DISTRIBUTION "Ubuntu 22.04.5 LTS"
  CXX_COMPILER_ID GNU
  CXX_COMPILER_VERSION 11.4.0
  STANDARD_LIBRARY libstdc++
  STANDARD_LIBRARY_IDENTITY
    "libstdc++:libstdc++.so.6.0.30:sha256=ff0825e113603c3866680d5d52216bc6d8eedf3a59f52a0aef67ff01994db128"
  C_LIBRARY_IDENTITY "ldd (Ubuntu GLIBC 2.35-0ubuntu3.13) 2.35"
  BUILD_TYPE Release
  SHARED TRUE
  CUDA_ENABLED TRUE
  CUDA_COMPILER_ID NVIDIA
  CUDA_COMPILER_VERSION 12.9.86
  CUDA_HOST_COMPILER_ID "${CUDA_HOST_COMPILER_ID}"
  CUDA_HOST_COMPILER_VERSION "${CUDA_HOST_COMPILER_VERSION}"
  CUDA_ARCHITECTURES 86
  READELF_IDENTITY "GNU readelf (GNU Binutils for Ubuntu) 2.38"
  NM_IDENTITY "GNU nm (GNU Binutils for Ubuntu) 2.38"
  CXXFILT_IDENTITY "GNU c++filt (GNU Binutils for Ubuntu) 2.38"
  MULTI_CONFIG FALSE
)
if(NOT "${_baseline}" STREQUAL "")
  message(FATAL_ERROR
    "The selector enforced a CUDA baseline without an exact host identity"
  )
endif()
