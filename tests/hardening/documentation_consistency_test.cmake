cmake_minimum_required(VERSION 3.24)

foreach(_required_variable IN ITEMS
    ASC_CPP_SOURCE_DIR
    ASC_CPP_EXPECTED_PREDECESSOR_COMMIT)
  if(NOT DEFINED ${_required_variable}
     OR "${${_required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_required_variable} is required.")
  endif()
endforeach()

set(_security_file "${ASC_CPP_SOURCE_DIR}/SECURITY.md")
set(_capability_file
  "${ASC_CPP_SOURCE_DIR}/docs/development/asc-cpp-architecture/capability-manifest.yaml"
)
set(_roadmap_file
  "${ASC_CPP_SOURCE_DIR}/docs/development/asc-cpp-architecture/release-roadmap.md"
)
foreach(_required_file IN ITEMS
    "${_security_file}"
    "${_capability_file}"
    "${_roadmap_file}")
  if(NOT EXISTS "${_required_file}")
    message(FATAL_ERROR "Required policy input does not exist: ${_required_file}")
  endif()
endforeach()

file(READ "${_security_file}" _security)
file(READ "${_capability_file}" _capability)
file(READ "${_roadmap_file}" _roadmap)

foreach(_security_text IN ITEMS
    "unreleased `0.9.0` Milestone 8 correction candidate"
    "Core, Utilities, Expression, Dense, Sparse, and Random"
    "Core, Dense, Sparse, and Random CUDA facets"
    "Dense and Sparse Random storage facets"
    "CMake, test, hardening, ABI, benchmark, and CI tooling"
    "unsafe path construction, traversal, symlink escape, recursive deletion"
    "misleading required/optional component availability"
    "CUDA facets are explicit optional providers")
  string(FIND "${_security}" "${_security_text}" _security_position)
  if(_security_position EQUAL -1)
    message(FATAL_ERROR
      "SECURITY.md is missing current Milestone 8 boundary text: "
      "${_security_text}"
    )
  endif()
endforeach()

foreach(_stale_security_text IN ITEMS
    "Milestone 1 Core candidate"
    "All numerical containers, kernels, optimized providers, and GPU backends "
    "are outside")
  string(FIND "${_security}" "${_stale_security_text}"
    _stale_security_position
  )
  if(NOT _stale_security_position EQUAL -1)
    message(FATAL_ERROR
      "SECURITY.md contains stale scope text: ${_stale_security_text}"
    )
  endif()
endforeach()

string(FIND "${_capability}"
  "status: \"milestone-8-publication-checkpoint-b\""
  _capability_status_position
)
if(_capability_status_position EQUAL -1)
  message(FATAL_ERROR
    "The capability manifest is not at Milestone 8 Publication Checkpoint B."
  )
endif()
string(FIND "${_roadmap}"
  "Milestone 8 reached local Publication"
  _roadmap_status_position
)
if(_roadmap_status_position EQUAL -1)
  message(FATAL_ERROR
    "The release roadmap is not at Milestone 8 Publication Checkpoint B."
  )
endif()

file(GLOB_RECURSE _documentation_files
  LIST_DIRECTORIES FALSE
  "${ASC_CPP_SOURCE_DIR}/docs/*.md"
)
set(_historical_banner_count 0)
foreach(_documentation_file IN LISTS _documentation_files)
  file(READ "${_documentation_file}" _documentation)
  string(FIND "${_documentation}" "> [!WARNING]" _warning_position)
  if(NOT _warning_position EQUAL -1)
    math(EXPR _historical_banner_count "${_historical_banner_count} + 1")
    foreach(_banner_text IN ITEMS
        "**Superseded historical document.**"
        "body below records the deleted"
        "${ASC_CPP_EXPECTED_PREDECESSOR_COMMIT}"
        "does not describe the active API or package"
        "[current documentation]"
        "[approved Stage A architecture]")
      string(FIND "${_documentation}" "${_banner_text}" _banner_position)
      if(_banner_position EQUAL -1)
        message(FATAL_ERROR
          "Historical banner in ${_documentation_file} is missing: "
          "${_banner_text}"
        )
      endif()
    endforeach()
  endif()
endforeach()

if(NOT _historical_banner_count EQUAL 20)
  message(FATAL_ERROR
    "Expected exactly 20 neutral historical banners; found "
    "${_historical_banner_count}."
  )
endif()

message(STATUS
  "Milestone 8 security policy and 20 historical banners are consistent."
)
