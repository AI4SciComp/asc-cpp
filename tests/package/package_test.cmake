cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS
    ACTION
    SOURCE_DIR
    PRODUCER_BINARY_DIR
    CONSUMER_SOURCE_DIR
    GENERATOR
    WORK_DIR
    ENABLE_CUDA
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
  set(_expected_target_exports
    ASCCppCoreTargets.cmake
    ASCCppUtilitiesTargets.cmake
    ASCCppExpressionTargets.cmake
    ASCCppDenseTargets.cmake
    ASCCppSparseTargets.cmake
    ASCCppRandomTargets.cmake
    ASCCppRandomDenseTargets.cmake
    ASCCppRandomSparseTargets.cmake
    ASCCppCppTargets.cmake
  )
  if(ENABLE_CUDA)
    list(APPEND
      _expected_target_exports
      ASCCppCoreCudaTargets.cmake
      ASCCppDenseCudaTargets.cmake
      ASCCppSparseCudaTargets.cmake
      ASCCppRandomCudaTargets.cmake
      ASCCppRandomDenseCudaTargets.cmake
      ASCCppRandomSparseCudaTargets.cmake
    )
  endif()
  foreach(_file IN ITEMS
      ASCCppConfig.cmake
      ASCCppConfigVersion.cmake
      ${_expected_target_exports}
  )
    if(NOT EXISTS "${package_directory}/${_file}")
      message(FATAL_ERROR "Missing ${_file} in ${package_directory}.")
    endif()
  endforeach()
  file(
    GLOB_RECURSE _target_exports
    LIST_DIRECTORIES FALSE
    "${package_directory}/*Targets.cmake"
    "${package_directory}/*-targets.cmake"
  )
  list(LENGTH _target_exports _target_export_count)
  list(LENGTH _expected_target_exports _expected_target_export_count)
  if(NOT _target_export_count EQUAL _expected_target_export_count)
    message(FATAL_ERROR
      "Milestone 8 component export count differs from this build: "
      "${_target_exports}"
    )
  endif()
  foreach(_target_export IN LISTS _target_exports)
    get_filename_component(_target_export_name "${_target_export}" NAME)
    if(NOT _target_export_name IN_LIST _expected_target_exports)
      message(FATAL_ERROR
        "Unexpected Milestone 8 target export: ${_target_export_name}"
      )
    endif()
  endforeach()
endfunction()

function(_assert_installed_metadata_has_no_build_paths package_directory)
  file(
    GLOB_RECURSE _metadata_files
    LIST_DIRECTORIES FALSE
    "${package_directory}/*.cmake"
  )
  foreach(_metadata_file IN LISTS _metadata_files)
    file(READ "${_metadata_file}" _metadata)
    foreach(_forbidden_path IN LISTS ARGN)
      if(_forbidden_path STREQUAL "")
        continue()
      endif()
      file(TO_CMAKE_PATH "${_forbidden_path}" _forbidden_cmake_path)
      string(FIND "${_metadata}" "${_forbidden_cmake_path}" _path_position)
      if(NOT _path_position EQUAL -1)
        message(FATAL_ERROR
          "Installed package metadata '${_metadata_file}' exposes "
          "forbidden build/source path '${_forbidden_cmake_path}'."
        )
      endif()
    endforeach()
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
  asc_cpp_prepare_test_workspace(
    WORK_DIR "${_consumer_build_dir}"
    ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
    GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
  )

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
    "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY:BOOL=ON"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
  )
  if(NOT component STREQUAL "")
    list(APPEND _command "-DASCCPP_REQUEST_COMPONENT:STRING=${component}")
  endif()
  set(_provider_components
    core_cuda
    dense_cuda
    sparse_cuda
    random_cuda
    random_dense_cuda
    random_sparse_cuda
  )
  if(ENABLE_CUDA
     AND (NOT component IN_LIST _provider_components
          OR request_mode STREQUAL "dense-with-optional-component"))
    list(APPEND
      _command
      "-DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit:BOOL=TRUE"
    )
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
        "Required case '${case_name}' lacks an ASCCpp diagnostic.\n"
        "${_combined_output}"
      )
    endif()
    if(component STREQUAL "")
      set(_expected_component cpp)
    else()
      set(_expected_component "${component}")
    endif()
    if(NOT _combined_output MATCHES "${_expected_component}")
      message(FATAL_ERROR
        "Required case '${case_name}' did not identify "
        "'${_expected_component}'.\n${_combined_output}"
      )
    endif()
  endif()
