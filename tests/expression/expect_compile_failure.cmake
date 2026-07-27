if(NOT DEFINED CXX_COMPILER OR CXX_COMPILER STREQUAL "")
  message(FATAL_ERROR "CXX_COMPILER is required.")
endif()
if(NOT DEFINED INCLUDE_DIR OR NOT IS_DIRECTORY "${INCLUDE_DIR}")
  message(FATAL_ERROR "INCLUDE_DIR must name the public include directory.")
endif()
if(NOT DEFINED SOURCE OR NOT EXISTS "${SOURCE}")
  message(FATAL_ERROR "SOURCE must name the negative compile source.")
endif()
if(NOT DEFINED OUTPUT_DIR OR OUTPUT_DIR STREQUAL "")
  message(FATAL_ERROR "OUTPUT_DIR is required.")
endif()
if(NOT DEFINED TEST_NAME OR TEST_NAME STREQUAL "")
  message(FATAL_ERROR "TEST_NAME is required.")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

if(CXX_COMPILER_ID STREQUAL "MSVC"
   OR CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  execute_process(
    COMMAND
      "${CXX_COMPILER}"
      /nologo
      /std:c++20
      /permissive-
      "/I${INCLUDE_DIR}"
      /c "${SOURCE}"
      "/Fo${OUTPUT_DIR}/${TEST_NAME}.obj"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
else()
  execute_process(
    COMMAND
      "${CXX_COMPILER}"
      -std=c++20
      -pedantic-errors
      "-I${INCLUDE_DIR}"
      -fsyntax-only
      "${SOURCE}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
endif()

if(_result EQUAL 0)
  message(FATAL_ERROR
    "Negative compile source unexpectedly compiled: ${SOURCE}"
  )
endif()

message(STATUS "Negative compile failed as required: ${SOURCE}")
