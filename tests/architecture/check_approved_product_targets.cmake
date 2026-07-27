cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR TARGET_INVENTORY ENABLE_CUDA)
  if(NOT DEFINED "${_required_variable}" OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()
if(NOT EXISTS "${TARGET_INVENTORY}")
  message(FATAL_ERROR "Target inventory was not generated: ${TARGET_INVENTORY}")
endif()

file(STRINGS "${TARGET_INVENTORY}" _target_records ENCODING UTF-8)
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
set(_allowed_product_targets
  asc_core
  asc_utilities
  asc_expression
  asc_dense
  asc_sparse
  asc_random
  asc_random_dense
  asc_random_sparse
  asc_cpp
  ASC::core
  ASC::utilities
  ASC::expression
  ASC::dense
  ASC::sparse
  ASC::random
  ASC::random_dense
  ASC::random_sparse
  ASC::cpp
)
if(ENABLE_CUDA)
  list(APPEND _allowed_product_targets
    asc_core_cuda
    asc_dense_cuda
    asc_sparse_cuda
    asc_random_cuda
    asc_random_dense_cuda
    asc_random_sparse_cuda
    ASC::core_cuda
    ASC::dense_cuda
    ASC::sparse_cuda
    ASC::random_cuda
    ASC::random_dense_cuda
    ASC::random_sparse_cuda
  )
endif()
set(_observed_asc_core FALSE)
set(_observed_asc_core_alias FALSE)
set(_observed_asc_utilities FALSE)
set(_observed_asc_utilities_alias FALSE)
set(_observed_asc_expression FALSE)
set(_observed_asc_expression_alias FALSE)
set(_observed_asc_dense FALSE)
set(_observed_asc_dense_alias FALSE)
set(_observed_asc_sparse FALSE)
set(_observed_asc_sparse_alias FALSE)
set(_observed_asc_random FALSE)
set(_observed_asc_random_alias FALSE)
set(_observed_asc_random_dense FALSE)
set(_observed_asc_random_dense_alias FALSE)
set(_observed_asc_random_sparse FALSE)
set(_observed_asc_random_sparse_alias FALSE)
set(_observed_asc_cpp FALSE)
set(_observed_asc_cpp_alias FALSE)
set(_observed_asc_core_cuda FALSE)
set(_observed_asc_core_cuda_alias FALSE)
set(_observed_asc_dense_cuda FALSE)
set(_observed_asc_dense_cuda_alias FALSE)
set(_observed_asc_sparse_cuda FALSE)
set(_observed_asc_sparse_cuda_alias FALSE)
set(_observed_asc_random_cuda FALSE)
set(_observed_asc_random_cuda_alias FALSE)
set(_observed_asc_random_dense_cuda FALSE)
set(_observed_asc_random_dense_cuda_alias FALSE)
set(_observed_asc_random_sparse_cuda FALSE)
set(_observed_asc_random_sparse_cuda_alias FALSE)

foreach(_record IN LISTS _target_records)
  if(NOT _record MATCHES "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$")
    message(FATAL_ERROR "Malformed target inventory record: ${_record}")
  endif()
  set(_kind "${CMAKE_MATCH_1}")
  set(_name "${CMAKE_MATCH_2}")
  set(_type "${CMAKE_MATCH_3}")
  set(_imported "${CMAKE_MATCH_4}")

  if(_name IN_LIST _known_product_targets
     AND NOT _name IN_LIST _allowed_product_targets)
    message(FATAL_ERROR
      "Milestone 8 exposes an unapproved product target "
      "(${_kind}, ${_type}): ${_name}"
    )
  endif()
  if(_name MATCHES "^ASC::" AND NOT _name IN_LIST _allowed_product_targets)
    message(FATAL_ERROR
      "Milestone 8 exposes an unapproved ASC namespace target: ${_name}"
    )
  endif()
  if(_kind STREQUAL "build"
     AND _name MATCHES "^asc_"
     AND _type MATCHES "^(STATIC|SHARED|MODULE|OBJECT|INTERFACE|UNKNOWN)_LIBRARY$"
     AND NOT _name IN_LIST _allowed_product_targets)
    message(FATAL_ERROR
      "Milestone 8 declares an unapproved ASCCpp library: "
      "${_name} (${_type})"
    )
  endif()
  if(_name STREQUAL "asc_core")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "asc_core has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR "The build-tree asc_core target must not be imported.")
    endif()
    set(_observed_asc_core TRUE)
  elseif(_name STREQUAL "ASC::core")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "ASC::core has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR "The build-tree ASC::core alias must not be imported.")
    endif()
    set(_observed_asc_core_alias TRUE)
  elseif(_name STREQUAL "asc_utilities")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "asc_utilities has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_utilities target must not be imported."
      )
    endif()
    set(_observed_asc_utilities TRUE)
  elseif(_name STREQUAL "ASC::utilities")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "ASC::utilities has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::utilities alias must not be imported."
      )
    endif()
    set(_observed_asc_utilities_alias TRUE)
  elseif(_name STREQUAL "asc_expression")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR "asc_expression has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_expression target must not be imported."
      )
    endif()
    set(_observed_asc_expression TRUE)
  elseif(_name STREQUAL "ASC::expression")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR "ASC::expression has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::expression alias must not be imported."
      )
    endif()
    set(_observed_asc_expression_alias TRUE)
  elseif(_name STREQUAL "asc_dense")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "asc_dense has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_dense target must not be imported."
      )
    endif()
    set(_observed_asc_dense TRUE)
  elseif(_name STREQUAL "ASC::dense")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "ASC::dense has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::dense alias must not be imported."
      )
    endif()
    set(_observed_asc_dense_alias TRUE)
  elseif(_name STREQUAL "asc_sparse")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "asc_sparse has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_sparse target must not be imported."
      )
    endif()
    set(_observed_asc_sparse TRUE)
  elseif(_name STREQUAL "ASC::sparse")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "ASC::sparse has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::sparse alias must not be imported."
      )
    endif()
    set(_observed_asc_sparse_alias TRUE)
  elseif(_name STREQUAL "asc_random")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "asc_random has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_random target must not be imported."
      )
    endif()
    set(_observed_asc_random TRUE)
  elseif(_name STREQUAL "ASC::random")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR "ASC::random has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::random alias must not be imported."
      )
    endif()
    set(_observed_asc_random_alias TRUE)
  elseif(_name STREQUAL "asc_random_dense")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR
        "asc_random_dense has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_random_dense target must not be imported."
      )
    endif()
    set(_observed_asc_random_dense TRUE)
  elseif(_name STREQUAL "ASC::random_dense")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR
        "ASC::random_dense has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::random_dense alias must not be imported."
      )
    endif()
    set(_observed_asc_random_dense_alias TRUE)
  elseif(_name STREQUAL "asc_random_sparse")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR
        "asc_random_sparse has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_random_sparse target must not be imported."
      )
    endif()
    set(_observed_asc_random_sparse TRUE)
  elseif(_name STREQUAL "ASC::random_sparse")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR
        "ASC::random_sparse has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::random_sparse alias must not be imported."
      )
    endif()
    set(_observed_asc_random_sparse_alias TRUE)
  elseif(_name STREQUAL "asc_cpp")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR "asc_cpp has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_cpp target must not be imported."
      )
    endif()
    set(_observed_asc_cpp TRUE)
  elseif(_name STREQUAL "ASC::cpp")
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR "ASC::cpp has unexpected target type: ${_type}")
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::cpp alias must not be imported."
      )
    endif()
    set(_observed_asc_cpp_alias TRUE)
  elseif(_name STREQUAL "asc_core_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR
        "asc_core_cuda has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_core_cuda target must not be imported."
      )
    endif()
    set(_observed_asc_core_cuda TRUE)
  elseif(_name STREQUAL "ASC::core_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR
        "ASC::core_cuda has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::core_cuda alias must not be imported."
      )
    endif()
    set(_observed_asc_core_cuda_alias TRUE)
  elseif(_name STREQUAL "asc_dense_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR
        "asc_dense_cuda has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree asc_dense_cuda target must not be imported."
      )
    endif()
    set(_observed_asc_dense_cuda TRUE)
  elseif(_name STREQUAL "ASC::dense_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$")
      message(FATAL_ERROR
        "ASC::dense_cuda has unexpected target type: ${_type}"
      )
    endif()
    if(_imported)
      message(FATAL_ERROR
        "The build-tree ASC::dense_cuda alias must not be imported."
      )
    endif()
    set(_observed_asc_dense_cuda_alias TRUE)
  elseif(_name STREQUAL "asc_sparse_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree asc_sparse_cuda target has invalid type/import state."
      )
    endif()
    set(_observed_asc_sparse_cuda TRUE)
  elseif(_name STREQUAL "ASC::sparse_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree ASC::sparse_cuda alias has invalid type/import state."
      )
    endif()
    set(_observed_asc_sparse_cuda_alias TRUE)
  elseif(_name STREQUAL "asc_random_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree asc_random_cuda target has invalid type/import state."
      )
    endif()
    set(_observed_asc_random_cuda TRUE)
  elseif(_name STREQUAL "ASC::random_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree ASC::random_cuda alias has invalid type/import state."
      )
    endif()
    set(_observed_asc_random_cuda_alias TRUE)
  elseif(_name STREQUAL "asc_random_dense_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree asc_random_dense_cuda target has invalid "
        "type/import state."
      )
    endif()
    set(_observed_asc_random_dense_cuda TRUE)
  elseif(_name STREQUAL "ASC::random_dense_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree ASC::random_dense_cuda alias has invalid "
        "type/import state."
      )
    endif()
    set(_observed_asc_random_dense_cuda_alias TRUE)
  elseif(_name STREQUAL "asc_random_sparse_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree asc_random_sparse_cuda target has invalid "
        "type/import state."
      )
    endif()
    set(_observed_asc_random_sparse_cuda TRUE)
  elseif(_name STREQUAL "ASC::random_sparse_cuda")
    if(NOT _type MATCHES "^(STATIC|SHARED)_LIBRARY$" OR _imported)
      message(FATAL_ERROR
        "The build-tree ASC::random_sparse_cuda alias has invalid "
        "type/import state."
      )
    endif()
    set(_observed_asc_random_sparse_cuda_alias TRUE)
  endif()
