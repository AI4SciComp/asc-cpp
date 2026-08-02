cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR BINARY_DIR ENABLE_CUDA)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

# Retired linalg interfaces may remain in historical migration prose and in
# negative compatibility tests, but never in product code or build logic.
set(_product_roots include src cmake benchmarks)
set(_product_files "${SOURCE_DIR}/CMakeLists.txt")
foreach(_root IN LISTS _product_roots)
  file(GLOB_RECURSE _root_files LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/${_root}/*.cc"
    "${SOURCE_DIR}/${_root}/*.cmake"
    "${SOURCE_DIR}/${_root}/*.cu"
    "${SOURCE_DIR}/${_root}/*.h"
    "${SOURCE_DIR}/${_root}/*.in"
    "${SOURCE_DIR}/${_root}/CMakeLists.txt"
  )
  list(APPEND _product_files ${_root_files})
endforeach()
foreach(_product_file IN LISTS _product_files)
  file(READ "${_product_file}" _product_contents)
  string(TOLOWER "${_product_contents}" _product_contents)
  string(FIND "${_product_contents}" "linalg" _linalg_position)
  if(NOT _linalg_position EQUAL -1)
    file(RELATIVE_PATH _relative_file "${SOURCE_DIR}" "${_product_file}")
    message(FATAL_ERROR
      "Retired linalg interface text remains in product code: "
      "${_relative_file}"
    )
  endif()
endforeach()

file(GLOB_RECURSE _ctest_files LIST_DIRECTORIES FALSE
  "${BINARY_DIR}/tests/*CTestTestfile.cmake"
)
if(NOT _ctest_files)
  message(FATAL_ERROR
    "No generated CTest inventory exists below ${BINARY_DIR}/tests."
  )
endif()
set(_ctest_inventory)
foreach(_ctest_file IN LISTS _ctest_files)
  file(READ "${_ctest_file}" _ctest_contents)
  string(APPEND _ctest_inventory "${_ctest_contents}\n")
endforeach()

set(_required_tests
  asc_cpp.architecture.dependency_manifest
  asc_cpp.architecture.approved_product_targets
  asc_cpp.architecture.public_file_policy
  asc_cpp.architecture.blas_coverage
  asc_cpp.architecture.blas_coverage_failures
  asc_cpp.architecture.blas_completion
  asc_cpp.dense.blas_level1_test
  asc_cpp.dense.blas_level2_test
  asc_cpp.dense.blas_level3_test
  asc_cpp.sparse.standard_blas_test
  asc_cpp.dense.benchmark
  asc_cpp.sparse.benchmark
  asc_cpp.package.build_tree_components
  asc_cpp.package.install_and_relocate_components
  asc_cpp.consumer.dense.build_tree
  asc_cpp.consumer.dense.install_relocate
  asc_cpp.consumer.sparse.build_tree
  asc_cpp.consumer.sparse.install_relocate
  asc_cpp.downstream.asc_xde.build_tree
  asc_cpp.downstream.asc_xde.relocated
  asc_cpp.hardening.documentation_consistency
)
if(ENABLE_CUDA)
  list(APPEND _required_tests
    asc_cpp.dense_cuda.dense_cuda_blas_level1_test
    asc_cpp.dense_cuda.dense_cuda_blas_level2_test
    asc_cpp.dense_cuda.dense_cuda_blas_level3_test
    asc_cpp.sparse_cuda.standard_blas
    asc_cpp.dense_cuda.benchmark
    asc_cpp.sparse_cuda.benchmark
    asc_cpp.consumer.dense_cuda.build_tree
    asc_cpp.consumer.dense_cuda.install_relocate
    asc_cpp.consumer.sparse_cuda.build_tree
    asc_cpp.consumer.sparse_cuda.install_relocate
  )
endif()

foreach(_required_test IN LISTS _required_tests)
  # CMake's generated CTest syntax varies by version: test names may be
  # bracket-quoted, ordinarily quoted, or unquoted. Match the registration
  # itself instead of depending on one serialization.
  set(_test_position -1)
  foreach(_test_registration IN ITEMS
      "add_test([=[${_required_test}]=]"
      "add_test(\"${_required_test}\""
      "add_test(${_required_test} "
  )
    string(FIND
      "${_ctest_inventory}" "${_test_registration}" _test_position
    )
    if(NOT _test_position EQUAL -1)
      break()
    endif()
  endforeach()
  if(_test_position EQUAL -1)
    message(FATAL_ERROR
      "BLAS completion evidence test is not registered: ${_required_test}"
    )
  endif()
endforeach()

foreach(_required_label IN ITEMS
    architecture benchmark consumer documentation package performance runtime
)
  string(FIND "${_ctest_inventory}" "${_required_label}" _label_position)
  if(_label_position EQUAL -1)
    message(FATAL_ERROR
      "BLAS completion CTest inventory omits '${_required_label}' coverage."
    )
  endif()
endforeach()

list(LENGTH _required_tests _required_test_count)
message(STATUS
  "BLAS completion audit found no product linalg interface and validated "
  "${_required_test_count} evidence registrations."
)
