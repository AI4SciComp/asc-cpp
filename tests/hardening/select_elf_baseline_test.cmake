cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/SelectElfBaseline.cmake")

set(_common
  BASELINE_DIRECTORY "/baseline"
  SYSTEM_NAME Linux
  PROCESSOR x86_64
  DISTRIBUTION "Ubuntu 22.04.5 LTS"
  STANDARD_LIBRARY libstdc++
  STANDARD_LIBRARY_IDENTITY
    "libstdc++:libstdc++.so.6.0.30:sha256=ff0825e113603c3866680d5d52216bc6d8eedf3a59f52a0aef67ff01994db128"
  C_LIBRARY_IDENTITY "ldd (Ubuntu GLIBC 2.35-0ubuntu3.13) 2.35"
  BUILD_TYPE Debug
  SHARED TRUE
  CUDA_ENABLED FALSE
  CXX_COMPILER_ID GNU
  CXX_COMPILER_VERSION 11.4.0
  READELF_IDENTITY "GNU readelf (GNU Binutils for Ubuntu) 2.38"
  NM_IDENTITY "GNU nm (GNU Binutils for Ubuntu) 2.38"
  CXXFILT_IDENTITY "GNU c++filt (GNU Binutils for Ubuntu) 2.38"
  MULTI_CONFIG FALSE
)

function(_expect_baseline name expected)
  asc_cpp_select_elf_baseline(
    OUTPUT_VARIABLE _actual
    ${ARGN}
  )
  if(NOT _actual STREQUAL expected)
    message(FATAL_ERROR
      "${name}: expected baseline '${expected}', got '${_actual}'"
    )
  endif()
endfunction()

_expect_baseline(
  exact_gcc
  "/baseline/linux-x86_64-gcc11-cpu-shared.txt"
  ${_common}
)

set(_clang ${_common})
list(TRANSFORM _clang REPLACE "^GNU$" "Clang")
list(TRANSFORM _clang REPLACE "^11\\.4\\.0$" "19.0.0")
_expect_baseline(
  exact_clang
  "/baseline/linux-x86_64-clang19-cpu-shared.txt"
  ${_clang}
)

foreach(_case IN ITEMS
    "aarch64;PROCESSOR;aarch64"
    "non_linux;SYSTEM_NAME;FreeBSD"
    "distribution_patch;DISTRIBUTION;Ubuntu 22.04.4 LTS"
    "libcxx;STANDARD_LIBRARY;libc++"
    "stdlib_patch;STANDARD_LIBRARY_IDENTITY;libstdc++:different"
    "glibc_patch;C_LIBRARY_IDENTITY;ldd (Ubuntu GLIBC 2.35) 2.35"
    "gcc_patch;CXX_COMPILER_VERSION;11.4.1"
    "multi_config;MULTI_CONFIG;TRUE"
    "different_tools;NM_IDENTITY;GNU nm (GNU Binutils) 2.40"
)
  list(GET _case 0 _name)
  list(GET _case 1 _key)
  list(GET _case 2 _value)
  set(_arguments ${_common})
  list(FIND _arguments "${_key}" _key_index)
  math(EXPR _value_index "${_key_index} + 1")
  list(REMOVE_AT _arguments "${_value_index}")
  list(INSERT _arguments "${_value_index}" "${_value}")
  _expect_baseline("${_name}" "" ${_arguments})
endforeach()

set(_cuda ${_common})
foreach(_replacement IN ITEMS
    "BUILD_TYPE;Release"
    "CUDA_ENABLED;TRUE"
)
  list(GET _replacement 0 _key)
  list(GET _replacement 1 _value)
  list(FIND _cuda "${_key}" _key_index)
  math(EXPR _value_index "${_key_index} + 1")
  list(REMOVE_AT _cuda "${_value_index}")
  list(INSERT _cuda "${_value_index}" "${_value}")
endforeach()
list(APPEND _cuda
  CUDA_COMPILER_ID NVIDIA
  CUDA_COMPILER_VERSION 12.9.86
  CUDA_HOST_COMPILER_ID GNU
  CUDA_HOST_COMPILER_VERSION 11.4.0
  CUDA_ARCHITECTURES 86
)
_expect_baseline(
  exact_cuda
  "/baseline/linux-x86_64-gcc11-cuda12-shared.txt"
  ${_cuda}
)

foreach(_case IN ITEMS
    "cuda_toolkit;CUDA_COMPILER_VERSION;12.9.85"
    "cuda_host;CUDA_HOST_COMPILER_VERSION;11.4.1"
    "cuda_arch;CUDA_ARCHITECTURES;80"
)
  list(GET _case 0 _name)
  list(GET _case 1 _key)
  list(GET _case 2 _value)
  set(_arguments ${_cuda})
  list(FIND _arguments "${_key}" _key_index)
  math(EXPR _value_index "${_key_index} + 1")
  list(REMOVE_AT _arguments "${_value_index}")
  list(INSERT _arguments "${_value_index}" "${_value}")
  _expect_baseline("${_name}" "" ${_arguments})
endforeach()
