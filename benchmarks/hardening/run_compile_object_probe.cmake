cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    CXX_COMPILER
    COMPILER_ID
    INCLUDE_DIR
    SOURCE_FILE
    OUTPUT_DIR
)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required.")
  endif()
endforeach()
if(NOT EXISTS "${SOURCE_FILE}")
  message(FATAL_ERROR "Probe source does not exist: ${SOURCE_FILE}")
endif()
if(NOT IS_DIRECTORY "${INCLUDE_DIR}")
  message(FATAL_ERROR "Probe include directory does not exist: ${INCLUDE_DIR}")
endif()
if(NOT DEFINED REPETITIONS)
  set(REPETITIONS 3)
endif()
if(REPETITIONS LESS 1)
  message(FATAL_ERROR "REPETITIONS must be positive.")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(_report
  "probe,compiler,compiler_id,repetition,elapsed_seconds,object_bytes\n"
)
set(_expected_size)
foreach(_repetition RANGE 1 "${REPETITIONS}")
  if(COMPILER_ID STREQUAL "MSVC")
    set(_object "${OUTPUT_DIR}/public_header_${_repetition}.obj")
    set(_compile_command
      "${CXX_COMPILER}"
      /nologo
      /std:c++20
      /permissive-
      "/I${INCLUDE_DIR}"
      /c "${SOURCE_FILE}"
      "/Fo${_object}"
      ${CXX_ARGUMENTS}
    )
  else()
    set(_object "${OUTPUT_DIR}/public_header_${_repetition}.o")
    set(_compile_command
      "${CXX_COMPILER}"
      -std=c++20
      "-I${INCLUDE_DIR}"
      -c "${SOURCE_FILE}"
      -o "${_object}"
      ${CXX_ARGUMENTS}
    )
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E time ${_compile_command}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "Public-header compile probe repetition ${_repetition} failed.\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}"
    )
  endif()
  if(NOT EXISTS "${_object}")
    message(FATAL_ERROR
      "Compile probe did not create '${_object}'."
    )
  endif()
  set(_timing "${_stdout}\n${_stderr}")
  if(NOT _timing MATCHES
     "Elapsed time \\(seconds\\): ([0-9]+([.][0-9]+)?)")
    message(FATAL_ERROR
      "Could not parse cmake -E time output: '${_timing}'."
    )
  endif()
  set(_elapsed "${CMAKE_MATCH_1}")
  file(SIZE "${_object}" _object_size)
  if(_object_size EQUAL 0)
    message(FATAL_ERROR "Compile probe created an empty object file.")
  endif()
  if(DEFINED _expected_size AND NOT _object_size EQUAL _expected_size)
    message(FATAL_ERROR
      "Repeated compilation changed object size from ${_expected_size} "
      "to ${_object_size} bytes."
    )
  endif()
  set(_expected_size "${_object_size}")
  string(APPEND _report
    "public_headers,\"${CXX_COMPILER}\",${COMPILER_ID},${_repetition},"
    "${_elapsed},${_object_size}\n"
  )
endforeach()

set(_report_file "${OUTPUT_DIR}/compile-object-observations.csv")
file(WRITE "${_report_file}" "${_report}")
message(STATUS
  "M8 compile/object probe passed: compiler=${CXX_COMPILER}; "
  "repetitions=${REPETITIONS}; object_bytes=${_expected_size}; "
  "report=${_report_file}"
)
