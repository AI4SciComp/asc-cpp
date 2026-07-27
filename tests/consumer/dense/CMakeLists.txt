cmake_minimum_required(VERSION 3.25)

project(ASCCppDenseConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)

if(NOT ASCCpp_FOUND OR NOT ASCCpp_core_FOUND OR
   NOT ASCCpp_expression_FOUND OR NOT ASCCpp_dense_FOUND)
  message(FATAL_ERROR
    "ASCCpp dense and its core/expression dependencies must be available."
  )
endif()
foreach(_required_target IN ITEMS ASC::core ASC::expression ASC::dense)
  if(NOT TARGET "${_required_target}")
    message(FATAL_ERROR
      "ASCCpp dense package did not define '${_required_target}'."
    )
  endif()
endforeach()
if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0")
  message(FATAL_ERROR "Expected ASCCpp 0.9.0, found '${ASCCpp_VERSION}'.")
endif()

set(_forbidden_targets
  ASC::utilities
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
      "The dense-only package imported forbidden target '${_target}'."
    )
  endif()
endforeach()

get_target_property(_links ASC::dense INTERFACE_LINK_LIBRARIES)
if(NOT _links OR NOT "ASC::core" IN_LIST _links OR
   NOT "ASC::expression" IN_LIST _links)
  message(FATAL_ERROR
    "ASC::dense must expose exactly its core/expression dependency ceiling, "
    "found '${_links}'."
  )
endif()
foreach(_forbidden_link IN ITEMS
    ASC::utilities
    ASC::sparse
    ASC::random
    ASC::cpp)
  if("${_forbidden_link}" IN_LIST _links)
    message(FATAL_ERROR
      "ASC::dense exposes forbidden direct dependency '${_forbidden_link}'."
    )
  endif()
endforeach()

get_target_property(_features ASC::dense INTERFACE_COMPILE_FEATURES)
if(NOT "cxx_std_20" IN_LIST _features)
  message(FATAL_ERROR
    "ASC::dense must publish cxx_std_20, found '${_features}'."
  )
endif()
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT ASCCPP_EXPECT_LIBRARY_TYPE STREQUAL "")
  get_target_property(_library_type ASC::dense TYPE)
  if(NOT _library_type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
    message(FATAL_ERROR
      "Expected ASC::dense type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found "
      "'${_library_type}'."
    )
  endif()
endif()

add_executable(asc_cpp_dense_consumer main.cc)
set_target_properties(
  asc_cpp_dense_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_dense_consumer PRIVATE ASC::dense)

unset(_features)
unset(_forbidden_link)
unset(_forbidden_targets)
unset(_library_type)
unset(_links)
