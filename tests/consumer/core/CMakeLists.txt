cmake_minimum_required(VERSION 3.25)

project(ASCCppCoreConsumer LANGUAGES CXX)

if(DEFINED ASCCPP_OPTIONAL_COMPONENT
   AND NOT ASCCPP_OPTIONAL_COMPONENT STREQUAL "")
  find_package(
    ASCCpp 0.9 CONFIG REQUIRED
    COMPONENTS core
    OPTIONAL_COMPONENTS "${ASCCPP_OPTIONAL_COMPONENT}"
  )
else()
  find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core)
endif()

if(NOT ASCCpp_FOUND OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR "ASCCpp core must be available to the isolated consumer.")
endif()
if(NOT TARGET ASC::core)
  message(FATAL_ERROR "ASCCpp core package did not define ASC::core.")
endif()
if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
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
if(DEFINED ASCCPP_OPTIONAL_COMPONENT
   AND NOT ASCCPP_OPTIONAL_COMPONENT STREQUAL "")
  if(NOT DEFINED "ASCCpp_${ASCCPP_OPTIONAL_COMPONENT}_FOUND"
     OR ASCCpp_${ASCCPP_OPTIONAL_COMPONENT}_FOUND)
    message(FATAL_ERROR
      "Optional component '${ASCCPP_OPTIONAL_COMPONENT}' must be unavailable."
    )
  endif()
endif()

set(_forbidden_targets
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
foreach(_target IN LISTS _forbidden_targets)
  if(TARGET "${_target}")
    message(FATAL_ERROR
      "The core-only package imported forbidden target '${_target}'."
    )
  endif()
endforeach()

get_target_property(_links ASC::core INTERFACE_LINK_LIBRARIES)
if(_links)
  message(FATAL_ERROR
    "ASC::core must have no direct link dependencies, found '${_links}'."
  )
endif()
get_target_property(_features ASC::core INTERFACE_COMPILE_FEATURES)
if(NOT "cxx_std_20" IN_LIST _features)
  message(FATAL_ERROR
    "ASC::core must publish cxx_std_20, found '${_features}'."
  )
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::core TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::core type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_core_consumer main.cc)
set_target_properties(
  asc_cpp_core_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_core_consumer PRIVATE ASC::core)

unset(_features)
unset(_actual_available_components)
unset(_expected_available_components)
unset(_forbidden_targets)
unset(_library_type)
unset(_links)
