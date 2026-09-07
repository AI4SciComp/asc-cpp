include_guard(GLOBAL)

function(_asc_cpp_hardening_get_property target property output_variable)
  get_property(
    _is_set
    TARGET "${target}"
    PROPERTY "${property}"
    SET
  )
  if(_is_set)
    get_target_property(_value "${target}" "${property}")
  else()
    set(_value)
  endif()
  set("${output_variable}" "${_value}" PARENT_SCOPE)
endfunction()

function(_asc_cpp_hardening_to_csv input_variable output_variable)
  set(_values "${${input_variable}}")
  if(_values)
    string(JOIN "," _csv ${_values})
  else()
    set(_csv)
  endif()
  set("${output_variable}" "${_csv}" PARENT_SCOPE)
endfunction()

function(_asc_cpp_hardening_collect_target_headers target output_variable)
  _asc_cpp_hardening_get_property(
    "${target}"
    HEADER_SETS
    _private_header_sets
  )
  _asc_cpp_hardening_get_property(
    "${target}"
    INTERFACE_HEADER_SETS
    _interface_header_sets
  )
  set(_header_sets ${_private_header_sets} ${_interface_header_sets})
  list(REMOVE_DUPLICATES _header_sets)

  set(_headers)
  foreach(_header_set IN LISTS _header_sets)
    if(_header_set STREQUAL "HEADERS")
      set(_property HEADER_SET)
    else()
      set(_property "HEADER_SET_${_header_set}")
    endif()
    _asc_cpp_hardening_get_property(
      "${target}"
      "${_property}"
      _set_headers
    )
    list(APPEND _headers ${_set_headers})
  endforeach()
  list(REMOVE_DUPLICATES _headers)

  set(_relative_headers)
  foreach(_header IN LISTS _headers)
    cmake_path(
      RELATIVE_PATH _header
      BASE_DIRECTORY "${PROJECT_SOURCE_DIR}/include"
      OUTPUT_VARIABLE _relative_header
    )
    if(_relative_header MATCHES "^\\.\\.")
      message(FATAL_ERROR
        "Target '${target}' has a public header outside include/: "
        "'${_header}'."
      )
    endif()
    list(APPEND _relative_headers "${_relative_header}")
  endforeach()
  list(SORT _relative_headers)
  set("${output_variable}" "${_relative_headers}" PARENT_SCOPE)
endfunction()

