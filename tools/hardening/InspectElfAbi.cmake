cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ASC_CPP_HARDENING_LIBRARIES
   OR ASC_CPP_HARDENING_LIBRARIES STREQUAL "")
  message(FATAL_ERROR
    "ASC_CPP_HARDENING_LIBRARIES must name at least one shared library."
  )
endif()
if(NOT DEFINED ASC_CPP_HARDENING_OUTPUT
   OR ASC_CPP_HARDENING_OUTPUT STREQUAL "")
  message(FATAL_ERROR "ASC_CPP_HARDENING_OUTPUT is required.")
endif()

if(DEFINED ASC_CPP_HARDENING_SOURCE_DIR)
  cmake_path(
    ABSOLUTE_PATH ASC_CPP_HARDENING_SOURCE_DIR
    NORMALIZE
    OUTPUT_VARIABLE _source_dir
  )
else()
  cmake_path(
    ABSOLUTE_PATH CMAKE_CURRENT_LIST_DIR
    NORMALIZE
    OUTPUT_VARIABLE _tools_dir
  )
  cmake_path(GET _tools_dir PARENT_PATH _tools_parent)
  cmake_path(GET _tools_parent PARENT_PATH _source_dir)
endif()
cmake_path(
  ABSOLUTE_PATH ASC_CPP_HARDENING_OUTPUT
  NORMALIZE
  OUTPUT_VARIABLE _output
)
cmake_path(
  IS_PREFIX _source_dir
  "${_output}"
  NORMALIZE
  _output_inside_source
)
if(_output_inside_source)
  message(FATAL_ERROR
    "ELF ABI report must be outside the source tree: '${_output}'."
  )
endif()
cmake_path(GET _output PARENT_PATH _output_parent)
if(NOT IS_DIRECTORY "${_output_parent}")
  message(FATAL_ERROR
    "ELF ABI report parent does not exist: '${_output_parent}'."
  )
endif()

find_program(_readelf NAMES readelf llvm-readelf)
find_program(_nm NAMES nm llvm-nm)
find_program(_cxxfilt NAMES c++filt llvm-cxxfilt)

set(_skip_reason)
if(NOT _readelf)
  set(_skip_reason "readelf-unavailable")
elseif(NOT _nm)
  set(_skip_reason "nm-unavailable")
elseif(NOT _cxxfilt)
  set(_skip_reason "c++filt-unavailable")
endif()

set(_libraries ${ASC_CPP_HARDENING_LIBRARIES})
list(SORT _libraries)
foreach(_library IN LISTS _libraries)
  if(NOT EXISTS "${_library}")
    message(FATAL_ERROR "Shared library does not exist: '${_library}'.")
  endif()
endforeach()

if(_skip_reason)
  set(_report "format|asc-cpp-elf-abi-v1\n")
  string(APPEND _report "status|skipped|${_skip_reason}\n")
  file(WRITE "${_output}" "${_report}")
  message(STATUS "ASCCpp ELF ABI inspection skipped: ${_skip_reason}.")
  return()
endif()

