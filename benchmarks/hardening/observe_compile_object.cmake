cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS CXX_COMPILER INCLUDE_DIR WORK_DIR)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT IS_ABSOLUTE "${CXX_COMPILER}" OR NOT EXISTS "${CXX_COMPILER}")
  message(FATAL_ERROR "CXX_COMPILER must name an existing absolute path")
endif()
if(NOT IS_DIRECTORY "${INCLUDE_DIR}")
  message(FATAL_ERROR "INCLUDE_DIR does not exist: ${INCLUDE_DIR}")
endif()

if(NOT DEFINED OBSERVATION_FILE OR "${OBSERVATION_FILE}" STREQUAL "")
  set(OBSERVATION_FILE "${WORK_DIR}/compile-object-observations.txt")
endif()
if(NOT DEFINED OBSERVATION_BUILD_MODE
   OR "${OBSERVATION_BUILD_MODE}" STREQUAL "")
  set(OBSERVATION_BUILD_MODE "standalone-unspecified")
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")

if(NOT DEFINED COMPILER_STYLE OR "${COMPILER_STYLE}" STREQUAL "")
  get_filename_component(_compiler_name "${CXX_COMPILER}" NAME)
  string(TOLOWER "${_compiler_name}" _compiler_name_lower)
  if(_compiler_name_lower MATCHES "^(cl|cl\\.exe|clang-cl|clang-cl\\.exe)$")
    set(COMPILER_STYLE msvc)
  else()
    set(COMPILER_STYLE gnu)
  endif()
endif()
if(NOT COMPILER_STYLE STREQUAL "gnu"
   AND NOT COMPILER_STYLE STREQUAL "msvc")
  message(FATAL_ERROR
    "COMPILER_STYLE must be 'gnu' or 'msvc'; got '${COMPILER_STYLE}'"
  )
endif()

if(COMPILER_STYLE STREQUAL "msvc")
  if(NOT WIN32)
    message(FATAL_ERROR "MSVC-style compile observation requires Windows")
  endif()
  get_filename_component(_compiler_directory "${CXX_COMPILER}" DIRECTORY)
  get_filename_component(_msvc_target_architecture
    "${_compiler_directory}" NAME
  )
  set(_vc_directory "${_compiler_directory}")
  foreach(_parent IN ITEMS 1 2 3 4 5 6)
    get_filename_component(_vc_directory "${_vc_directory}" DIRECTORY)
  endforeach()
  set(_vcvarsall "${_vc_directory}/Auxiliary/Build/vcvarsall.bat")
  if(NOT EXISTS "${_vcvarsall}")
    message(FATAL_ERROR
      "Cannot locate vcvarsall.bat for the configured compiler: "
      "${CXX_COMPILER}"
    )
  endif()
  find_program(_command_interpreter NAMES cmd.exe cmd REQUIRED)
endif()

execute_process(
  COMMAND "${CXX_COMPILER}" --version
  RESULT_VARIABLE _version_result
  OUTPUT_VARIABLE _compiler_version
  ERROR_VARIABLE _compiler_version_error
)
if(NOT _version_result EQUAL 0 AND COMPILER_STYLE STREQUAL "msvc")
  execute_process(
    COMMAND "${CXX_COMPILER}" /Bv
    RESULT_VARIABLE _version_result
    OUTPUT_VARIABLE _compiler_version
    ERROR_VARIABLE _compiler_version_error
  )
endif()
set(_compiler_version
  "${_compiler_version}\n${_compiler_version_error}"
)
string(REPLACE "\r\n" "\n" _compiler_version "${_compiler_version}")
string(STRIP "${_compiler_version}" _compiler_version)
if("${_compiler_version}" STREQUAL "")
  message(FATAL_ERROR
    "Compiler version query produced no diagnostic (${_version_result})"
  )
endif()

set(_sources
  compile_core.cc
  compile_dense.cc
  compile_provider_free.cc
)
set(_workload_compile_core_cc
  "compile one Core public-header translation unit"
)
set(_workload_compile_dense_cc
  "compile one Dense storage/expression translation unit"
)
set(_workload_compile_provider_free_cc
  "compile one provider-free umbrella-header translation unit"
)
cmake_host_system_information(
  RESULT _host_os_name
  QUERY OS_NAME
)
cmake_host_system_information(
  RESULT _host_os_release
  QUERY OS_RELEASE
)
cmake_host_system_information(
  RESULT _host_processor
  QUERY PROCESSOR_NAME
)
cmake_host_system_information(
  RESULT _host_logical_cores
  QUERY NUMBER_OF_LOGICAL_CORES
)
string(JOIN " " _flags_text ${COMPILE_FLAGS})
file(WRITE "${OBSERVATION_FILE}"
  "kind=asc-cpp-m8-public-header-compile-object-observation\n"
  "compiler=${CXX_COMPILER}\n"
  "compiler_version_begin\n${_compiler_version}\ncompiler_version_end\n"
  "compiler_style=${COMPILER_STYLE}\n"
  "language_standard=c++20\n"
  "build_mode=${OBSERVATION_BUILD_MODE}\n"
  "hardware=${_host_os_name} ${_host_os_release}; "
  "${_host_processor}; logical_cores=${_host_logical_cores}\n"
  "provider=host compiler; compile-only; no runtime provider\n"
  "include_dir=${INCLUDE_DIR}\n"
  "extra_flags=${_flags_text}\n"
  "warmup_count=0\n"
  "repetition_count=1 per translation unit\n"
  "synchronization_boundary=compiler process exit\n"
  "timing_policy=local observation only; no threshold\n"
  "known_noise=filesystem and page cache, process scheduling, concurrent "
  "load, compiler-local state, and storage latency\n"
)