function(asc_cpp_hardening_check_target_inventory)
  set(_one_value_arguments BASELINE HEADER_OWNERS OUTPUT)
  cmake_parse_arguments(
    PARSE_ARGV 0
    _argument
    ""
    "${_one_value_arguments}"
    ""
  )
  if(_argument_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "asc_cpp_hardening_check_target_inventory received unknown "
      "arguments: ${_argument_UNPARSED_ARGUMENTS}"
    )
  endif()
  foreach(_required IN LISTS _one_value_arguments)
    if(NOT DEFINED _argument_${_required}
       OR _argument_${_required} STREQUAL "")
      message(FATAL_ERROR
        "asc_cpp_hardening_check_target_inventory requires ${_required}."
      )
    endif()
  endforeach()
  foreach(_input IN ITEMS
      "${_argument_BASELINE}"
      "${_argument_HEADER_OWNERS}"
  )
    if(NOT EXISTS "${_input}")
      message(FATAL_ERROR
        "Target-inventory baseline does not exist: '${_input}'."
      )
    endif()
  endforeach()

  cmake_path(
    ABSOLUTE_PATH _argument_OUTPUT
    NORMALIZE
    OUTPUT_VARIABLE _absolute_output
  )
  cmake_path(
    IS_PREFIX PROJECT_SOURCE_DIR
    "${_absolute_output}"
    NORMALIZE
    _output_inside_source
  )
  if(_output_inside_source)
    message(FATAL_ERROR
      "Target-inventory report must be outside the source tree: "
      "'${_absolute_output}'."
    )
  endif()
  cmake_path(GET _absolute_output PARENT_PATH _output_parent)
  if(NOT IS_DIRECTORY "${_output_parent}")
    file(MAKE_DIRECTORY "${_output_parent}")
  endif()

  file(STRINGS "${_argument_HEADER_OWNERS}" _header_owner_lines)
  set(_expected_all_headers)
  foreach(_line IN LISTS _header_owner_lines)
    if(_line STREQUAL "" OR _line MATCHES "^#")
      continue()
    endif()
    string(REPLACE "|" ";" _fields "${_line}")
    list(LENGTH _fields _field_count)
    if(NOT _field_count EQUAL 2)
      message(FATAL_ERROR
        "Malformed header-owner baseline row: '${_line}'."
      )
    endif()
    list(GET _fields 0 _header)
    list(GET _fields 1 _owner)
    list(APPEND "_expected_headers_${_owner}" "${_header}")
    list(APPEND _expected_all_headers "${_header}")
  endforeach()
  list(SORT _expected_all_headers)

  file(STRINGS "${_argument_BASELINE}" _baseline_lines)
  set(_target_names)
  foreach(_line IN LISTS _baseline_lines)
    if(NOT _line MATCHES "^target\\|")
      continue()
    endif()
    string(REPLACE "|" ";" _fields "${_line}")
    list(LENGTH _fields _field_count)
    if(NOT _field_count EQUAL 10)
      message(FATAL_ERROR
        "Malformed target baseline row: '${_line}'."
      )
    endif()
    list(GET _fields 1 _target)
    list(APPEND _target_names "${_target}")
    list(GET _fields 2 "_expected_alias_${_target}")
    list(GET _fields 3 "_expected_kind_${_target}")
    list(GET _fields 4 "_expected_export_${_target}")
    list(GET _fields 5 "_expected_output_${_target}")
    list(GET _fields 6 "_expected_static_macro_${_target}")
    list(GET _fields 7 "_expected_direct_${_target}")
    list(GET _fields 8 "_expected_public_${_target}")
    list(GET _fields 9 "_expected_provider_${_target}")
  endforeach()

  set(_report "format|asc-cpp-target-inventory-v1\n")
  if(BUILD_SHARED_LIBS)
    string(APPEND _report "linkage|shared\n")
  else()
    string(APPEND _report "linkage|static\n")
  endif()
  if(ASC_CPP_ENABLE_CUDA)
    string(APPEND _report "cuda|enabled\n")
  else()
    string(APPEND _report "cuda|disabled\n")
  endif()

  set(_actual_all_headers)
  set(_checked_target_count 0)
  foreach(_target IN LISTS _target_names)
    if(_expected_provider_${_target} STREQUAL "lapack"
       AND NOT ASC_CPP_ENABLE_LAPACK)
      string(APPEND _report "target|${_target}|not-built|lapack-disabled\n")
      continue()
    endif()
    if(_expected_provider_${_target} STREQUAL "cuda"
       AND NOT ASC_CPP_ENABLE_CUDA)
      string(APPEND _report "target|${_target}|skipped|cuda-disabled\n")
      continue()
    endif()
    if(NOT TARGET "${_target}")
      message(FATAL_ERROR "Required product target is absent: '${_target}'.")
    endif()
    if(NOT TARGET "${_expected_alias_${_target}}")
      message(FATAL_ERROR
        "Required public alias is absent: "
        "'${_expected_alias_${_target}}'."
      )
    endif()

    get_target_property(_type "${_target}" TYPE)
    if(_type STREQUAL "INTERFACE_LIBRARY")
      set(_kind INTERFACE)
    elseif(_type STREQUAL "STATIC_LIBRARY"
           OR _type STREQUAL "SHARED_LIBRARY")
      set(_kind COMPILED)
    else()
      message(FATAL_ERROR
        "Unexpected type '${_type}' for product target '${_target}'."
      )
    endif()
    if(NOT "${_kind}" STREQUAL "${_expected_kind_${_target}}")
      message(FATAL_ERROR
        "Target '${_target}' kind '${_kind}' differs from baseline "
        "'${_expected_kind_${_target}}'."
      )
    endif()

    _asc_cpp_hardening_get_property("${_target}" EXPORT_NAME _export_name)
    _asc_cpp_hardening_get_property("${_target}" OUTPUT_NAME _output_name)
    if(NOT "${_export_name}" STREQUAL "${_expected_export_${_target}}")
      message(FATAL_ERROR
        "Target '${_target}' EXPORT_NAME '${_export_name}' differs from "
        "baseline '${_expected_export_${_target}}'."
      )
    endif()
    if(NOT "${_output_name}" STREQUAL "${_expected_output_${_target}}")
      message(FATAL_ERROR
        "Target '${_target}' OUTPUT_NAME '${_output_name}' differs from "
        "baseline '${_expected_output_${_target}}'."
      )
    endif()

    if(_kind STREQUAL "INTERFACE")
      _asc_cpp_hardening_get_property(
        "${_target}"
        INTERFACE_LINK_LIBRARIES
        _direct_links
      )
    else()
      _asc_cpp_hardening_get_property(
        "${_target}"
        LINK_LIBRARIES
        _direct_links
      )
    endif()
    _asc_cpp_hardening_to_csv(_direct_links _direct_csv)
    if(NOT "${_direct_csv}" STREQUAL "${_expected_direct_${_target}}")
      message(FATAL_ERROR
        "Target '${_target}' direct links '${_direct_csv}' differ from "
        "baseline '${_expected_direct_${_target}}'."
      )
    endif()

    _asc_cpp_hardening_get_property(
      "${_target}"
      INTERFACE_LINK_LIBRARIES
      _interface_links
    )
    set(_public_links)
    foreach(_link IN LISTS _interface_links)
      if(NOT _link MATCHES "^\\$<LINK_ONLY:")
        list(APPEND _public_links "${_link}")
      endif()
    endforeach()
    _asc_cpp_hardening_to_csv(_public_links _public_csv)
    if(NOT "${_public_csv}" STREQUAL "${_expected_public_${_target}}")
      message(FATAL_ERROR
        "Target '${_target}' public links '${_public_csv}' differ from "
        "baseline '${_expected_public_${_target}}'."
      )
    endif()

    _asc_cpp_hardening_get_property(
      "${_target}"
      INTERFACE_COMPILE_FEATURES
      _compile_features
    )
    if(NOT "cxx_std_20" IN_LIST _compile_features)
      message(FATAL_ERROR
        "Target '${_target}' does not propagate cxx_std_20."
      )
    endif()
    _asc_cpp_hardening_get_property("${_target}" CXX_STANDARD _cxx_standard)
    _asc_cpp_hardening_get_property(
      "${_target}"
      CXX_STANDARD_REQUIRED
      _cxx_standard_required
    )
    _asc_cpp_hardening_get_property(
      "${_target}"
      CXX_EXTENSIONS
      _cxx_extensions
    )
    if(NOT _cxx_standard STREQUAL "20"
       OR NOT _cxx_standard_required
       OR _cxx_extensions)
      message(FATAL_ERROR
        "Target '${_target}' does not have strict local C++20 "
        "properties."
      )
    endif()

    _asc_cpp_hardening_get_property(
      "${_target}"
      INTERFACE_COMPILE_DEFINITIONS
      _interface_definitions
    )
    if(_type STREQUAL "STATIC_LIBRARY")
      set(_expected_definition "${_expected_static_macro_${_target}}")
    else()
      set(_expected_definition)
    endif()
    if(NOT "${_interface_definitions}" STREQUAL "${_expected_definition}")
      message(FATAL_ERROR
        "Target '${_target}' interface definitions "
        "'${_interface_definitions}' differ from expected "
        "'${_expected_definition}'."
      )
    endif()

    _asc_cpp_hardening_collect_target_headers("${_target}" _actual_headers)
    set(_expected_headers "${_expected_headers_${_target}}")
    list(SORT _expected_headers)
    if(NOT "${_actual_headers}" STREQUAL "${_expected_headers}")
      message(FATAL_ERROR
        "Target '${_target}' public header file set differs from "
        "'${_argument_HEADER_OWNERS}'."
      )
    endif()
    list(APPEND _actual_all_headers ${_actual_headers})
    list(LENGTH _actual_headers _header_count)
    math(EXPR _checked_target_count "${_checked_target_count} + 1")
    string(APPEND _report
      "target|${_target}|passed|${_type}|${_direct_csv}|"
      "${_header_count}\n"
    )
  endforeach()

  list(REMOVE_DUPLICATES _actual_all_headers)
  list(SORT _actual_all_headers)
  set(_expected_configuration_headers)
  foreach(_header IN LISTS _expected_all_headers)
    if(_header MATCHES "^asc/dense/providers/lapack")
      if(ASC_CPP_ENABLE_LAPACK)
        list(APPEND _expected_configuration_headers "${_header}")
      endif()
    elseif(_header MATCHES "/providers/")
      if(ASC_CPP_ENABLE_CUDA)
        list(APPEND _expected_configuration_headers "${_header}")
      endif()
    else()
      list(APPEND _expected_configuration_headers "${_header}")
    endif()
  endforeach()
  if(NOT "${_actual_all_headers}"
     STREQUAL "${_expected_configuration_headers}")
    message(FATAL_ERROR
      "Configured target header union differs from the ownership baseline."
    )
  endif()

  string(APPEND _report "status|passed\n")
  string(APPEND _report "checked-target-count|${_checked_target_count}\n")
  file(WRITE "${_absolute_output}" "${_report}")
  message(STATUS
    "ASCCpp target inventory passed for ${_checked_target_count} targets."
  )
endfunction()