endforeach()

if(NOT _observed_asc_core
   OR NOT _observed_asc_core_alias
   OR NOT _observed_asc_utilities
   OR NOT _observed_asc_utilities_alias
   OR NOT _observed_asc_expression
   OR NOT _observed_asc_expression_alias
   OR NOT _observed_asc_dense
   OR NOT _observed_asc_dense_alias
   OR NOT _observed_asc_sparse
   OR NOT _observed_asc_sparse_alias
   OR NOT _observed_asc_random
   OR NOT _observed_asc_random_alias
   OR NOT _observed_asc_random_dense
   OR NOT _observed_asc_random_dense_alias
   OR NOT _observed_asc_random_sparse
   OR NOT _observed_asc_random_sparse_alias
   OR NOT _observed_asc_cpp
   OR NOT _observed_asc_cpp_alias)
  message(FATAL_ERROR
    "Milestone 8 must expose exactly the approved six modules, two random "
    "facets, and provider-free aggregate."
  )
endif()

if(ENABLE_CUDA)
  if(NOT _observed_asc_core_cuda
     OR NOT _observed_asc_core_cuda_alias
     OR NOT _observed_asc_dense_cuda
     OR NOT _observed_asc_dense_cuda_alias
     OR NOT _observed_asc_sparse_cuda
     OR NOT _observed_asc_sparse_cuda_alias
     OR NOT _observed_asc_random_cuda
     OR NOT _observed_asc_random_cuda_alias
     OR NOT _observed_asc_random_dense_cuda
     OR NOT _observed_asc_random_dense_cuda_alias
     OR NOT _observed_asc_random_sparse_cuda
     OR NOT _observed_asc_random_sparse_cuda_alias)
    message(FATAL_ERROR
      "A CUDA-enabled Milestone 8 build must expose exactly the six approved "
      "CUDA facets in addition to the provider-free graph."
    )
  endif()