foreach(_source_name IN LISTS _sources)
  set(_source "${CMAKE_CURRENT_LIST_DIR}/${_source_name}")
  unset(_compile_input_file_arguments)
  if(COMPILER_STYLE STREQUAL "msvc")
    set(_object "${WORK_DIR}/${_source_name}.obj")
    file(TO_NATIVE_PATH "${CXX_COMPILER}" _compiler_native)
    file(TO_NATIVE_PATH "${_vcvarsall}" _vcvarsall_native)
    file(TO_NATIVE_PATH "${INCLUDE_DIR}" _include_native)
    file(TO_NATIVE_PATH "${_source}" _source_native)
    file(TO_NATIVE_PATH "${_object}" _object_native)
    set(_compile_batch "${WORK_DIR}/${_source_name}.bat")
    string(CONCAT _compile_batch_contents
      "@echo off\n"
      "call \"${_vcvarsall_native}\" ${_msvc_target_architecture} >nul\n"
      "if errorlevel 1 exit /b %errorlevel%\n"
      "\"${_compiler_native}\" /nologo /std:c++20 /EHsc "
      "\"/I${_include_native}\""
    )
    foreach(_flag IN LISTS COMPILE_FLAGS)
      string(APPEND _compile_batch_contents " ${_flag}")
    endforeach()
    string(APPEND _compile_batch_contents
      " /c \"${_source_native}\" \"/Fo${_object_native}\"\n"
      "exit /b %errorlevel%\n"
    )
    file(WRITE "${_compile_batch}" "${_compile_batch_contents}")
    set(_compile_command
      "${_command_interpreter}" /D /Q
    )
    set(_compile_input_file_arguments INPUT_FILE "${_compile_batch}")
  else()
    set(_object "${WORK_DIR}/${_source_name}.o")
    set(_compile_command
      "${CXX_COMPILER}"
      -std=c++20
      "-I${INCLUDE_DIR}"
      ${COMPILE_FLAGS}
      -c "${_source}"
      -o "${_object}"
    )
  endif()
  if(NOT EXISTS "${_source}")
    message(FATAL_ERROR "Observation source is missing: ${_source}")
  endif()

  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E time
      ${_compile_command}
    ${_compile_input_file_arguments}
    RESULT_VARIABLE _compile_result
    OUTPUT_VARIABLE _compile_stdout
    ERROR_VARIABLE _compile_stderr
  )
  if(NOT _compile_result EQUAL 0)
    message(FATAL_ERROR
      "Compile observation failed for ${_source_name} (${_compile_result}).\n"
      "stdout:\n${_compile_stdout}\n"
      "stderr:\n${_compile_stderr}"
    )
  endif()
  if(NOT EXISTS "${_object}")
    message(FATAL_ERROR "Compiler did not create ${_object}")
  endif()

  file(SIZE "${_object}" _object_size)
  file(SHA256 "${_source}" _source_sha256)
  file(SHA256 "${_object}" _object_sha256)
  string(REPLACE "." "_" _source_key "${_source_name}")
  set(_timing_observation "${_compile_stdout}\n${_compile_stderr}")
  string(REPLACE "\r\n" "\n" _timing_observation "${_timing_observation}")
  string(STRIP "${_timing_observation}" _timing_observation)
  string(REPLACE "\n" " | " _timing_observation "${_timing_observation}")

  file(APPEND "${OBSERVATION_FILE}"
    "source=${_source_name}\n"
    "workload=${_workload_${_source_key}}\n"
    "source_sha256=${_source_sha256}\n"
    "object_bytes=${_object_size}\n"
    "object_sha256=${_object_sha256}\n"
    "timing_observation=${_timing_observation}\n"
    "result=pass\n"
  )
endforeach()

message(STATUS
  "Wrote compile/object observations to ${OBSERVATION_FILE}"
)
