cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    SOURCE_DIR
    GENERATOR
    WORK_DIR
    ASCCMAKE_DIR
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

set(_generator_arguments -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_arguments -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _generator_arguments -T "${GENERATOR_TOOLSET}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
  list(APPEND
    _generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}"
  )
endif()

set(_binary_dir "${WORK_DIR}/producer with CUDA forcibly undiscoverable")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${_binary_dir}"
    ${_generator_arguments}
    "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
    "-DBUILD_TESTING:BOOL=OFF"
    "-DASC_CPP_BUILD_TESTING:BOOL=OFF"
    "-DASC_CPP_ENABLE_CUDA:BOOL=OFF"
    "-DASC_CPP_INSTALL:BOOL=ON"
    "-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit:BOOL=TRUE"
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "CUDA-disabled isolated configuration failed.\n"
    "stdout:\n${_configure_stdout}\n"
    "stderr:\n${_configure_stderr}"
  )
endif()

file(READ "${_binary_dir}/CMakeCache.txt" _cache)
if(_cache MATCHES "(^|\n)CMAKE_CUDA_COMPILER:")
  message(FATAL_ERROR
    "CUDA-disabled configuration unexpectedly initialized CUDA language."
  )
endif()

set(_package_dir "${_binary_dir}/package/ASCCpp")
foreach(_provider_export IN ITEMS
    ASCCppCoreCudaTargets.cmake
    ASCCppDenseCudaTargets.cmake
    ASCCppSparseCudaTargets.cmake
    ASCCppRandomCudaTargets.cmake
    ASCCppRandomDenseCudaTargets.cmake
    ASCCppRandomSparseCudaTargets.cmake
)
  if(EXISTS "${_package_dir}/${_provider_export}")
    message(FATAL_ERROR
      "CUDA-disabled package unexpectedly exports ${_provider_export}."
    )
  endif()
endforeach()
file(READ "${_package_dir}/ASCCppConfig.cmake" _config)
if(NOT _config MATCHES
   "set\\(ASCCpp_AVAILABLE_COMPONENTS \"core;utilities;expression;dense;sparse;random;random_dense;random_sparse;cpp\"\\)")
  message(FATAL_ERROR
    "CUDA-disabled package advertises an unexpected component set."
  )
endif()