elseif(_observed_asc_core_cuda
       OR _observed_asc_core_cuda_alias
       OR _observed_asc_dense_cuda
       OR _observed_asc_dense_cuda_alias
       OR _observed_asc_sparse_cuda
       OR _observed_asc_sparse_cuda_alias
       OR _observed_asc_random_cuda
       OR _observed_asc_random_cuda_alias
       OR _observed_asc_random_dense_cuda
       OR _observed_asc_random_dense_cuda_alias
       OR _observed_asc_random_sparse_cuda
       OR _observed_asc_random_sparse_cuda_alias)
  message(FATAL_ERROR
    "A CUDA-disabled Milestone 8 build must expose no CUDA provider target."
  )
endif()

set(_cmake_sources "${SOURCE_DIR}/CMakeLists.txt")
file(
  GLOB_RECURSE _cmake_module_sources
  LIST_DIRECTORIES FALSE
  "${SOURCE_DIR}/cmake/*.cmake"
  "${SOURCE_DIR}/cmake/*.cmake.in"
)
list(APPEND _cmake_sources ${_cmake_module_sources})
foreach(_cmake_source IN LISTS _cmake_sources)
  file(READ "${_cmake_source}" _contents)
  string(REGEX REPLACE "#[^\n]*" "" _without_comments "${_contents}")
  if(_without_comments MATCHES
     "(^|[\n\r])[ \t]*export[ \t\r\n]*\\([ \t\r\n]*PACKAGE([ \t\r\n]|\\))")
    message(FATAL_ERROR
      "The CMake user package registry must not be mutated: "
      "${_cmake_source} calls export(PACKAGE)."
    )
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
string(REGEX MATCH "project\\([^)]*\\)" _project_declaration "${_root_cmake}")
string(REGEX REPLACE "[ \t\r\n]+" " " _project_declaration "${_project_declaration}")
if(NOT _project_declaration MATCHES "^project\\( ASCCpp( |\\))"
   OR NOT _project_declaration MATCHES " VERSION 0\\.9\\.0( |\\))"
   OR NOT _project_declaration MATCHES " LANGUAGES CXX( |\\))")
  message(FATAL_ERROR
    "The root project must be ASCCpp 0.9.0 with LANGUAGES CXX."
  )
endif()
