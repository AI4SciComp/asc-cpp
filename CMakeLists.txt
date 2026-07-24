cmake_minimum_required(VERSION 3.25)

project(
  ASCCpp
  VERSION 0.1.0
  DESCRIPTION "C++20 foundations for AI4SciComp"
  LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  set(_asc_cpp_build_testing_default "${BUILD_TESTING}")
else()
  set(_asc_cpp_build_testing_default OFF)
endif()

option(
  ASC_CPP_BUILD_TESTING
  "Build the asc-cpp test suite"
  "${_asc_cpp_build_testing_default}"
)
option(ASC_CPP_BUILD_EXAMPLES "Build asc-cpp examples" ${PROJECT_IS_TOP_LEVEL})
option(ASC_CPP_ENABLE_ASSERTIONS "Enable ASC runtime assertions" ON)
option(ASC_CPP_ENABLE_EXCEPTIONS "Throw exceptions for recoverable ASC errors" ON)
option(ASC_CPP_ENABLE_OPENMP "Enable the OpenMP execution backend" OFF)
option(ASC_CPP_ENABLE_CUDA "Enable CUDA-aware APIs" OFF)
option(ASC_CPP_ENABLE_EIGEN "Enable Eigen interoperability" OFF)
option(ASC_CPP_ENABLE_MKL "Enable Eigen's Intel MKL/Pardiso adapters" OFF)
option(ASC_CPP_WARNINGS_AS_ERRORS "Treat asc-cpp warnings as errors" OFF)
option(ASC_CPP_ENABLE_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
set(ASC_CPP_PRECISION "double" CACHE STRING "Scalar precision: single or double")
set_property(CACHE ASC_CPP_PRECISION PROPERTY STRINGS single double)
if(ASC_CPP_PRECISION STREQUAL "single")
  set(ASC_USE_SINGLE ON)
  set(ASC_USE_DOUBLE OFF)
elseif(ASC_CPP_PRECISION STREQUAL "double")
  set(ASC_USE_SINGLE OFF)
  set(ASC_USE_DOUBLE ON)
else()
  message(FATAL_ERROR "ASC_CPP_PRECISION must be 'single' or 'double'")
endif()
set(ASC_DEBUG "${ASC_CPP_ENABLE_ASSERTIONS}")
set(ASC_USE_EXCEPTION "${ASC_CPP_ENABLE_EXCEPTIONS}")
set(ASC_USE_OPENMP "${ASC_CPP_ENABLE_OPENMP}")
set(ASC_USE_CUDA "${ASC_CPP_ENABLE_CUDA}")
set(ASC_USE_EIGEN "${ASC_CPP_ENABLE_EIGEN}")
set(ASC_USE_MKL "${ASC_CPP_ENABLE_MKL}")
if(ASC_CPP_ENABLE_MKL AND NOT ASC_CPP_ENABLE_EIGEN)
  message(FATAL_ERROR "ASC_CPP_ENABLE_MKL requires ASC_CPP_ENABLE_EIGEN")
endif()
if(ASC_CPP_ENABLE_CUDA)
  if(NOT CMAKE_CUDA_HOST_COMPILER)
    set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_CXX_COMPILER}")
  endif()
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 20)
  set(CMAKE_CUDA_STANDARD_REQUIRED ON)
  set(CMAKE_CUDA_EXTENSIONS OFF)
endif()
unset(_asc_cpp_build_testing_default)
set(
  ASC_CPP_ASCCMAKE_SOURCE_DIR ""
  CACHE PATH "Source tree fallback for asc-cmake when ASCCMake is not installed"
)

find_package(ASCCMake 0.1 CONFIG QUIET)
if(NOT ASCCMake_FOUND)
  if(NOT ASC_CPP_ASCCMAKE_SOURCE_DIR)
    get_filename_component(
      _asc_cpp_sibling_asc_cmake "${CMAKE_CURRENT_SOURCE_DIR}/../asc-cmake" ABSOLUTE
    )
    if(EXISTS "${_asc_cpp_sibling_asc_cmake}/modules/ASCProjectOptions.cmake")
      set(ASC_CPP_ASCCMAKE_SOURCE_DIR "${_asc_cpp_sibling_asc_cmake}")
    endif()
  endif()
  if(NOT EXISTS "${ASC_CPP_ASCCMAKE_SOURCE_DIR}/modules/ASCProjectOptions.cmake")
    message(FATAL_ERROR
      "asc-cpp requires asc-cmake 0.1. Install ASCCMake, set ASCCMake_DIR, "
      "or set ASC_CPP_ASCCMAKE_SOURCE_DIR to an asc-cmake source tree."
    )
  endif()
  include("${ASC_CPP_ASCCMAKE_SOURCE_DIR}/modules/ASCProjectOptions.cmake")
  include("${ASC_CPP_ASCCMAKE_SOURCE_DIR}/modules/ASCInstallPackage.cmake")
  include("${ASC_CPP_ASCCMAKE_SOURCE_DIR}/modules/ASCTesting.cmake")
endif()

set(_asc_cpp_project_option_arguments WARNINGS)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  list(APPEND _asc_cpp_project_option_arguments WARNINGS_AS_ERRORS)
endif()
if(ASC_CPP_ENABLE_SANITIZERS)
  list(APPEND _asc_cpp_project_option_arguments SANITIZERS ADDRESS UNDEFINED)
endif()
asc_add_project_options(
  TARGET asc_cpp_project_options
  ${_asc_cpp_project_option_arguments}
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/dependencies.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppTargetHelpers.cmake")

# Configuration and components own their generated files, sources, public
# headers, direct dependencies, and backend-specific usage requirements.
add_subdirectory(config)
add_subdirectory(src)

asc_install_package(
  PACKAGE ASCCpp
  EXPORT ASCCppTargets
  NAMESPACE ASC::
  VERSION "${PROJECT_VERSION}"
  TARGETS asc_core asc_utilities asc_array asc_linalg asc_random asc_cpp
  CONFIG_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppConfig.cmake.in"
)

include(GNUInstallDirs)
install(
  FILES LICENSE README.md CHANGELOG.md CONTRIBUTING.md THIRD_PARTY_NOTICES
  DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)
install(DIRECTORY docs DESTINATION "${CMAKE_INSTALL_DOCDIR}")

if(ASC_CPP_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ASC_CPP_BUILD_TESTING)
  include(CTest)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR "ASC_CPP_BUILD_TESTING requires BUILD_TESTING=ON")
  endif()
  add_subdirectory(tests)
endif()