endfunction()

function(_verify_milestone_8_package package_directory phase_name)
  _assert_package_files("${package_directory}")

  set(_available_components
    core utilities expression dense sparse random
    random_dense random_sparse cpp
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
    foreach(_mode IN ITEMS component required-component)
      _configure_consumer(
        "${package_directory}"
        "${_mode}"
        "${_component}"
        TRUE
        TRUE
        TRUE
        "${phase_name}_${_component}_${_mode}"
      )
    endforeach()
  endforeach()

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
        "${phase_name}_${_provider}_quiet_unavailable"
      )
      _configure_consumer(
        "${package_directory}"
        required-component
        "${_provider}"
        FALSE
        FALSE
        FALSE
        "${phase_name}_${_provider}_required_unavailable"
      )
    endforeach()
  else()
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
        dense-with-optional-component
        "${_provider}"
        TRUE
        TRUE
        TRUE
        "${phase_name}_${_provider}_optional_without_toolkit"
      )
    endforeach()
  endif()

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
  _configure_consumer(
    "${package_directory}"
    optional-component
    definitely_unknown
    TRUE
    TRUE
    FALSE
    "${phase_name}_optional_unknown"
  )
  _configure_consumer(
    "${package_directory}"
    core-with-optional-component
    definitely_unknown
    TRUE
    TRUE
    TRUE
    "${phase_name}_core_with_optional_unknown"
  )
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
      "Expected one ASCCppConfig.cmake below '${prefix}', found "
      "${_config_count}: ${_config_files}"
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
  _verify_milestone_8_package("${PACKAGE_DIR}" build_tree)

  set(_copied_package_directory
    "${WORK_DIR}/copied build tree package path with spaces"
  )
  file(MAKE_DIRECTORY "${_copied_package_directory}")
  file(COPY "${PACKAGE_DIR}/" DESTINATION "${_copied_package_directory}")
  _verify_milestone_8_package(
    "${_copied_package_directory}" copied_build_tree
  )
elseif(ACTION STREQUAL "INSTALL_AND_RELOCATE")
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
  _assert_installed_metadata_has_no_build_paths(
    "${_installed_package_directory}"
    "${SOURCE_DIR}"
    "${PRODUCER_BINARY_DIR}"
  )
  _verify_milestone_8_package("${_installed_package_directory}" installed)

  file(RENAME "${_original_prefix}" "${_relocated_prefix}")
  _find_installed_package_directory(
    "${_relocated_prefix}"
    _relocated_package_directory
  )
  _assert_installed_metadata_has_no_build_paths(
    "${_relocated_package_directory}"
    "${SOURCE_DIR}"
    "${PRODUCER_BINARY_DIR}"
    "${_original_prefix}"
  )
  _verify_milestone_8_package(
    "${_relocated_package_directory}" relocated
  )
elseif(ACTION STREQUAL "VERIFY_REGISTRY")
  if(NOT DEFINED ASCCMAKE_DIR OR ASCCMAKE_DIR STREQUAL "")
    message(FATAL_ERROR "ASCCMAKE_DIR is required for VERIFY_REGISTRY.")
  endif()
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
    "-DASC_CPP_ENABLE_CUDA:BOOL=${ENABLE_CUDA}"
    "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY:BOOL=ON"
  )
  if(ENABLE_CUDA AND DEFINED CUDA_ARCHITECTURES
     AND NOT CUDA_ARCHITECTURES STREQUAL "")
    list(APPEND
      _configure_command
      "-DCMAKE_CUDA_ARCHITECTURES:STRING=${CUDA_ARCHITECTURES}"
    )
  endif()
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
      "CMake user package registry changed.\n"
      "  before: ${_registry_before}\n"
      "  after:  ${_registry_after}"
    )
  endif()
else()
  message(FATAL_ERROR "Unknown package test ACTION: ${ACTION}")
endif()
