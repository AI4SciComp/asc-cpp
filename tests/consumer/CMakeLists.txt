cmake_minimum_required(VERSION 3.25)
project(ASCCppConsumer LANGUAGES CXX)

if(NOT DEFINED ASC_TEST_COMPONENT)
  message(FATAL_ERROR "ASC_TEST_COMPONENT is required")
endif()

find_package(ASCCpp CONFIG REQUIRED COMPONENTS "${ASC_TEST_COMPONENT}")
add_executable(asc_cpp_consumer main.cc)
target_link_libraries(asc_cpp_consumer PRIVATE "ASC::${ASC_TEST_COMPONENT}")
string(TOUPPER "${ASC_TEST_COMPONENT}" _asc_test_component_upper)
target_compile_definitions(
  asc_cpp_consumer PRIVATE "ASC_TEST_${_asc_test_component_upper}=1"
)
