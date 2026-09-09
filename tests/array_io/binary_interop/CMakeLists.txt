cmake_minimum_required(VERSION 3.25)
project(ASCCppInstalledBinaryInterop LANGUAGES CXX)
include(CTest)
if(NOT ASC_BINARY_COMPONENT MATCHES "^(dense|sparse)$")
  message(FATAL_ERROR "ASC_BINARY_COMPONENT must select dense or sparse.")
endif()
find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS ${ASC_BINARY_COMPONENT})
if(ASC_BINARY_COMPONENT STREQUAL "dense")
  set(_other sparse)
else()
  set(_other dense)
endif()
foreach(_target IN ITEMS ${_other} dense_lapack random utilities core_cuda)
  if(TARGET ASC::${_target})
    message(FATAL_ERROR "Isolated binary consumer imported ${_target}.")
  endif()
endforeach()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(binary_interop main.cc)
target_compile_features(binary_interop PRIVATE cxx_std_20)
set_target_properties(binary_interop PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(binary_interop PRIVATE ASC::${ASC_BINARY_COMPONENT})
if(MSVC)
  target_compile_options(binary_interop PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(binary_interop PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
if(ASC_BINARY_COMPONENT STREQUAL "dense")
  target_compile_definitions(binary_interop PRIVATE TEST_DENSE=1)
endif()
# Each invocation owns fresh scratch, including when CTest is rerun in one build.
add_test(NAME asc_cpp.array_io.binary_interop.${ASC_BINARY_COMPONENT}
  COMMAND "${CMAKE_COMMAND}"
    "-DPYTHON=${Python3_EXECUTABLE}"
    "-DEXECUTABLE=$<TARGET_FILE:binary_interop>"
    "-DCOMPONENT=${ASC_BINARY_COMPONENT}"
    "-DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/scratch"
    "-DTOOL_DIR=${ASC_BINARY_TOOLS}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/run.cmake")
set_tests_properties(asc_cpp.array_io.binary_interop.${ASC_BINARY_COMPONENT}
  PROPERTIES LABELS "array-io;installed;independent-wire")
