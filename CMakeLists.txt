cmake_minimum_required(VERSION 3.25)

project(
  ASCCpp
  VERSION 0.9.0
  DESCRIPTION "AI4SciComp C++ scientific-computing foundation"
  LANGUAGES CXX
)

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppOptions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppComponents.cmake")

find_package(ASCCMake 0.1.0 EXACT CONFIG REQUIRED)
if(NOT ASCCMake_VERSION VERSION_EQUAL "0.1.0")
  message(FATAL_ERROR
    "Milestone 8 requires ASCCMake 0.1.0 exactly; found "
    "'${ASCCMake_VERSION}'."
  )
endif()

if(ASC_CPP_ENABLE_CUDA)
  include(CheckLanguage)
  check_language(CUDA)
  if(NOT CMAKE_CUDA_COMPILER)
    message(FATAL_ERROR
      "ASC_CPP_ENABLE_CUDA=ON requires an available CUDA compiler."
    )
  endif()
  enable_language(CUDA)
  # Every CUDA-enabled target and producer-side test must observe the same
  # runtime state as the approved CUDA::cudart dependency.  NVCC otherwise
  # defaults producer executables and shared libraries to cudart_static.
  set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)
  block(SCOPE_FOR VARIABLES)
    # CMake 3.25's FindCUDAToolkit unconditionally deprecates the
    # CUDA::nvToolsExt target after CUDA 12.9 removed that library. Suppress
    # only that module-side deprecation branch; keep standard toolkit
    # discovery and the project's actual minimum unchanged.
    if(CMAKE_VERSION VERSION_LESS 4.1)
      set(CMAKE_MINIMUM_REQUIRED_VERSION 3.24)
    endif()
    find_package(CUDAToolkit 12 REQUIRED)
  endblock()
  foreach(_asc_cpp_cuda_target IN ITEMS
      CUDA::cudart
      CUDA::cublas
      CUDA::cusparse
  )
    if(NOT TARGET "${_asc_cpp_cuda_target}")
      message(FATAL_ERROR
        "Milestone 8 requires CUDAToolkit target "
        "'${_asc_cpp_cuda_target}'."
      )
    endif()
  endforeach()
  unset(_asc_cpp_cuda_target)
endif()

add_subdirectory(src/core)
add_subdirectory(src/utilities)
add_subdirectory(src/expression)
add_subdirectory(src/random)
add_subdirectory(src/dense)
add_subdirectory(src/sparse)

add_library(asc_cpp INTERFACE)
add_library(ASC::cpp ALIAS asc_cpp)
set_target_properties(
  asc_cpp
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    EXPORT_NAME cpp
)
target_link_libraries(
  asc_cpp
  INTERFACE
    ASC::core
    ASC::utilities
    ASC::expression
    ASC::dense
    ASC::sparse
    ASC::random
    ASC::random_dense
    ASC::random_sparse
)
# Released ASCCMake v0.1.0 deliberately rejects interface libraries in
# asc_target_enable_cxx20. Use its exact standard-CMake equivalent.
target_compile_features(asc_cpp INTERFACE cxx_std_20)

