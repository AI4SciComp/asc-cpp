cmake_minimum_required(VERSION 3.25)

foreach(_required_variable IN ITEMS SOURCE_DIR TARGET_INVENTORY)
  if(NOT DEFINED "${_required_variable}"
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()
if(NOT EXISTS "${TARGET_INVENTORY}")
  message(FATAL_ERROR "Target inventory was not generated: ${TARGET_INVENTORY}")
endif()

file(STRINGS "${TARGET_INVENTORY}" _target_records ENCODING UTF-8)
foreach(_record IN LISTS _target_records)
  if(NOT _record MATCHES "^([^|]+)\\|([^|]+)\\|([^|]+)\\|([^|]+)$")
    message(FATAL_ERROR "Malformed target inventory record: ${_record}")
  endif()
  set(_name "${CMAKE_MATCH_2}")
  set(_type "${CMAKE_MATCH_3}")
  if(_name MATCHES "^ASC::" OR _name MATCHES "^asc_")
    message(FATAL_ERROR
      "architecture baseline must expose no ASC product target: ${_name} (${_type})."
    )
  endif()
  string(CONCAT _production_type_pattern
    "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|"
    "OBJECT_LIBRARY|INTERFACE_LIBRARY|UNKNOWN_LIBRARY)$"
  )
  if(_type MATCHES "${_production_type_pattern}")
    message(FATAL_ERROR
      "architecture baseline must declare no executable or library target: "
      "${_name} (${_type})."
    )
  endif()
endforeach()

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
      "Package-registry mutation is prohibited: ${_cmake_source}."
    )
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
string(REGEX MATCH "project\\([^)]*\\)" _project_declaration "${_root_cmake}")
string(
  REGEX REPLACE "[ \t\r\n]+" " "
  _project_declaration "${_project_declaration}"
)
if(NOT _project_declaration MATCHES "^project\\( ASCCpp( |\\))"
   OR NOT _project_declaration MATCHES " VERSION 0\\.0\\.0( |\\))"
   OR NOT _project_declaration MATCHES " LANGUAGES NONE( |\\))")
  message(FATAL_ERROR
    "architecture baseline must declare project ASCCpp 0.0.0 with LANGUAGES NONE."
  )
endif()
