cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR TARGET_INVENTORY)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()
if(NOT EXISTS "${TARGET_INVENTORY}")
  message(FATAL_ERROR "Target inventory was not generated: ${TARGET_INVENTORY}")
endif()

set(_approved_targets
  ASC::core
  ASC::dense
  ASC::expression
  ASC::cpp
  ASC::random
  ASC::random_dense
  ASC::random_sparse
  ASC::sparse
  ASC::utilities
  asc_core
  asc_dense
  asc_expression
  asc_cpp
  asc_random
  asc_random_dense
  asc_random_sparse
  asc_sparse
  asc_utilities
)
if(ENABLE_CUDA)
  list(APPEND
    _approved_targets
    ASC::core_cuda
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
    asc_core_cuda
    asc_dense_cuda
    asc_sparse_cuda
    asc_random_cuda
    asc_random_dense_cuda
    asc_random_sparse_cuda
  )
endif()
file(STRINGS "${TARGET_INVENTORY}" _target_records ENCODING UTF-8)
set(_observed_product_targets)
foreach(_record IN LISTS _target_records)
  if(NOT _record MATCHES "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$")
    message(FATAL_ERROR "Malformed target inventory record: ${_record}")
  endif()
  set(_category "${CMAKE_MATCH_1}")
  set(_name "${CMAKE_MATCH_2}")
  set(_type "${CMAKE_MATCH_3}")
  set(_imported "${CMAKE_MATCH_4}")
  if(_category STREQUAL "known-product")
    list(APPEND _observed_product_targets "${_name}")
    if(NOT _name IN_LIST _approved_targets)
      message(FATAL_ERROR "Milestone 8 exposes forbidden target '${_name}'.")
    endif()
    if(_name MATCHES "(expression|random_dense|random_sparse|cpp)$")
      if(NOT _type STREQUAL "INTERFACE_LIBRARY")
        message(FATAL_ERROR
          "Milestone 8 interface target has an invalid type: "
          "${_name} (${_type})."
        )
      endif()
    elseif(NOT _type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY)$")
      message(FATAL_ERROR
        "Milestone 8 compiled target has invalid type: ${_name} (${_type})."
      )
    endif()
    if(_imported)
      message(FATAL_ERROR "Build-tree product target is unexpectedly imported.")
    endif()
  endif()
endforeach()

list(REMOVE_DUPLICATES _observed_product_targets)
list(SORT _observed_product_targets)
list(SORT _approved_targets)
if(NOT _observed_product_targets STREQUAL _approved_targets)
  message(FATAL_ERROR
    "Milestone 8 product targets differ from the frozen contract.\n"
    "Expected: ${_approved_targets}\n"
    "Observed: ${_observed_product_targets}"
  )
endif()

set(_cmake_sources "${SOURCE_DIR}/CMakeLists.txt")
file(
  GLOB_RECURSE _cmake_modules
  LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/cmake/*.cmake"
  "${SOURCE_DIR}/cmake/*.cmake.in"
)
list(APPEND _cmake_sources ${_cmake_modules})
foreach(_cmake_source IN LISTS _cmake_sources)
  file(READ "${_cmake_source}" _contents)
  string(REGEX REPLACE "#[^\n]*" "" _without_comments "${_contents}")
  if(_without_comments MATCHES
     "(^|[\n\r])[ \t]*export[ \t\r\n]*\\([ \t\r\n]*PACKAGE([ \t\r\n]|\\))")
    message(FATAL_ERROR
      "Package-registry mutation is prohibited: ${_cmake_source}."
    )
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
string(REGEX MATCH "project\\([^)]*\\)" _project_declaration "${_root_cmake}")
string(
  REGEX REPLACE "[ \t\r\n]+" " "
  _project_declaration "${_project_declaration}"
)
if(NOT _project_declaration MATCHES "^project\\( ASCCpp( |\\))"
   OR NOT _project_declaration MATCHES " VERSION 0\\.9\\.0( |\\))"
   OR NOT _project_declaration MATCHES " LANGUAGES CXX( |\\))")
  message(FATAL_ERROR
    "Milestone 8 must declare project ASCCpp 0.9.0 with LANGUAGES CXX."
  )
endif()