set(ASC_CPP_AVAILABLE_COMPONENTS
  core
  utilities
  expression
  random
  dense
  sparse
  random_dense
  random_sparse
  cpp
)
if(ASC_CPP_ENABLE_CUDA)
  list(APPEND ASC_CPP_AVAILABLE_COMPONENTS
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()

include(CMakePackageConfigHelpers)
if(DEFINED CMAKE_INSTALL_LIBDIR AND NOT CMAKE_INSTALL_LIBDIR STREQUAL "")
  set(_asc_cpp_install_libdir "${CMAKE_INSTALL_LIBDIR}")
else()
  # Keep the package layout deterministic without depending on platform-
  # specific GNUInstallDirs inference.
  set(_asc_cpp_install_libdir "lib")
endif()
if(DEFINED CMAKE_INSTALL_DOCDIR AND NOT CMAKE_INSTALL_DOCDIR STREQUAL "")
  set(_asc_cpp_install_docdir "${CMAKE_INSTALL_DOCDIR}")
elseif(DEFINED CMAKE_INSTALL_DATAROOTDIR
       AND NOT CMAKE_INSTALL_DATAROOTDIR STREQUAL "")
  set(_asc_cpp_install_docdir "${CMAKE_INSTALL_DATAROOTDIR}/doc/ASCCpp")
else()
  set(_asc_cpp_install_docdir "share/doc/ASCCpp")
endif()
if(DEFINED CMAKE_INSTALL_BINDIR AND NOT CMAKE_INSTALL_BINDIR STREQUAL "")
  set(_asc_cpp_install_bindir "${CMAKE_INSTALL_BINDIR}")
else()
  set(_asc_cpp_install_bindir "bin")
endif()
if(DEFINED CMAKE_INSTALL_INCLUDEDIR
   AND NOT CMAKE_INSTALL_INCLUDEDIR STREQUAL "")
  set(_asc_cpp_install_includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(_asc_cpp_install_includedir "include")
endif()

set(ASC_CPP_BUILD_PACKAGE_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/package/ASCCpp"
  CACHE INTERNAL "ASCCpp build-tree package directory"
)
set(_asc_cpp_install_package_dir
  "${_asc_cpp_install_libdir}/cmake/ASCCpp"
)
set(_asc_cpp_install_staging_dir
  "${CMAKE_CURRENT_BINARY_DIR}/package/install"
)
file(MAKE_DIRECTORY
  "${ASC_CPP_BUILD_PACKAGE_DIR}"
  "${_asc_cpp_install_staging_dir}"
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppConfig.cmake.in"
  "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppConfig.cmake"
  INSTALL_DESTINATION "."
)
write_basic_package_version_file(
  "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMinorVersion
)
export(
  TARGETS asc_core
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppCoreTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_utilities
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppUtilitiesTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_expression
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppExpressionTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_random
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_dense
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppDenseTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_sparse
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppSparseTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_random_dense
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomDenseTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_random_sparse
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomSparseTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_cpp
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppCppTargets.cmake"
  NAMESPACE ASC::
)
if(ASC_CPP_ENABLE_CUDA)
  export(
    TARGETS asc_core_cuda
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppCoreCudaTargets.cmake"
    NAMESPACE ASC::
  )
  export(
    TARGETS asc_dense_cuda
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppDenseCudaTargets.cmake"
    NAMESPACE ASC::
  )
  export(
    TARGETS asc_sparse_cuda
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppSparseCudaTargets.cmake"
    NAMESPACE ASC::
  )
  export(
    TARGETS asc_random_cuda
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomCudaTargets.cmake"
    NAMESPACE ASC::
  )
  export(
    TARGETS asc_random_dense_cuda
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomDenseCudaTargets.cmake"
    NAMESPACE ASC::
  )
  export(
    TARGETS asc_random_sparse_cuda
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomSparseCudaTargets.cmake"
    NAMESPACE ASC::
  )
endif()

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppConfig.cmake.in"
  "${_asc_cpp_install_staging_dir}/ASCCppConfig.cmake"
  INSTALL_DESTINATION "${_asc_cpp_install_package_dir}"
)
write_basic_package_version_file(
  "${_asc_cpp_install_staging_dir}/ASCCppConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMinorVersion
)

if(ASC_CPP_INSTALL)
  install(
    TARGETS asc_core
    EXPORT ASCCppCoreTargets
    RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
    LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
    ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppCoreTargets
    FILE ASCCppCoreTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_utilities
    EXPORT ASCCppUtilitiesTargets
    RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
    LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
    ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppUtilitiesTargets
    FILE ASCCppUtilitiesTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_expression
    EXPORT ASCCppExpressionTargets
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppExpressionTargets
    FILE ASCCppExpressionTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_random
    EXPORT ASCCppRandomTargets
    RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
    LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
    ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppRandomTargets
    FILE ASCCppRandomTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_dense
    EXPORT ASCCppDenseTargets
    RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
    LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
    ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppDenseTargets
    FILE ASCCppDenseTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_sparse
    EXPORT ASCCppSparseTargets
    RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
    LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
    ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppSparseTargets
    FILE ASCCppSparseTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_random_dense
    EXPORT ASCCppRandomDenseTargets
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppRandomDenseTargets
    FILE ASCCppRandomDenseTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_random_sparse
    EXPORT ASCCppRandomSparseTargets
    FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
  )
  install(
    EXPORT ASCCppRandomSparseTargets
    FILE ASCCppRandomSparseTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    TARGETS asc_cpp
    EXPORT ASCCppCppTargets
  )
  install(
    EXPORT ASCCppCppTargets
    FILE ASCCppCppTargets.cmake
    NAMESPACE ASC::
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  if(ASC_CPP_ENABLE_CUDA)
    install(
      TARGETS asc_core_cuda
      EXPORT ASCCppCoreCudaTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
    )
    install(
      EXPORT ASCCppCoreCudaTargets
      FILE ASCCppCoreCudaTargets.cmake
      NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}"
    )
    install(
      TARGETS asc_dense_cuda
      EXPORT ASCCppDenseCudaTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
    )
    install(
      EXPORT ASCCppDenseCudaTargets
      FILE ASCCppDenseCudaTargets.cmake
      NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}"
    )
    install(
      TARGETS asc_sparse_cuda
      EXPORT ASCCppSparseCudaTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
    )
    install(
      EXPORT ASCCppSparseCudaTargets
      FILE ASCCppSparseCudaTargets.cmake
      NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}"
    )
    install(
      TARGETS asc_random_cuda
      EXPORT ASCCppRandomCudaTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
    )
    install(
      EXPORT ASCCppRandomCudaTargets
      FILE ASCCppRandomCudaTargets.cmake
      NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}"
    )
    install(
      TARGETS asc_random_dense_cuda
      EXPORT ASCCppRandomDenseCudaTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
    )
    install(
      EXPORT ASCCppRandomDenseCudaTargets
      FILE ASCCppRandomDenseCudaTargets.cmake
      NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}"
    )
    install(
      TARGETS asc_random_sparse_cuda
      EXPORT ASCCppRandomSparseCudaTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}"
    )
    install(
      EXPORT ASCCppRandomSparseCudaTargets
      FILE ASCCppRandomSparseCudaTargets.cmake
      NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}"
    )
  endif()
  install(
    FILES
      "${_asc_cpp_install_staging_dir}/ASCCppConfig.cmake"
      "${_asc_cpp_install_staging_dir}/ASCCppConfigVersion.cmake"
    DESTINATION "${_asc_cpp_install_package_dir}"
  )
  install(
    FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    DESTINATION "${_asc_cpp_install_docdir}"
  )
endif()

if(ASC_CPP_BUILD_TESTING)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR
      "ASC_CPP_BUILD_TESTING requires BUILD_TESTING=ON so CTest is enabled."
    )
  endif()
  add_subdirectory(tests)
endif()

unset(_asc_cpp_install_package_dir)
unset(_asc_cpp_install_staging_dir)
unset(_asc_cpp_install_docdir)
unset(_asc_cpp_install_bindir)
unset(_asc_cpp_install_includedir)
unset(_asc_cpp_install_libdir)
