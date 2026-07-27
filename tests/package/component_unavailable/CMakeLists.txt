cmake_minimum_required(VERSION 3.25)

project(ASCCppComponentContractConsumer LANGUAGES CXX)

foreach(_required_variable IN ITEMS
    ASCCPP_REQUEST_MODE
    ASCCPP_EXPECT_FOUND
    ASCCPP_EXPECT_CUDA
)
  if(NOT DEFINED "${_required_variable}")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

if(ASCCPP_REQUEST_MODE STREQUAL "no-component")
  find_package(ASCCpp 0.9.0 EXACT CONFIG QUIET)
elseif(ASCCPP_REQUEST_MODE STREQUAL "required-no-component")
  find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED)
elseif(ASCCPP_REQUEST_MODE STREQUAL "component")
  find_package(
    ASCCpp 0.9.0 EXACT CONFIG QUIET
    COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "required-component")
  find_package(
    ASCCpp 0.9.0 EXACT CONFIG REQUIRED
    COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "optional-component")
  find_package(
    ASCCpp 0.9.0 EXACT CONFIG QUIET
    OPTIONAL_COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "core-with-optional-component")
  find_package(
    ASCCpp 0.9.0 EXACT CONFIG REQUIRED
    COMPONENTS core
    OPTIONAL_COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
else()
  message(FATAL_ERROR "Unknown ASCCPP_REQUEST_MODE: ${ASCCPP_REQUEST_MODE}")
endif()

if(ASCCPP_EXPECT_FOUND AND NOT ASCCpp_FOUND)
  message(FATAL_ERROR
    "Expected ASCCpp to be found: '${ASCCpp_NOT_FOUND_MESSAGE}'."
  )
elseif(NOT ASCCPP_EXPECT_FOUND AND ASCCpp_FOUND)
  message(FATAL_ERROR "Expected ASCCpp not to be found.")
endif()
if(NOT DEFINED ASCCpp_VERSION OR NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR
    "Expected ASCCpp_VERSION 0.9.0, got '${ASCCpp_VERSION}'."
  )
endif()

set(_expected_known_components
  core
  utilities
  expression
  dense
  sparse
  random
  random_dense
  random_sparse
  cpp
  core_cuda
  dense_cuda
  sparse_cuda
  random_cuda
  random_dense_cuda
  random_sparse_cuda
)
set(_actual_known_components "${ASCCpp_KNOWN_COMPONENTS}")
list(SORT _expected_known_components)
list(SORT _actual_known_components)
if(NOT _actual_known_components STREQUAL _expected_known_components)
  message(FATAL_ERROR
    "ASCCpp_KNOWN_COMPONENTS differs from the frozen contract: "
    "'${ASCCpp_KNOWN_COMPONENTS}'."
  )
endif()
set(_expected_available_components
  core utilities expression random dense sparse random_dense random_sparse cpp
)
if(ASCCPP_EXPECT_CUDA)
  list(APPEND _expected_available_components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()
set(_actual_available_components "${ASCCpp_AVAILABLE_COMPONENTS}")
list(SORT _expected_available_components)
list(SORT _actual_available_components)
if(NOT _actual_available_components STREQUAL _expected_available_components)
  message(FATAL_ERROR
    "Milestone 8 available components differ from the frozen contract: "
    "'${ASCCpp_AVAILABLE_COMPONENTS}'."
  )
endif()

set(_expected_targets)
if(ASCCpp_FOUND)
  if(ASCCPP_REQUEST_MODE STREQUAL "core-with-optional-component")
    list(APPEND _expected_targets core)
    if(ASCCPP_REQUEST_COMPONENT IN_LIST _expected_available_components)
      list(APPEND _expected_targets "${ASCCPP_REQUEST_COMPONENT}")
    endif()
  elseif(ASCCPP_REQUEST_MODE STREQUAL "no-component"
         OR ASCCPP_REQUEST_MODE STREQUAL "required-no-component")
    list(APPEND _expected_targets
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
  elseif(ASCCPP_REQUEST_MODE STREQUAL "component"
         OR ASCCPP_REQUEST_MODE STREQUAL "required-component"
         OR ASCCPP_REQUEST_MODE STREQUAL "optional-component")
    if(ASCCPP_REQUEST_COMPONENT IN_LIST _expected_available_components)
      list(APPEND _expected_targets "${ASCCPP_REQUEST_COMPONENT}")
    endif()
  endif()

  if("cpp" IN_LIST _expected_targets)
    list(APPEND _expected_targets
      core
      utilities
      expression
      random
      dense
      sparse
      random_dense
      random_sparse
    )
  endif()
  if("random_dense" IN_LIST _expected_targets)
    list(APPEND _expected_targets core expression random dense)
  endif()
  if("random_sparse" IN_LIST _expected_targets)
    list(APPEND _expected_targets core expression random sparse)
  endif()
  if("core_cuda" IN_LIST _expected_targets)
    list(APPEND _expected_targets core)
  endif()
  if("dense_cuda" IN_LIST _expected_targets)
    list(APPEND _expected_targets
      core
      expression
      dense
      core_cuda
    )
  endif()
  if("sparse_cuda" IN_LIST _expected_targets)
    list(APPEND _expected_targets
      core
      expression
      sparse
      core_cuda
    )
  endif()
  if("random_cuda" IN_LIST _expected_targets)
    list(APPEND _expected_targets core random core_cuda)
  endif()
  if("random_dense_cuda" IN_LIST _expected_targets)
    list(APPEND _expected_targets
      core
      expression
      random
      dense
      random_dense
      core_cuda
      random_cuda
    )
  endif()
  if("random_sparse_cuda" IN_LIST _expected_targets)
    list(APPEND _expected_targets
      core
      expression
      random
      sparse
      random_sparse
      core_cuda
      random_cuda
    )
  endif()
  if("dense" IN_LIST _expected_targets OR "sparse" IN_LIST _expected_targets)
    list(APPEND _expected_targets core expression)
  endif()
  if(_expected_targets AND NOT "core" IN_LIST _expected_targets)
    list(APPEND _expected_targets core)
  endif()
endif()
list(REMOVE_DUPLICATES _expected_targets)

foreach(_component IN LISTS _expected_known_components)
  if(_component IN_LIST _expected_targets)
    if(NOT TARGET "ASC::${_component}")
      message(FATAL_ERROR
        "Request did not create expected target ASC::${_component}."
      )
    endif()
  elseif(TARGET "ASC::${_component}")
    message(FATAL_ERROR
      "Request unexpectedly created target ASC::${_component}."
    )
  endif()
endforeach()

if("core_cuda" IN_LIST _expected_targets
   OR "dense_cuda" IN_LIST _expected_targets
   OR "sparse_cuda" IN_LIST _expected_targets
   OR "random_cuda" IN_LIST _expected_targets
   OR "random_dense_cuda" IN_LIST _expected_targets
   OR "random_sparse_cuda" IN_LIST _expected_targets)
  if(NOT TARGET CUDA::cudart)
    message(FATAL_ERROR
      "A requested CUDA provider closure must discover CUDA::cudart."
    )
  endif()
  if("dense_cuda" IN_LIST _expected_targets AND NOT TARGET CUDA::cublas)
    message(FATAL_ERROR
      "The dense_cuda closure must discover CUDA::cublas."
    )
  endif()
  if("sparse_cuda" IN_LIST _expected_targets AND NOT TARGET CUDA::cusparse)
    message(FATAL_ERROR
      "The sparse_cuda closure must discover CUDA::cusparse."
    )
  endif()
elseif(TARGET CUDA::cudart
       OR TARGET CUDA::cublas
       OR TARGET CUDA::cusparse
       OR CUDAToolkit_FOUND)
  message(FATAL_ERROR
    "A provider-free package request must not discover CUDAToolkit."
  )
endif()

if(TARGET ASC::core)
  add_executable(asc_cpp_package_core_smoke core_smoke.cc)
  target_link_libraries(asc_cpp_package_core_smoke PRIVATE ASC::core)
endif()
