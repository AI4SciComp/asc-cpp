cmake_minimum_required(VERSION 3.25)

project(ASCCppUtilitiesConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS utilities)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0"
   OR NOT ASCCpp_utilities_FOUND
   OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR "Expected ASCCpp 0.9.0 utilities with core closure.")
endif()
if(NOT TARGET ASC::utilities OR NOT TARGET ASC::core)
  message(FATAL_ERROR "Utilities lookup omitted an imported target.")
endif()
foreach(_forbidden IN ITEMS expression dense sparse random random_dense
                            random_sparse cpp core_cuda dense_cuda sparse_cuda
                            random_cuda random_dense_cuda random_sparse_cuda)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR
      "Utilities-only lookup imported ASC::${_forbidden}."
    )
  endif()
endforeach()
get_target_property(_type ASC::utilities TYPE)
if(DEFINED ASCCPP_EXPECT_LIBRARY_TYPE
   AND NOT _type STREQUAL ASCCPP_EXPECT_LIBRARY_TYPE)
  message(FATAL_ERROR
    "Expected utilities type '${ASCCPP_EXPECT_LIBRARY_TYPE}', found '${_type}'."
  )
endif()
get_target_property(_links ASC::utilities INTERFACE_LINK_LIBRARIES)
if(NOT _links STREQUAL "ASC::core")
  message(FATAL_ERROR "Utilities dependencies differ: '${_links}'.")
endif()

add_executable(asc_cpp_utilities_consumer main.cc)
set_target_properties(
  asc_cpp_utilities_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(asc_cpp_utilities_consumer PRIVATE ASC::utilities)
