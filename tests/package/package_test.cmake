cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    ACTION
    SOURCE_DIR
    PRODUCER_BINARY_DIR
    CONSUMER_SOURCE_DIR
    GENERATOR
    ENABLE_CUDA
    WORK_DIR
)
  if(NOT DEFINED "${_required_variable}" OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

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

function(_assert_package_files package_directory)
  if(NOT EXISTS "${package_directory}/ASCCppConfig.cmake")
    message(FATAL_ERROR
      "Missing ASCCppConfig.cmake in package directory: ${package_directory}"
    )
  endif()
  if(NOT EXISTS "${package_directory}/ASCCppConfigVersion.cmake")
    message(FATAL_ERROR
      "Missing ASCCppConfigVersion.cmake in package directory: "
      "${package_directory}"
    )
  endif()
  file(
    GLOB_RECURSE _target_exports
    LIST_DIRECTORIES FALSE
    "${package_directory}/*Targets.cmake"
    "${package_directory}/*-targets.cmake"
  )
  set(_expected_target_exports
    ASCCppCoreTargets.cmake
    ASCCppUtilitiesTargets.cmake
    ASCCppExpressionTargets.cmake
    ASCCppRandomTargets.cmake
    ASCCppDenseTargets.cmake
    ASCCppSparseTargets.cmake
    ASCCppRandomDenseTargets.cmake
    ASCCppRandomSparseTargets.cmake
    ASCCppCppTargets.cmake
  )
  if(ENABLE_CUDA)
    list(APPEND _expected_target_exports
      ASCCppCoreCudaTargets.cmake
      ASCCppDenseCudaTargets.cmake
      ASCCppSparseCudaTargets.cmake
      ASCCppRandomCudaTargets.cmake
      ASCCppRandomDenseCudaTargets.cmake
      ASCCppRandomSparseCudaTargets.cmake
    )
  endif()
  foreach(_expected_export IN LISTS _expected_target_exports)
    if(NOT EXISTS "${package_directory}/${_expected_export}")
      message(FATAL_ERROR
        "Milestone 8 package is missing ${_expected_export}."
      )
    endif()
  endforeach()
  foreach(_target_export IN LISTS _target_exports)
    get_filename_component(_target_export_name "${_target_export}" NAME)
    if(NOT _target_export_name MATCHES
       "^ASCCpp(Core|Utilities|Expression|Random|Dense|Sparse|RandomDense|RandomSparse|Cpp|CoreCuda|DenseCuda|SparseCuda|RandomCuda|RandomDenseCuda|RandomSparseCuda)Targets(-[A-Za-z0-9_]+)?\\.cmake$")
      message(FATAL_ERROR
        "Unexpected Milestone 8 target export: ${_target_export_name}"
      )
    endif()
  endforeach()
endfunction()

function(
  _configure_consumer
  package_directory
  request_mode
  component
  expect_configure_success
  expect_found
  expect_core_target
  case_name
)
  string(MAKE_C_IDENTIFIER "${case_name}" _case_identifier)
  set(_consumer_build_dir
    "${WORK_DIR}/consumer builds with spaces/${_case_identifier}"
  )
  file(REMOVE_RECURSE "${_consumer_build_dir}")

  _generator_arguments(_generator_args)
  set(_command
    "${CMAKE_COMMAND}"
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${_consumer_build_dir}"
    ${_generator_args}
    "-DASCCpp_DIR:PATH=${package_directory}"
    "-DASCCPP_REQUEST_MODE:STRING=${request_mode}"
    "-DASCCPP_EXPECT_FOUND:BOOL=${expect_found}"
    "-DASCCPP_EXPECT_CORE_TARGET:BOOL=${expect_core_target}"
    "-DASCCPP_EXPECT_CUDA:BOOL=${ENABLE_CUDA}"
  )
  if(NOT component STREQUAL "")
    list(APPEND _command "-DASCCPP_REQUEST_COMPONENT:STRING=${component}")
  endif()

  execute_process(
    COMMAND ${_command}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(expect_configure_success)
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR
        "Consumer case '${case_name}' unexpectedly failed.\n"
        "stdout:\n${_stdout}\n"
        "stderr:\n${_stderr}"
      )
    endif()
    set(_build_command "${CMAKE_COMMAND}" --build "${_consumer_build_dir}")
    if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
      list(APPEND _build_command --config "${CONFIG}")
    endif()
    _run_success("Consumer case '${case_name}' build" ${_build_command})
  else()
    if(_result EQUAL 0)
      message(FATAL_ERROR
        "Required consumer case '${case_name}' unexpectedly configured."
      )
    endif()
    set(_combined_output "${_stdout}\n${_stderr}")
    if(NOT _combined_output MATCHES "ASCCpp")
      message(FATAL_ERROR
        "Required consumer case '${case_name}' failed without an ASCCpp "
        "diagnostic.\n${_combined_output}"
      )
    endif()
    if(component STREQUAL "")
      set(_expected_component cpp)
    else()
      set(_expected_component "${component}")
    endif()
    if(NOT _combined_output MATCHES "${_expected_component}")
      message(FATAL_ERROR
        "Required consumer case '${case_name}' did not identify "
        "'${_expected_component}'.\n${_combined_output}"
      )
    endif()
  endif()
endfunction()

function(_verify_milestone_7_package package_directory phase_name)
  _assert_package_files("${package_directory}")

  _configure_consumer(
    "${package_directory}"
    component
    core
    TRUE
    TRUE
    TRUE
    "${phase_name}_core_quiet"
  )
  _configure_consumer(
    "${package_directory}"
    required-component
    core
    TRUE
    TRUE
    TRUE
    "${phase_name}_core_required"
  )
  set(_available_components
    utilities
    expression
    random
    dense
    sparse
    random_dense
    random_sparse
    cpp
  )
  if(ENABLE_CUDA)
    list(APPEND _available_components
      core_cuda
      dense_cuda
      sparse_cuda
      random_cuda
      random_dense_cuda
      random_sparse_cuda
    )
  endif()
  foreach(_component IN LISTS _available_components)
    _configure_consumer(
      "${package_directory}"
      component
      "${_component}"
      TRUE
      TRUE
      TRUE
      "${phase_name}_${_component}_quiet"
    )
    _configure_consumer(
      "${package_directory}"
      required-component
      "${_component}"
      TRUE
      TRUE
      TRUE
      "${phase_name}_${_component}_required"
    )
  endforeach()

  _configure_consumer(
    "${package_directory}"
    core-with-optional-component
    random_dense
    TRUE
    TRUE
    TRUE
    "${phase_name}_core_optional_random_dense"
  )
  _configure_consumer(
    "${package_directory}"
    component
    definitely_unknown
    TRUE
    FALSE
    FALSE
    "${phase_name}_unknown_quiet"
  )
  _configure_consumer(
    "${package_directory}"
    required-component
    definitely_unknown
    FALSE
    FALSE
    FALSE
    "${phase_name}_unknown_required"
  )
  if(NOT ENABLE_CUDA)
    foreach(_provider IN ITEMS
        core_cuda
        dense_cuda
        sparse_cuda
        random_cuda
        random_dense_cuda
        random_sparse_cuda
    )
      _configure_consumer(
        "${package_directory}"
        component
        "${_provider}"
        TRUE
        FALSE
        FALSE
        "${phase_name}_${_provider}_unavailable_quiet"
      )
      _configure_consumer(
        "${package_directory}"
        required-component
        "${_provider}"
        FALSE
        FALSE
        FALSE
        "${phase_name}_${_provider}_unavailable_required"
      )
    endforeach()
  endif()
  _configure_consumer(
    "${package_directory}"
    no-component
    ""
    TRUE
    TRUE
    TRUE
    "${phase_name}_no_component_quiet"
  )
  _configure_consumer(
    "${package_directory}"
    required-no-component
    ""
    TRUE
    TRUE
    TRUE
    "${phase_name}_no_component_required"
  )
  _configure_consumer(
    "${package_directory}"
    optional-component
    random_dense
    TRUE
    TRUE
    TRUE
    "${phase_name}_optional_available_random_dense"
  )
endfunction()

function(_find_installed_package_directory prefix output_variable)
  file(
    GLOB_RECURSE _config_files
    LIST_DIRECTORIES FALSE
    "${prefix}/ASCCppConfig.cmake"
  )
  list(LENGTH _config_files _config_count)
  if(NOT _config_count EQUAL 1)
    message(FATAL_ERROR
      "Expected exactly one installed ASCCppConfig.cmake below '${prefix}', "
      "found ${_config_count}: ${_config_files}"
    )
  endif()
  list(GET _config_files 0 _config_file)
  get_filename_component(_config_directory "${_config_file}" DIRECTORY)
  set("${output_variable}" "${_config_directory}" PARENT_SCOPE)
endfunction()

function(_filesystem_registry_snapshot registry_directory output_variable)
  if(NOT EXISTS "${registry_directory}")
    set("${output_variable}" "<absent>" PARENT_SCOPE)
    return()
  endif()

  file(
    GLOB_RECURSE _entries
    LIST_DIRECTORIES FALSE
    RELATIVE "${registry_directory}"
    "${registry_directory}/*"
  )
  list(SORT _entries)
  set(_snapshot)
  foreach(_entry IN LISTS _entries)
    file(SHA256 "${registry_directory}/${_entry}" _entry_hash)
    string(APPEND _snapshot "${_entry}=${_entry_hash}\n")
  endforeach()
  string(SHA256 _snapshot_hash "${_snapshot}")
  set("${output_variable}" "${_snapshot_hash}" PARENT_SCOPE)
endfunction()

function(_windows_registry_snapshot output_variable)
  set(_registry_root "HKCU/Software/Kitware/CMake/Packages")
  cmake_host_system_information(
    RESULT _packages
    QUERY WINDOWS_REGISTRY "${_registry_root}" SUBKEYS
    ERROR_VARIABLE _packages_error
  )
  if(_packages_error)
    set("${output_variable}" "<absent>" PARENT_SCOPE)
    return()
  endif()

  list(SORT _packages)
  set(_snapshot)
  foreach(_package IN LISTS _packages)
    cmake_host_system_information(
      RESULT _value_names
      QUERY WINDOWS_REGISTRY "${_registry_root}/${_package}" VALUE_NAMES
      ERROR_VARIABLE _values_error
    )
    if(_values_error)
      string(APPEND _snapshot "${_package}=<unreadable>\n")
      continue()
    endif()
    list(SORT _value_names)
    foreach(_value_name IN LISTS _value_names)
      cmake_host_system_information(
        RESULT _value
        QUERY WINDOWS_REGISTRY "${_registry_root}/${_package}"
        VALUE "${_value_name}"
        ERROR_VARIABLE _value_error
      )
      if(_value_error)
        set(_value "<unreadable>")
      endif()
      string(APPEND _snapshot "${_package}/${_value_name}=${_value}\n")
    endforeach()
  endforeach()
  string(SHA256 _snapshot_hash "${_snapshot}")
  set("${output_variable}" "${_snapshot_hash}" PARENT_SCOPE)
endfunction()

if(ACTION STREQUAL "VERIFY_BUILD_TREE")
  if(NOT DEFINED PACKAGE_DIR OR PACKAGE_DIR STREQUAL "")
    message(FATAL_ERROR "PACKAGE_DIR is required for VERIFY_BUILD_TREE.")
  endif()
  file(REMOVE_RECURSE "${WORK_DIR}")
  file(MAKE_DIRECTORY "${WORK_DIR}")
  _verify_milestone_7_package("${PACKAGE_DIR}" build_tree)

  set(_copied_package_directory
    "${WORK_DIR}/copied build tree package path with spaces"
  )
  file(MAKE_DIRECTORY "${_copied_package_directory}")
  file(
    COPY "${PACKAGE_DIR}/"
    DESTINATION "${_copied_package_directory}"
  )
  _verify_milestone_7_package(
    "${_copied_package_directory}"
    copied_build_tree
  )
elseif(ACTION STREQUAL "INSTALL_AND_RELOCATE")
  file(REMOVE_RECURSE "${WORK_DIR}")
  file(MAKE_DIRECTORY "${WORK_DIR}")

  set(_original_prefix "${WORK_DIR}/original install prefix")
  set(_relocated_prefix "${WORK_DIR}/relocated ASCCpp prefix with spaces")
  set(_install_command
    "${CMAKE_COMMAND}"
    --install "${PRODUCER_BINARY_DIR}"
    --prefix "${_original_prefix}"
  )
  if(DEFINED CONFIG AND NOT CONFIG STREQUAL "")
    list(APPEND _install_command --config "${CONFIG}")
  endif()
  _run_success("ASCCpp installation" ${_install_command})

  _find_installed_package_directory(
    "${_original_prefix}"
    _installed_package_directory
  )
  _verify_milestone_7_package("${_installed_package_directory}" installed)

  file(RENAME "${_original_prefix}" "${_relocated_prefix}")
  _find_installed_package_directory(
    "${_relocated_prefix}"
    _relocated_package_directory
  )
  _verify_milestone_7_package("${_relocated_package_directory}" relocated)
elseif(ACTION STREQUAL "VERIFY_REGISTRY")
  if(NOT DEFINED ASCCMAKE_DIR OR ASCCMAKE_DIR STREQUAL "")
    message(FATAL_ERROR "ASCCMAKE_DIR is required for VERIFY_REGISTRY.")
  endif()
  file(REMOVE_RECURSE "${WORK_DIR}")
  file(MAKE_DIRECTORY "${WORK_DIR}")

  set(_cmake_sources "${SOURCE_DIR}/CMakeLists.txt")
  file(
    GLOB_RECURSE _cmake_modules
    LIST_DIRECTORIES FALSE
    "${SOURCE_DIR}/cmake/*.cmake"
    "${SOURCE_DIR}/cmake/*.cmake.in"
  )
  list(APPEND _cmake_sources ${_cmake_modules})
  foreach(_cmake_source IN LISTS _cmake_sources)
    file(READ "${_cmake_source}" _contents)
    string(REGEX REPLACE "#[^\n]*" "" _without_comments "${_contents}")
    if(_without_comments MATCHES
       "(^|[\n\r])[ \t]*export[ \t\r\n]*\\([ \t\r\n]*PACKAGE([ \t\r\n]|\\))")
      message(FATAL_ERROR
        "Package-registry mutation is prohibited: ${_cmake_source} calls "
        "export(PACKAGE)."
      )
    endif()
  endforeach()

  set(_isolated_home "${WORK_DIR}/isolated user home")
  set(_registry_directory "${_isolated_home}/.cmake/packages")
  file(MAKE_DIRECTORY "${_isolated_home}")
  if(WIN32)
    _windows_registry_snapshot(_registry_before)
  else()
    _filesystem_registry_snapshot("${_registry_directory}" _registry_before)
  endif()

  _generator_arguments(_generator_args)
  set(_nested_binary "${WORK_DIR}/fresh producer build with spaces")
  set(_configure_command
    "${CMAKE_COMMAND}"
    -S "${SOURCE_DIR}"
    -B "${_nested_binary}"
    ${_generator_args}
    "-DASCCMake_DIR:PATH=${ASCCMAKE_DIR}"
    "-DBUILD_TESTING:BOOL=OFF"
    "-DASC_CPP_BUILD_TESTING:BOOL=OFF"
    "-DASC_CPP_INSTALL:BOOL=ON"
    "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY:BOOL=ON"
  )
  _run_success(
    "fresh registry-isolated ASCCpp configuration"
    "${CMAKE_COMMAND}" -E env
    "HOME=${_isolated_home}"
    "USERPROFILE=${_isolated_home}"
    ${_configure_command}
  )

  if(WIN32)
    _windows_registry_snapshot(_registry_after)
  else()
    _filesystem_registry_snapshot("${_registry_directory}" _registry_after)
  endif()
  if(NOT _registry_before STREQUAL _registry_after)
    message(FATAL_ERROR
      "The CMake user package registry changed during configuration.\n"
      "  before: ${_registry_before}\n"
      "  after:  ${_registry_after}"
    )
  endif()
else()
  message(FATAL_ERROR "Unknown package test ACTION: ${ACTION}")
endif()
