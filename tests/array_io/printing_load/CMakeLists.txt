cmake_minimum_required(VERSION 3.25)
project(ASCCppInstalledPrintingLoadTests LANGUAGES CXX)
include(CTest)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|AppleClang)$")
  message(FATAL_ERROR "Compiler-load observation requires Clang callbacks.")
endif()
find_package(ASCCpp 0.9.0 EXACT CONFIG REQUIRED COMPONENTS dense sparse)
foreach(_target IN ITEMS dense_lapack random utilities core_cuda)
  if(TARGET ASC::${_target})
    message(FATAL_ERROR "Printing diagnostics imported unrelated ${_target}.")
  endif()
endforeach()
add_library(observer OBJECT observation.cc)
target_compile_features(observer PRIVATE cxx_std_20)
target_compile_options(observer PRIVATE -O1 -Wall -Wextra -Wpedantic -Werror)
foreach(_name IN ITEMS instrumented uninstrumented)
  add_executable(${_name} main.cc $<TARGET_OBJECTS:observer>)
  target_compile_features(${_name} PRIVATE cxx_std_20)
  set_target_properties(${_name} PROPERTIES CXX_EXTENSIONS OFF)
  target_link_libraries(${_name} PRIVATE ASC::dense ASC::sparse)
  target_compile_options(${_name} PRIVATE -O1 -Wall -Wextra -Wpedantic -Werror)
endforeach()
target_compile_options(instrumented PRIVATE
  -fsanitize-coverage=trace-pc,trace-loads)
add_test(NAME asc_cpp.printing.compiler_load_observation COMMAND instrumented)
add_test(NAME asc_cpp.printing.compiler_load_negative_control
  COMMAND "${CMAKE_COMMAND}"
    "-DEXECUTABLE=$<TARGET_FILE:uninstrumented>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/negative_control.cmake")
set_tests_properties(asc_cpp.printing.compiler_load_observation
  asc_cpp.printing.compiler_load_negative_control
  PROPERTIES LABELS "array-io;printing;installed;compiler-loads")
