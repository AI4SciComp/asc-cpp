cmake_minimum_required(VERSION 3.25)

project(
  ASCCpp
  VERSION 0.9.0
  DESCRIPTION "AI4SciComp C++ scientific-computing foundation"
  LANGUAGES CXX
)

if(WIN32 AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppOptions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppComponents.cmake")

if(ASC_CPP_INTERNAL_ENABLE_CUDA_RUNTIME_TEST_HOOKS
   AND (NOT ASC_CPP_BUILD_TESTING OR NOT ASC_CPP_ENABLE_CUDA))
  message(FATAL_ERROR
    "ASC_CPP_INTERNAL_ENABLE_CUDA_RUNTIME_TEST_HOOKS requires "
    "ASC_CPP_BUILD_TESTING=ON and ASC_CPP_ENABLE_CUDA=ON."
  )
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/AcquireASCCMake.cmake")
if(ASC_CPP_ENABLE_LAPACK)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConfigureLapackProvider.cmake")
endif()

if(ASC_CPP_ENABLE_CUDA)
  set(CMAKE_CUDA_RUNTIME_LIBRARY Shared)
  include(CheckLanguage)
  set(_asc_cpp_restore_cuda_host_environment FALSE)
  if(DEFINED CMAKE_CUDA_HOST_COMPILER
     AND NOT "${CMAKE_CUDA_HOST_COMPILER}" STREQUAL "")
    if(DEFINED ENV{CUDAHOSTCXX}
       AND NOT "$ENV{CUDAHOSTCXX}" STREQUAL "${CMAKE_CUDA_HOST_COMPILER}")
      message(FATAL_ERROR
        "CMAKE_CUDA_HOST_COMPILER and CUDAHOSTCXX name different compilers."
      )
    elseif(NOT DEFINED ENV{CUDAHOSTCXX})
      set(ENV{CUDAHOSTCXX} "${CMAKE_CUDA_HOST_COMPILER}")
      set(_asc_cpp_restore_cuda_host_environment TRUE)
    endif()
  endif()
  check_language(CUDA)
  if(_asc_cpp_restore_cuda_host_environment)
    unset(ENV{CUDAHOSTCXX})
  endif()
  unset(_asc_cpp_restore_cuda_host_environment)
  if(NOT CMAKE_CUDA_COMPILER)
    message(FATAL_ERROR
      "ASC_CPP_ENABLE_CUDA=ON requires an available CUDA compiler."
    )
  endif()
  enable_language(CUDA)
  find_package(CUDAToolkit 12 REQUIRED)
  foreach(_cuda_target IN ITEMS CUDA::cudart CUDA::cublas CUDA::cusparse)
    if(NOT TARGET "${_cuda_target}")
      message(FATAL_ERROR
        "ASC_CPP_ENABLE_CUDA=ON requires CMake target '${_cuda_target}'."
      )
    endif()
  endforeach()
endif()

add_subdirectory(src/core)
add_subdirectory(src/utilities)
add_subdirectory(src/expression)
add_subdirectory(src/dense)
add_subdirectory(src/sparse)
add_subdirectory(src/random)

set(_asc_cpp_reproducible_source_root "ASCCpp-${PROJECT_VERSION}")
set(_asc_cpp_reproducible_build_root "ASCCpp-${PROJECT_VERSION}-build")
string(FIND "${PROJECT_SOURCE_DIR}" " " _asc_cpp_source_space)
string(FIND "${PROJECT_BINARY_DIR}" " " _asc_cpp_build_space)
foreach(_asc_cpp_compiled_target IN ITEMS
    asc_core
    asc_utilities
    asc_dense
    asc_dense_lapack
    asc_sparse
    asc_random
    asc_core_cuda
    asc_dense_cuda
    asc_sparse_cuda
    asc_random_cuda
    asc_random_dense_cuda
    asc_random_sparse_cuda
)
  if(TARGET "${_asc_cpp_compiled_target}")
    if(MSVC)
      target_compile_options(
        "${_asc_cpp_compiled_target}"
        PRIVATE
          "$<$<COMPILE_LANGUAGE:CXX>:/pathmap:${PROJECT_SOURCE_DIR}=${_asc_cpp_reproducible_source_root}>"
          "$<$<COMPILE_LANGUAGE:CXX>:/pathmap:${PROJECT_BINARY_DIR}=${_asc_cpp_reproducible_build_root}>"
      )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
      target_compile_options(
        "${_asc_cpp_compiled_target}"
        PRIVATE
          "$<$<COMPILE_LANGUAGE:CXX>:-ffile-prefix-map=${PROJECT_SOURCE_DIR}=${_asc_cpp_reproducible_source_root}>"
          "$<$<COMPILE_LANGUAGE:CXX>:-ffile-prefix-map=${PROJECT_BINARY_DIR}=${_asc_cpp_reproducible_build_root}>"
      )
    endif()
    if(CMAKE_CUDA_COMPILER_ID STREQUAL "NVIDIA")
      if(MSVC)
        if(_asc_cpp_source_space EQUAL -1)
          target_compile_options(
            "${_asc_cpp_compiled_target}"
            PRIVATE
              "$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/pathmap:${PROJECT_SOURCE_DIR}=${_asc_cpp_reproducible_source_root}>"
          )
        endif()
        if(_asc_cpp_build_space EQUAL -1)
          target_compile_options(
            "${_asc_cpp_compiled_target}"
            PRIVATE
              "$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/pathmap:${PROJECT_BINARY_DIR}=${_asc_cpp_reproducible_build_root}>"
          )
        endif()
      else()
        # NVCC splits a forwarded host option at spaces even when its argv
        # element is quoted.  Omit only the affected CUDA-language map; CXX
        # compilation still uses the quoted compiler option above.
        if(_asc_cpp_source_space EQUAL -1)
          target_compile_options(
            "${_asc_cpp_compiled_target}"
            PRIVATE
              "$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-ffile-prefix-map=${PROJECT_SOURCE_DIR}=${_asc_cpp_reproducible_source_root}>"
          )
        endif()
        if(_asc_cpp_build_space EQUAL -1)
          target_compile_options(
            "${_asc_cpp_compiled_target}"
            PRIVATE
              "$<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=-ffile-prefix-map=${PROJECT_BINARY_DIR}=${_asc_cpp_reproducible_build_root}>"
          )
        endif()
      endif()
    endif()
    set_target_properties(
      "${_asc_cpp_compiled_target}"
      PROPERTIES
        BUILD_RPATH_USE_ORIGIN ON
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    )
  endif()
endforeach()
unset(_asc_cpp_compiled_target)
unset(_asc_cpp_build_space)
unset(_asc_cpp_reproducible_build_root)
unset(_asc_cpp_reproducible_source_root)
unset(_asc_cpp_source_space)

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
target_compile_features(asc_cpp INTERFACE cxx_std_20)

set(ASC_CPP_AVAILABLE_COMPONENTS
  core
  utilities
  expression
  dense
  sparse
  random
  random_dense
  random_sparse
  cpp
)
if(ASC_CPP_ENABLE_LAPACK)
  list(APPEND ASC_CPP_AVAILABLE_COMPONENTS dense_lapack)
endif()
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
  TARGETS asc_dense
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppDenseTargets.cmake"
  NAMESPACE ASC::
)
export(
  TARGETS asc_sparse
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppSparseTargets.cmake"
  NAMESPACE ASC::
)
if(ASC_CPP_ENABLE_LAPACK)
  export(TARGETS asc_dense_lapack
    FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppDenseLapackTargets.cmake"
    NAMESPACE ASC::)
  configure_file("${ASC_CPP_LAPACK_METADATA}"
    "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppLapackProvider.json" COPYONLY)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppLapackDependency.cmake"
    "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppLapackDependency.cmake" COPYONLY)