set(_report "format|asc-cpp-elf-abi-v1\n")
foreach(_library IN LISTS _libraries)
  get_filename_component(_library_name "${_library}" NAME)
  execute_process(
    COMMAND "${_readelf}" --file-header "${_library}"
    RESULT_VARIABLE _header_result
    OUTPUT_VARIABLE _header_output
    ERROR_VARIABLE _header_error
  )
  if(NOT _header_result EQUAL 0)
    set(_report "format|asc-cpp-elf-abi-v1\n")
    string(APPEND _report
      "status|skipped|not-elf|${_library_name}\n"
    )
    file(WRITE "${_output}" "${_report}")
    message(STATUS
      "ASCCpp ELF ABI inspection skipped for '${_library_name}': "
      "not an inspectable ELF file."
    )
    return()
  endif()

  foreach(_field IN ITEMS Class Data OS/ABI Machine)
    string(
      REGEX MATCH
      "${_field}:[ \t]*([^\n\r]+)"
      _field_match
      "${_header_output}"
    )
    if(NOT _field_match)
      message(FATAL_ERROR
        "readelf did not report '${_field}' for '${_library_name}'."
      )
    endif()
    set(_field_value "${CMAKE_MATCH_1}")
    string(STRIP "${_field_value}" _field_value)
    string(TOLOWER "${_field}" _field_key)
    string(REPLACE "/" "-" _field_key "${_field_key}")
    string(APPEND _report
      "elf|${_library_name}|${_field_key}|${_field_value}\n"
    )
  endforeach()

  execute_process(
    COMMAND "${_readelf}" --dynamic --wide "${_library}"
    RESULT_VARIABLE _dynamic_result
    OUTPUT_VARIABLE _dynamic_output
    ERROR_VARIABLE _dynamic_error
  )
  if(NOT _dynamic_result EQUAL 0)
    message(FATAL_ERROR
      "readelf dynamic inspection failed for '${_library_name}': "
      "${_dynamic_error}"
    )
  endif()
  string(
    REGEX MATCHALL
    "\\(SONAME\\)[^\n\r]*\\[[^]]+\\]"
    _soname_rows
    "${_dynamic_output}"
  )
  foreach(_row IN LISTS _soname_rows)
    string(REGEX REPLACE ".*\\[([^]]+)\\].*" "\\1" _soname "${_row}")
    string(APPEND _report
      "soname|${_library_name}|${_soname}\n"
    )
  endforeach()
  string(
    REGEX MATCHALL
    "\\(NEEDED\\)[^\n\r]*\\[[^]]+\\]"
    _needed_rows
    "${_dynamic_output}"
  )
  set(_needed)
  foreach(_row IN LISTS _needed_rows)
    string(REGEX REPLACE ".*\\[([^]]+)\\].*" "\\1" _dependency "${_row}")
    list(APPEND _needed "${_dependency}")
  endforeach()
  list(SORT _needed)
  foreach(_dependency IN LISTS _needed)
    string(APPEND _report
      "needed|${_library_name}|${_dependency}\n"
    )
  endforeach()

  execute_process(
    COMMAND
      "${_nm}"
      --dynamic
      --defined-only
      --extern-only
      --format=posix
      "${_library}"
    RESULT_VARIABLE _nm_result
    OUTPUT_VARIABLE _nm_output
    ERROR_VARIABLE _nm_error
  )
  if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR
      "nm inspection failed for '${_library_name}': ${_nm_error}"
    )
  endif()
  string(REPLACE "\r\n" "\n" _nm_output "${_nm_output}")
  string(REPLACE "\r" "\n" _nm_output "${_nm_output}")
  string(REPLACE "\n" ";" _nm_rows "${_nm_output}")

  set(_symbols)
  foreach(_row IN LISTS _nm_rows)
    if(_row STREQUAL "")
      continue()
    endif()
    string(
      REGEX MATCH
      "^([^ \t]+)[ \t]+([^ \t]+)"
      _symbol_match
      "${_row}"
    )
    if(NOT _symbol_match)
      message(FATAL_ERROR
        "Could not parse nm output for '${_library_name}': '${_row}'."
      )
    endif()
    set(_mangled "${CMAKE_MATCH_1}")
    set(_type "${CMAKE_MATCH_2}")
    execute_process(
      COMMAND "${_cxxfilt}" "${_mangled}"
      RESULT_VARIABLE _cxxfilt_result
      OUTPUT_VARIABLE _demangled
      ERROR_VARIABLE _cxxfilt_error
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT _cxxfilt_result EQUAL 0)
      message(FATAL_ERROR
        "c++filt failed for '${_mangled}': ${_cxxfilt_error}"
      )
    endif()
    list(APPEND _symbols
      "${_demangled}|${_type}|${_mangled}"
    )
  endforeach()
  list(SORT _symbols)
  foreach(_symbol IN LISTS _symbols)
    string(REPLACE "|" ";" _fields "${_symbol}")
    list(GET _fields 0 _demangled)
    list(GET _fields 1 _type)
    list(GET _fields 2 _mangled)
    string(APPEND _report
      "symbol|${_library_name}|${_type}|${_mangled}|${_demangled}\n"
    )
  endforeach()
endforeach()
string(APPEND _report "status|passed\n")

file(WRITE "${_output}" "${_report}")
if(DEFINED ASC_CPP_HARDENING_BASELINE
   AND NOT ASC_CPP_HARDENING_BASELINE STREQUAL "")
  if(NOT EXISTS "${ASC_CPP_HARDENING_BASELINE}")
    message(FATAL_ERROR
      "ELF ABI baseline does not exist: "
      "'${ASC_CPP_HARDENING_BASELINE}'."
    )
  endif()
  file(READ "${ASC_CPP_HARDENING_BASELINE}" _expected)
  string(REPLACE "\r\n" "\n" _expected "${_expected}")
  string(REPLACE "\r" "\n" _expected "${_expected}")
  if(NOT _expected MATCHES "\n$")
    string(APPEND _expected "\n")
  endif()
  if(_expected MATCHES "^format\\|asc-cpp-elf-abi-digest-v1\n")
    string(
      REGEX MATCH
      "report-sha256\\|([0-9a-f]+)"
      _digest_match
      "${_expected}"
    )
    if(NOT _digest_match)
      message(FATAL_ERROR
        "ELF ABI digest baseline has no valid report-sha256 row: "
        "'${ASC_CPP_HARDENING_BASELINE}'."
      )
    endif()
    set(_expected_digest "${CMAKE_MATCH_1}")
    string(LENGTH "${_expected_digest}" _expected_digest_length)
    if(NOT _expected_digest_length EQUAL 64)
      message(FATAL_ERROR
        "ELF ABI digest baseline SHA-256 has length "
        "${_expected_digest_length}, expected 64."
      )
    endif()
    string(SHA256 _actual_digest "${_report}")
    if(NOT _actual_digest STREQUAL _expected_digest)
      message(FATAL_ERROR
        "ELF ABI observation digest '${_actual_digest}' differs from "
        "baseline '${_expected_digest}' in "
        "'${ASC_CPP_HARDENING_BASELINE}'."
      )
    endif()
  elseif(NOT _report STREQUAL _expected)
    message(FATAL_ERROR
      "ELF ABI observation differs from full baseline "
      "'${ASC_CPP_HARDENING_BASELINE}'."
    )
  endif()
endif()

list(LENGTH _libraries _library_count)
message(STATUS
  "ASCCpp ELF ABI inspection passed for ${_library_count} libraries."
)
