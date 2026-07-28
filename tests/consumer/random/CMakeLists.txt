cmake_minimum_required(VERSION 3.25)

project(ASCCppRandomConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0"
   OR NOT ASCCpp_random_FOUND
   OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR "Expected ASCCpp 0.9.0 random with core closure.")
endif()
if(NOT TARGET ASC::random OR NOT TARGET ASC::core)
  message(FATAL_ERROR "Random lookup omitted an imported target.")
endif()
foreach(_forbidden IN ITEMS utilities expression dense sparse random_dense
                            random_sparse cpp core_cuda dense_cuda sparse_cuda
                            random_cuda random_dense_cuda random_sparse_cuda)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR
      "Random-only lookup imported ASC::${_forbidden}."
    )
  endif()
endforeach()
get_target_property(_type ASC::random TYPE)
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT _type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
  message(FATAL_ERROR
    "Expected random type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found '${_type}'."
  )
endif()
get_target_property(_links ASC::random INTERFACE_LINK_LIBRARIES)
if(NOT _links STREQUAL "ASC::core")
  message(FATAL_ERROR "Random dependencies differ: '${_links}'.")
endif()

add_executable(asc_cpp_random_consumer main.cc)
set_target_properties(
  asc_cpp_random_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_random_consumer PRIVATE ASC::random)
