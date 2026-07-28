if(CMAKE_VERSION VERSION_LESS 3.25)
  message(FATAL_ERROR "Milestone 8 tests require CMake 3.25 or newer.")
endif()

set(
  ASC_CPP_TEST_WORKSPACE_ROOT
  "${PROJECT_BINARY_DIR}/asc-cpp-test-workspaces"
  CACHE INTERNAL "Dedicated root for recursively cleaned ASCCpp test work"
)
set(
  ASC_CPP_TEST_WORKSPACE_GUARD
  "${ASC_CPP_TEST_WORKSPACE_ROOT}/.asc-cpp-test-workspace-root"
  CACHE INTERNAL "Guard proving the dedicated ASCCpp test workspace root"
)
file(MAKE_DIRECTORY "${ASC_CPP_TEST_WORKSPACE_ROOT}")
file(WRITE "${ASC_CPP_TEST_WORKSPACE_GUARD}"
  "ASCCpp dedicated test workspace root\n"
  "source=${PROJECT_SOURCE_DIR}\n"
  "binary=${PROJECT_BINARY_DIR}\n"
)
add_test(
  NAME asc_cpp.test_workspace.guard
  COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR:PATH=${PROJECT_SOURCE_DIR}"
    "-DBINARY_DIR:PATH=${PROJECT_BINARY_DIR}"
    "-DROOT:PATH=${ASC_CPP_TEST_WORKSPACE_ROOT}"
    "-DGUARD:FILEPATH=${ASC_CPP_TEST_WORKSPACE_GUARD}"
    -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_workspace_guard_test.cmake"
)
set_tests_properties(
  asc_cpp.test_workspace.guard
  PROPERTIES
    LABELS "architecture;security;hardening;milestone-8"
    RUN_SERIAL TRUE
)

set(
  _asc_cpp_target_inventory
  "${CMAKE_CURRENT_BINARY_DIR}/architecture/observed-targets-$<CONFIG>.txt"
)
set_property(
  GLOBAL
  PROPERTY ASC_CPP_M1_TARGET_INVENTORY
  "${_asc_cpp_target_inventory}"
)
set_property(
  GLOBAL
  PROPERTY ASC_CPP_M1_SOURCE_DIRECTORY
  "${PROJECT_SOURCE_DIR}"
)

function(_asc_cpp_collect_target_inventory directory output_variable)
  set(_records)

  get_property(
    _build_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS
  )
  foreach(_target IN LISTS _build_targets)
    get_target_property(_type "${_target}" TYPE)
    get_target_property(_imported "${_target}" IMPORTED)
    list(APPEND _records "build|${_target}|${_type}|${_imported}")
  endforeach()

  get_property(
    _imported_targets DIRECTORY "${directory}" PROPERTY IMPORTED_TARGETS
  )
  foreach(_target IN LISTS _imported_targets)
    get_target_property(_type "${_target}" TYPE)
    get_target_property(_imported "${_target}" IMPORTED)
    list(APPEND _records "imported|${_target}|${_type}|${_imported}")
  endforeach()

  get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
  foreach(_subdirectory IN LISTS _subdirectories)
    _asc_cpp_collect_target_inventory("${_subdirectory}" _child_records)
    list(APPEND _records ${_child_records})
  endforeach()

  set("${output_variable}" "${_records}" PARENT_SCOPE)
endfunction()

function(_asc_cpp_write_target_inventory)
  get_property(_source_directory GLOBAL PROPERTY ASC_CPP_M1_SOURCE_DIRECTORY)
  get_property(_output_file GLOBAL PROPERTY ASC_CPP_M1_TARGET_INVENTORY)
  _asc_cpp_collect_target_inventory("${_source_directory}" _records)

  set(_known_product_targets
    asc_core
    asc_utilities
    asc_expression
    asc_dense
    asc_sparse
    asc_random
    asc_random_dense
    asc_random_sparse
    asc_cpp
    asc_core_cuda
    asc_dense_cuda
    asc_sparse_cuda
    asc_random_cuda
    asc_random_dense_cuda
    asc_random_sparse_cuda
    ASC::core
    ASC::utilities
    ASC::expression
    ASC::dense
    ASC::sparse
    ASC::random
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
    ASC::core_cuda
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
  )
  foreach(_target IN LISTS _known_product_targets)
    if(TARGET "${_target}")
      get_target_property(_type "${_target}" TYPE)
      get_target_property(_imported "${_target}" IMPORTED)
      list(APPEND _records "known-product|${_target}|${_type}|${_imported}")
    endif()
  endforeach()

  list(REMOVE_DUPLICATES _records)
  list(SORT _records)
  string(REPLACE ";" "\n" _contents "${_records}")
  if(_contents)
    string(APPEND _contents "\n")
  endif()
  file(GENERATE OUTPUT "${_output_file}" CONTENT "${_contents}")
endfunction()

cmake_language(
  DEFER
  DIRECTORY "${PROJECT_SOURCE_DIR}"
  CALL _asc_cpp_write_target_inventory
)

add_subdirectory(architecture)
add_subdirectory(compile)
add_subdirectory(core)
add_subdirectory(utilities)
add_subdirectory(expression)
add_subdirectory(dense)
add_subdirectory(sparse)
add_subdirectory(random)
add_subdirectory(random_dense)
add_subdirectory(random_sparse)
add_subdirectory(consumer)
add_subdirectory(package)
add_subdirectory(hardening)
add_subdirectory(downstream)
if(ASC_CPP_ENABLE_CUDA)
  add_subdirectory(core_cuda)
  add_subdirectory(dense_cuda)
  add_subdirectory(sparse_cuda)
  add_subdirectory(random_cuda)
  add_subdirectory(random_dense_cuda)
  add_subdirectory(random_sparse_cuda)
endif()

unset(_asc_cpp_target_inventory)
