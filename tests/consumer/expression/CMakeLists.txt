cmake_minimum_required(VERSION 3.25)

project(ASCCppExpressionConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS expression)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0"
   OR NOT ASCCpp_expression_FOUND
   OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR "Expected ASCCpp 0.9.0 expression with core closure.")
endif()
if(NOT TARGET ASC::expression OR NOT TARGET ASC::core)
  message(FATAL_ERROR "Expression lookup omitted an imported target.")
endif()
foreach(_forbidden IN ITEMS utilities dense sparse random random_dense
                            random_sparse cpp core_cuda dense_cuda sparse_cuda
                            random_cuda random_dense_cuda random_sparse_cuda)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR
      "Expression-only lookup imported ASC::${_forbidden}."
    )
  endif()
endforeach()
get_target_property(_type ASC::expression TYPE)
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT _type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
  message(FATAL_ERROR
    "Expected expression type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found '${_type}'."
  )
endif()
get_target_property(_links ASC::expression INTERFACE_LINK_LIBRARIES)
if(NOT _links STREQUAL "ASC::core")
  message(FATAL_ERROR "Expression dependencies differ: '${_links}'.")
endif()

add_executable(asc_cpp_expression_consumer main.cc)
set_target_properties(
  asc_cpp_expression_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_expression_consumer PRIVATE ASC::expression)
