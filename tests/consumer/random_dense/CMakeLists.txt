cmake_minimum_required(VERSION 3.25)

project(ASCCppRandomDenseConsumer LANGUAGES CXX)

find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS random_dense)

if(NOT ASCCpp_VERSION VERSION_EQUAL "0.9.0"
   OR NOT ASCCpp_random_dense_FOUND
   OR NOT ASCCpp_random_FOUND
   OR NOT ASCCpp_dense_FOUND
   OR NOT ASCCpp_expression_FOUND
   OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR
    "Expected ASCCpp 0.9.0 random_dense with its exact closure."
  )
endif()
foreach(_required IN ITEMS core expression random dense random_dense)
  if(NOT TARGET "ASC::${_required}")
    message(FATAL_ERROR
      "random_dense lookup omitted ASC::${_required}."
    )
  endif()
endforeach()
foreach(_forbidden IN ITEMS
    utilities sparse random_sparse cpp core_cuda dense_cuda sparse_cuda
    random_cuda random_dense_cuda random_sparse_cuda)
  if(TARGET "ASC::${_forbidden}")
    message(FATAL_ERROR
      "random_dense-only lookup imported ASC::${_forbidden}."
    )
  endif()
endforeach()
get_target_property(_type ASC::random_dense TYPE)
if(NOT _type STREQUAL "INTERFACE_LIBRARY")
  message(FATAL_ERROR
    "Expected random_dense interface target, found '${_type}'."
  )
endif()
get_target_property(_links ASC::random_dense INTERFACE_LINK_LIBRARIES)
if(NOT _links STREQUAL "ASC::random;ASC::dense")
  message(FATAL_ERROR "random_dense dependencies differ: '${_links}'.")
endif()

add_executable(asc_cpp_random_dense_consumer main.cc)
set_target_properties(
  asc_cpp_random_dense_consumer
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
)
target_link_libraries(
  asc_cpp_random_dense_consumer
  PRIVATE ASC::random_dense
)
