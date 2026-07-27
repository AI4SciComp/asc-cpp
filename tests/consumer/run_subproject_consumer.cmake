cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    SOURCE_DIR
    CONSUMER_SOURCE_DIR
    ASCCMAKE_DIR
    GENERATOR
    WORK_DIR
)
  if(NOT DEFINED "${_required_variable}" OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

function(_run_success description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed with exit code ${_result}.\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}"
    )
  endif()
endfunction()

set(_generator_arguments -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_arguments -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _generator_arguments -T "${GENERATOR_TOOLSET}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
  list(APPEND
    _generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}"
  )
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
set(_build_directory "${WORK_DIR}/build with spaces")
_run_success(
  "ASCCpp subproject consumer configure"
  "${CMAKE_COMMAND}"
  -S "${CONSUMER_SOURCE_DIR}"
  -B "${_build_directory}"
  ${_generator_arguments}
  "-DASCCPP_SOURCE_DIR:PATH=${SOURCE_DIR}"
  "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
)
set(_build_command "${CMAKE_COMMAND}" --build "${_build_directory}")
if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
  list(APPEND _build_command --config "${CONFIG}")
endif()
_run_success("ASCCpp subproject consumer build" ${_build_command})

file(
  GLOB_RECURSE _consumer_files
  LIST_DIRECTORIES FALSE
  "${_build_directory}/*"
)
set(_executables)
foreach(_consumer_file IN LISTS _consumer_files)
  get_filename_component(_consumer_file_name "${_consumer_file}" NAME)
  if(_consumer_file_name STREQUAL "asc_cpp_subproject_consumer"
     OR _consumer_file_name STREQUAL "asc_cpp_subproject_consumer.exe")
    list(APPEND _executables "${_consumer_file}")
  endif()
endforeach()
list(LENGTH _executables _executable_count)
if(NOT _executable_count EQUAL 1)
  message(FATAL_ERROR
    "Expected one subproject consumer executable, found "
    "${_executable_count}: ${_executables}"
  )
endif()
list(GET _executables 0 _executable)
if(WIN32)
  file(
    GLOB_RECURSE _runtime_libraries
    LIST_DIRECTORIES FALSE
    "${_build_directory}/*.dll"
  )
  get_filename_component(_executable_directory "${_executable}" DIRECTORY)
  foreach(_runtime_library IN LISTS _runtime_libraries)
    _run_success(
      "Subproject runtime-library staging"
      "${CMAKE_COMMAND}" -E copy_if_different
      "${_runtime_library}" "${_executable_directory}"
    )
  endforeach()
endif()
_run_success("ASCCpp subproject consumer runtime" "${_executable}")