endif()
export(
  TARGETS asc_random
  FILE "${ASC_CPP_BUILD_PACKAGE_DIR}/ASCCppRandomTargets.cmake"
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
  if(ASC_CPP_ENABLE_LAPACK)
    install(TARGETS asc_dense_lapack EXPORT ASCCppDenseLapackTargets
      RUNTIME DESTINATION "${_asc_cpp_install_bindir}"
      LIBRARY DESTINATION "${_asc_cpp_install_libdir}"
      ARCHIVE DESTINATION "${_asc_cpp_install_libdir}"
      FILE_SET HEADERS DESTINATION "${_asc_cpp_install_includedir}")
    install(EXPORT ASCCppDenseLapackTargets
      FILE ASCCppDenseLapackTargets.cmake NAMESPACE ASC::
      DESTINATION "${_asc_cpp_install_package_dir}")
    install(FILES "${ASC_CPP_LAPACK_METADATA}"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ASCCppLapackDependency.cmake"
      DESTINATION "${_asc_cpp_install_package_dir}")
  endif()
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
      "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES"
    DESTINATION "${_asc_cpp_install_docdir}"
  )
  install(
    FILES
      "${CMAKE_CURRENT_SOURCE_DIR}/data/random/LICENSE.joe-kuo"
      "${CMAKE_CURRENT_SOURCE_DIR}/data/random/new-joe-kuo-6.21201"
    DESTINATION "${_asc_cpp_install_docdir}/random"
  )
endif()

if(ASC_CPP_BUILD_TESTING)
  if(NOT BUILD_TESTING)
    message(FATAL_ERROR
      "ASC_CPP_BUILD_TESTING=ON requires BUILD_TESTING=ON."
    )
  endif()
  find_package(Threads REQUIRED)
  add_subdirectory(tests)
endif()

if(ASC_CPP_BUILD_DOCUMENTATION)
  add_subdirectory(docs)
endif()

unset(_asc_cpp_install_bindir)
unset(_asc_cpp_install_docdir)
unset(_asc_cpp_install_includedir)
unset(_asc_cpp_install_libdir)
unset(_asc_cpp_install_package_dir)
unset(_asc_cpp_install_staging_dir)
