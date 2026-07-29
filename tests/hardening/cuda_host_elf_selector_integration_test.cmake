cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    SOURCE_DIR
    WORK_DIR
    ROOT
    GUARD
    GENERATOR
    CXX_COMPILER
    CUDA_HOST_COMPILER
    CUDA_ARCHITECTURES
    ASCCMAKE_DIR
)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

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
  "-DCMAKE_CUDA_HOST_COMPILER:FILEPATH=${CUDA_HOST_COMPILER}"
  "-DCMAKE_CUDA_ARCHITECTURES:STRING=${CUDA_ARCHITECTURES}"
  "-DBUILD_SHARED_LIBS:BOOL=ON"
  "-DBUILD_TESTING:BOOL=ON"
  "-DASC_CPP_BUILD_TESTING:BOOL=ON"
  "-DASC_CPP_INSTALL:BOOL=ON"
  "-DASC_CPP_ENABLE_CUDA:BOOL=ON"
  "-DASC_CPP_WARNINGS_AS_ERRORS:BOOL=ON"
  "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
)
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
    "Unlike CUDA-host integration configure failed.\n"
    "stdout:\n${_configure_stdout}\n"
    "stderr:\n${_configure_stderr}"
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
if(NOT _compiler_record MATCHES
   "set\\(CMAKE_CUDA_HOST_COMPILER_ID \"Clang\"\\)"
   OR NOT _compiler_record MATCHES
   "set\\(CMAKE_CUDA_HOST_COMPILER_VERSION \"19\\.[0-9.]+\"\\)")
  message(FATAL_ERROR
    "The nested configure did not use the requested unlike CUDA host.\n"
    "${_compiler_record}"
  )
endif()

set(_ctest_file "${WORK_DIR}/tests/hardening/CTestTestfile.cmake")
if(NOT EXISTS "${_ctest_file}")
  message(FATAL_ERROR "Nested hardening CTest file is missing")
endif()
file(READ "${_ctest_file}" _ctest_contents)
if(_ctest_contents MATCHES
   "ASC_CPP_HARDENING_BASELINE:FILEPATH=[^\" ]+gcc11-cuda12-shared")
  message(FATAL_ERROR
    "The exact GCC-host CUDA ELF baseline was enforced for a Clang host"
  )
endif()
if(NOT _ctest_contents MATCHES
   "ASC_CPP_HARDENING_BASELINE:FILEPATH=")
  message(FATAL_ERROR
    "The nested ELF test did not record a non-enforcing baseline selection"
  )
endif()
