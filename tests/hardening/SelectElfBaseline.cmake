include_guard(GLOBAL)

function(asc_cpp_select_elf_baseline)
  set(_one_value
    OUTPUT_VARIABLE
    BASELINE_DIRECTORY
    SYSTEM_NAME
    PROCESSOR
    DISTRIBUTION
    CXX_COMPILER_ID
    CXX_COMPILER_VERSION
    STANDARD_LIBRARY
    STANDARD_LIBRARY_IDENTITY
    C_LIBRARY_IDENTITY
    BUILD_TYPE
    SHARED
    CUDA_ENABLED
    CUDA_COMPILER_ID
    CUDA_COMPILER_VERSION
    CUDA_HOST_COMPILER_ID
    CUDA_HOST_COMPILER_VERSION
    CUDA_ARCHITECTURES
    READELF_IDENTITY
    NM_IDENTITY
    CXXFILT_IDENTITY
    MULTI_CONFIG
  )
  cmake_parse_arguments(ENV "" "${_one_value}" "" ${ARGN})
  foreach(_required IN ITEMS OUTPUT_VARIABLE BASELINE_DIRECTORY)
    if(NOT DEFINED ENV_${_required}
       OR "${ENV_${_required}}" STREQUAL "")
      message(FATAL_ERROR
        "asc_cpp_select_elf_baseline requires ${_required}"
      )
    endif()
  endforeach()

  set(_baseline)
  if(NOT ENV_SYSTEM_NAME STREQUAL "Linux"
     OR NOT ENV_PROCESSOR STREQUAL "x86_64"
     OR NOT ENV_DISTRIBUTION STREQUAL "Ubuntu 22.04.5 LTS"
     OR NOT ENV_STANDARD_LIBRARY STREQUAL "libstdc++"
     OR NOT ENV_STANDARD_LIBRARY_IDENTITY STREQUAL
            "libstdc++:libstdc++.so.6.0.30:sha256=ff0825e113603c3866680d5d52216bc6d8eedf3a59f52a0aef67ff01994db128"
     OR NOT ENV_C_LIBRARY_IDENTITY STREQUAL
            "ldd (Ubuntu GLIBC 2.35-0ubuntu3.14) 2.35"
     OR NOT ENV_SHARED
     OR ENV_MULTI_CONFIG
     OR NOT ENV_READELF_IDENTITY MATCHES
            "^GNU readelf \\(GNU Binutils for Ubuntu\\) 2\\.38$"
     OR NOT ENV_NM_IDENTITY MATCHES
            "^GNU nm \\(GNU Binutils for Ubuntu\\) 2\\.38$"
     OR NOT ENV_CXXFILT_IDENTITY MATCHES
            "^GNU c\\+\\+filt \\(GNU Binutils for Ubuntu\\) 2\\.38$")
    set("${ENV_OUTPUT_VARIABLE}" "" PARENT_SCOPE)
    return()
  endif()

  if(NOT ENV_CUDA_ENABLED
     AND ENV_BUILD_TYPE STREQUAL "Debug"
     AND ENV_CXX_COMPILER_ID STREQUAL "GNU"
     AND ENV_CXX_COMPILER_VERSION VERSION_EQUAL "11.4.0")
    set(_baseline
      "${ENV_BASELINE_DIRECTORY}/linux-x86_64-gcc11-cpu-shared.txt"
    )
  elseif(NOT ENV_CUDA_ENABLED
         AND ENV_BUILD_TYPE STREQUAL "Debug"
         AND ENV_CXX_COMPILER_ID MATCHES "Clang"
         AND ENV_CXX_COMPILER_VERSION VERSION_EQUAL "19.0.0")
    set(_baseline
      "${ENV_BASELINE_DIRECTORY}/linux-x86_64-clang19-cpu-shared.txt"
    )
  elseif(ENV_CUDA_ENABLED
         AND ENV_BUILD_TYPE STREQUAL "Release"
         AND ENV_CXX_COMPILER_ID STREQUAL "GNU"
         AND ENV_CXX_COMPILER_VERSION VERSION_EQUAL "11.4.0"
         AND ENV_CUDA_COMPILER_ID STREQUAL "NVIDIA"
         AND ENV_CUDA_COMPILER_VERSION VERSION_EQUAL "12.9.86"
         AND ENV_CUDA_HOST_COMPILER_ID STREQUAL "GNU"
         AND ENV_CUDA_HOST_COMPILER_VERSION VERSION_EQUAL "11.4.0"
         AND ENV_CUDA_ARCHITECTURES STREQUAL "86")
    set(_baseline
      "${ENV_BASELINE_DIRECTORY}/linux-x86_64-gcc11-cuda12-shared.txt"
    )
  endif()

  set("${ENV_OUTPUT_VARIABLE}" "${_baseline}" PARENT_SCOPE)
endfunction()
