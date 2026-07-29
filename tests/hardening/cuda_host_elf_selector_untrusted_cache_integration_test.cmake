cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    SOURCE_DIR
    WORK_DIR
    ROOT
    GUARD
    GENERATOR
    CXX_COMPILER
    CUDA_ARCHITECTURES
    ASCCMAKE_DIR
)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.31")
  message(FATAL_ERROR
    "The untrusted-cache regression applies only to CMake before 3.31"
  )
endif()

include("${SOURCE_DIR}/tests/cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ROOT}"
  GUARD "${GUARD}"
)

set(_command
  "${CMAKE_COMMAND}"
  -S "${SOURCE_DIR}"
  -B "${WORK_DIR}"
  -G "${GENERATOR}"
  "-DCMAKE_BUILD_TYPE:STRING=Release"
  "-DCMAKE_CXX_COMPILER:FILEPATH=${CXX_COMPILER}"
  "-DCMAKE_CUDA_HOST_COMPILER_ID:STRING=GNU"
  "-DCMAKE_CUDA_HOST_COMPILER_VERSION:STRING=11.4.0"
  "-DCMAKE_CUDA_ARCHITECTURES:STRING=${CUDA_ARCHITECTURES}"
  "-DBUILD_SHARED_LIBS:BOOL=ON"
  "-DBUILD_TESTING:BOOL=ON"
  "-DASC_CPP_BUILD_TESTING:BOOL=ON"
  "-DASC_CPP_INSTALL:BOOL=ON"
  "-DASC_CPP_ENABLE_CUDA:BOOL=ON"
  "-DASC_CPP_WARNINGS_AS_ERRORS:BOOL=ON"
  "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
)
if(DEFINED CUDA_HOST_COMPILER AND NOT "${CUDA_HOST_COMPILER}" STREQUAL "")
  list(APPEND _command
    "-DCMAKE_CUDA_HOST_COMPILER:FILEPATH=${CUDA_HOST_COMPILER}"
  )
endif()
if(DEFINED MAKE_PROGRAM AND NOT "${MAKE_PROGRAM}" STREQUAL "")
  list(APPEND _command "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}")
endif()

execute_process(
  COMMAND ${_command}
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_stdout
  ERROR_VARIABLE _configure_stderr
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "Untrusted CUDA-host cache integration configure failed.\n"
    "stdout:\n${_configure_stdout}\n"
    "stderr:\n${_configure_stderr}"
  )
endif()

set(_ctest_file "${WORK_DIR}/tests/hardening/CTestTestfile.cmake")
if(NOT EXISTS "${_ctest_file}")
  message(FATAL_ERROR "Nested hardening CTest file is missing")
endif()
file(READ "${_ctest_file}" _ctest_contents)
if(_ctest_contents MATCHES
   "ASC_CPP_HARDENING_BASELINE:FILEPATH=[^\" ]+")
  message(FATAL_ERROR
    "Untrusted pre-3.31 cache values selected an enforcing ELF baseline"
  )
endif()
if(NOT _ctest_contents MATCHES
   "ASC_CPP_HARDENING_BASELINE:FILEPATH=")
  message(FATAL_ERROR
    "The nested ELF test did not record a non-enforcing baseline selection"
  )
endif()
if(_ctest_contents MATCHES
   "cuda_host_elf_selector_integration")
  message(FATAL_ERROR
    "The exact-host integration was registered before CMake 3.31"
  )
endif()

file(
  GLOB
  _compiler_records
  LIST_DIRECTORIES FALSE
  "${WORK_DIR}/CMakeFiles/*/CMakeCUDACompiler.cmake"
)
list(LENGTH _compiler_records _compiler_record_count)
if(NOT _compiler_record_count EQUAL 1)
  message(FATAL_ERROR
    "Expected one configured CUDA compiler record, found "
    "${_compiler_record_count}"
  )
endif()
file(READ "${_compiler_records}" _compiler_record)
if(_compiler_record MATCHES
   "CMAKE_CUDA_HOST_COMPILER_(ID|VERSION)")
  message(FATAL_ERROR
    "CMake before 3.31 unexpectedly recorded a trusted CUDA host identity"
  )
endif()
