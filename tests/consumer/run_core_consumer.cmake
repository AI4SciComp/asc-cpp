cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    ACTION
    PRODUCER_BINARY_DIR
    CONSUMER_SOURCE_DIR
    BUILD_PACKAGE_DIR
    EXPECT_LIBRARY_TYPE
    GENERATOR
    WORK_DIR
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

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

function(_generator_arguments output_variable)
  set(_arguments -G "${GENERATOR}")
  if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
    list(APPEND _arguments -A "${GENERATOR_PLATFORM}")
  endif()
  if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
    list(APPEND _arguments -T "${GENERATOR_TOOLSET}")
  endif()
  if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
    list(APPEND _arguments "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}")
  endif()
  set("${output_variable}" "${_arguments}" PARENT_SCOPE)
endfunction()

function(_find_package_directory prefix output_variable)
  file(
    GLOB_RECURSE _config_files
    LIST_DIRECTORIES FALSE
    "${prefix}/ASCCppConfig.cmake"
  )
  list(LENGTH _config_files _config_count)
  if(NOT _config_count EQUAL 1)
    message(FATAL_ERROR
      "Expected one ASCCppConfig.cmake below '${prefix}', found "
      "${_config_count}: ${_config_files}"
    )
  endif()
  list(GET _config_files 0 _config_file)
  get_filename_component(_package_directory "${_config_file}" DIRECTORY)
  set("${output_variable}" "${_package_directory}" PARENT_SCOPE)
endfunction()

function(_configure_build_run package_directory case_name)
  set(_consumer_build_directory "${WORK_DIR}/${case_name} build")
  _generator_arguments(_generator_args)
  _run_success(
    "${case_name} isolated core consumer configure"
    "${CMAKE_COMMAND}"
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${_consumer_build_directory}"
    ${_generator_args}
    "-DASCCpp_DIR:PATH=${package_directory}"
    "-DASCCPP_EXPECT_LIBRARY_TYPE:STRING=${EXPECT_LIBRARY_TYPE}"
    "-DASCCPP_EXPECT_CUDA:BOOL=${ASCCPP_EXPECT_CUDA}"
    "-DASCCPP_EXPECT_LAPACK:BOOL=${ASCCPP_EXPECT_LAPACK}"
    "-DASCCPP_OPTIONAL_COMPONENT:STRING=definitely_unknown"
  )

  set(_build_command
    "${CMAKE_COMMAND}" --build "${_consumer_build_directory}"
  )
  if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND _build_command --config "${CONFIG}")
  endif()
  _run_success("${case_name} isolated core consumer build" ${_build_command})

  file(
    GLOB_RECURSE _consumer_files
    LIST_DIRECTORIES FALSE
    "${_consumer_build_directory}/*"
  )
  set(_executables)
  foreach(_consumer_file IN LISTS _consumer_files)
    get_filename_component(_consumer_file_name "${_consumer_file}" NAME)
    if(_consumer_file_name STREQUAL "asc_cpp_core_consumer"
       OR _consumer_file_name STREQUAL "asc_cpp_core_consumer.exe")
      list(APPEND _executables "${_consumer_file}")
    endif()
  endforeach()
  list(LENGTH _executables _executable_count)
  if(NOT _executable_count EQUAL 1)
    message(FATAL_ERROR
      "Expected one isolated core consumer executable, found "
      "${_executable_count}: ${_executables}"
    )
  endif()
  list(GET _executables 0 _executable)
  if(WIN32)
    file(
      GLOB_RECURSE _runtime_libraries
      LIST_DIRECTORIES FALSE
      "${PRODUCER_BINARY_DIR}/*.dll"
      "${WORK_DIR}/*.dll"
    )
    get_filename_component(_executable_directory "${_executable}" DIRECTORY)
    foreach(_runtime_library IN LISTS _runtime_libraries)
      _run_success(
        "${case_name} runtime-library staging"
        "${CMAKE_COMMAND}" -E copy_if_different
        "${_runtime_library}" "${_executable_directory}"
      )
    endforeach()
  endif()
  _run_success("${case_name} isolated core consumer runtime" "${_executable}")
endfunction()

if(ACTION STREQUAL "BUILD_TREE")
  _configure_build_run("${BUILD_PACKAGE_DIR}" "build tree")
elseif(ACTION STREQUAL "INSTALL_RELOCATE")
  set(_original_prefix "${WORK_DIR}/original prefix")
  set(_relocated_prefix "${WORK_DIR}/relocated prefix with spaces")
  set(_install_command
    "${CMAKE_COMMAND}" --install "${PRODUCER_BINARY_DIR}"
    --prefix "${_original_prefix}"
  )
  if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND _install_command --config "${CONFIG}")
  endif()
  _run_success("ASCCpp isolated-consumer installation" ${_install_command})
  file(RENAME "${_original_prefix}" "${_relocated_prefix}")
  _find_package_directory("${_relocated_prefix}" _package_directory)
  _configure_build_run("${_package_directory}" "relocated install")
else()
  message(FATAL_ERROR "Unknown consumer ACTION: ${ACTION}")
endif()
