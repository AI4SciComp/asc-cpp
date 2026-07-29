cmake_minimum_required(VERSION 3.25)
project(ASCCppUnavailableComponentConsumer LANGUAGES NONE)

foreach(_required_variable IN ITEMS
    ASCCPP_REQUEST_MODE
    ASCCPP_EXPECT_FOUND
)
  if(NOT DEFINED "${_required_variable}")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

if(ASCCPP_REQUEST_MODE STREQUAL "component")
  find_package(ASCCpp 0.9.0 QUIET COMPONENTS "${ASCCPP_REQUEST_COMPONENT}")
elseif(ASCCPP_REQUEST_MODE STREQUAL "required-component")
  find_package(
    ASCCpp 0.9.0 REQUIRED COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "optional-component")
  find_package(
    ASCCpp 0.9.0 QUIET OPTIONAL_COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "core-with-optional-component")
  find_package(
    ASCCpp 0.9.0 REQUIRED
    COMPONENTS core
    OPTIONAL_COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "dense-with-optional-component")
  find_package(
    ASCCpp 0.9.0 REQUIRED
    COMPONENTS dense
    OPTIONAL_COMPONENTS "${ASCCPP_REQUEST_COMPONENT}"
  )
elseif(ASCCPP_REQUEST_MODE STREQUAL "no-component")
  find_package(ASCCpp 0.9.0 QUIET)
elseif(ASCCPP_REQUEST_MODE STREQUAL "required-no-component")
  find_package(ASCCpp 0.9.0 REQUIRED)
else()
  message(FATAL_ERROR "Unknown request mode: ${ASCCPP_REQUEST_MODE}")
endif()

if(ASCCpp_FOUND AND NOT ASCCPP_EXPECT_FOUND)
  message(FATAL_ERROR "ASCCpp was unexpectedly found.")
elseif(NOT ASCCpp_FOUND AND ASCCPP_EXPECT_FOUND)
  message(FATAL_ERROR "ASCCpp was unexpectedly not found.")
endif()
if(NOT ASCCpp_VERSION STREQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp version 0.9.0, got '${ASCCpp_VERSION}'.")
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
set(_provider_free_components
  core utilities expression dense sparse random
  random_dense random_sparse cpp
)
set(_expected_available_components "${_provider_free_components}")
if(DEFINED ASCCPP_EXPECT_CUDA AND ASCCPP_EXPECT_CUDA)
  list(APPEND _expected_available_components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()
if(NOT ASCCpp_AVAILABLE_COMPONENTS STREQUAL
   "${_expected_available_components}")
  message(FATAL_ERROR
    "Milestone 8 available components differ from this build: "
    "'${ASCCpp_AVAILABLE_COMPONENTS}'."
  )
endif()

if(NOT DEFINED ASCCPP_EXPECT_CORE_TARGET)
  message(FATAL_ERROR "ASCCPP_EXPECT_CORE_TARGET is required.")
endif()
if(ASCCPP_EXPECT_CORE_TARGET AND NOT TARGET ASC::core)
  message(FATAL_ERROR "This lookup must create ASC::core.")
elseif(NOT ASCCPP_EXPECT_CORE_TARGET AND TARGET ASC::core)
  message(FATAL_ERROR "This lookup unexpectedly created ASC::core.")
endif()

if(ASCCPP_REQUEST_MODE MATCHES
   "^(core|dense)-with-optional-component$"
   AND ASCCpp_${ASCCPP_REQUEST_COMPONENT}_FOUND)
  message(FATAL_ERROR
    "Optional unknown component '${ASCCPP_REQUEST_COMPONENT}' was found."
  )
endif()

set(_available_components
  core utilities expression dense sparse random
  random_dense random_sparse cpp
)
if(DEFINED ASCCPP_EXPECT_CUDA AND ASCCPP_EXPECT_CUDA)
  list(APPEND _available_components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
endif()
set(_expected_targets)
if(ASCCPP_REQUEST_MODE STREQUAL "core-with-optional-component")
  list(APPEND _expected_targets core)
elseif(ASCCPP_REQUEST_MODE STREQUAL "dense-with-optional-component")
  list(APPEND _expected_targets core expression dense)
elseif(ASCCPP_REQUEST_MODE MATCHES "^(no-component|required-no-component)$"
       AND ASCCpp_FOUND)
  set(_expected_targets "${_provider_free_components}")
elseif(ASCCpp_FOUND
       AND ASCCPP_REQUEST_COMPONENT IN_LIST _available_components)
  list(APPEND _expected_targets core "${ASCCPP_REQUEST_COMPONENT}")
  if(ASCCPP_REQUEST_COMPONENT STREQUAL "utilities")
    # Core is the only dependency.
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "expression")
    # Core is the only dependency.
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "dense"
         OR ASCCPP_REQUEST_COMPONENT STREQUAL "sparse")
    list(APPEND _expected_targets expression)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "random")
    # Core is the only dependency.
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "random_dense")
    list(APPEND _expected_targets expression random dense)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "random_sparse")
    list(APPEND _expected_targets expression random sparse)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "cpp")
    set(_expected_targets "${_provider_free_components}")
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "core_cuda")
    list(APPEND _expected_targets core_cuda)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "dense_cuda")
    list(APPEND _expected_targets expression dense core_cuda dense_cuda)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "sparse_cuda")
    list(APPEND _expected_targets expression sparse core_cuda sparse_cuda)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "random_cuda")
    list(APPEND _expected_targets random core_cuda random_cuda)
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "random_dense_cuda")
    list(APPEND _expected_targets
      expression
      random
      dense
      random_dense
      core_cuda
      random_cuda
      random_dense_cuda
    )
  elseif(ASCCPP_REQUEST_COMPONENT STREQUAL "random_sparse_cuda")
    list(APPEND _expected_targets
      expression
      random
      sparse
      random_sparse
      core_cuda
      random_cuda
      random_sparse_cuda
    )
  endif()
endif()
foreach(_component IN LISTS _expected_known_components)
  if(_component IN_LIST _expected_targets)
    if(NOT TARGET "ASC::${_component}")
      message(FATAL_ERROR
        "Component closure did not create ASC::${_component}."
      )
    endif()
  elseif(TARGET "ASC::${_component}")
    message(FATAL_ERROR
      "Lookup imported unexpected target ASC::${_component}."
    )
  endif()
endforeach()
