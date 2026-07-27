cmake_minimum_required(VERSION 3.25)

project(ASCCppCppSubprojectConsumer LANGUAGES CXX)

if(NOT DEFINED ASCCPP_SOURCE_DIR OR ASCCPP_SOURCE_DIR STREQUAL "")
  message(FATAL_ERROR "ASCCPP_SOURCE_DIR is required.")
endif()

add_subdirectory("${ASCCPP_SOURCE_DIR}" asc-cpp)

if(ASC_CPP_BUILD_TESTING)
  message(FATAL_ERROR "Subproject use must default ASC_CPP_BUILD_TESTING off.")
endif()
if(ASC_CPP_INSTALL)
  message(FATAL_ERROR "Subproject use must default ASC_CPP_INSTALL off.")
endif()
foreach(_expected_target IN ITEMS
    ASC::core
    ASC::utilities
    ASC::expression
    ASC::random
    ASC::dense
    ASC::sparse
    ASC::random_dense
    ASC::random_sparse
    ASC::cpp
)
  if(NOT TARGET "${_expected_target}")
    message(FATAL_ERROR
      "Subproject use did not create ${_expected_target}."
    )
  endif()
endforeach()

add_executable(asc_cpp_subproject_consumer main.cc)
target_link_libraries(asc_cpp_subproject_consumer PRIVATE ASC::cpp)
