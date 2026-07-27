cmake_minimum_required(VERSION 3.25)

if(NOT PROJECT_NAME STREQUAL "ASCCpp")
  message(FATAL_ERROR
    "ProjectTargetInventoryHook.cmake must be loaded by the ASCCpp "
    "top-level project."
  )
endif()
if(NOT DEFINED ASC_CPP_HARDENING_TARGET_OUTPUT
   OR ASC_CPP_HARDENING_TARGET_OUTPUT STREQUAL "")
  message(FATAL_ERROR
    "ASC_CPP_HARDENING_TARGET_OUTPUT is required when loading the "
    "target-inventory project hook."
  )
endif()

include(
  "${PROJECT_SOURCE_DIR}/tools/hardening/TargetInventory.cmake"
)

set_property(
  GLOBAL PROPERTY
  ASC_CPP_HARDENING_TARGET_BASELINE
  "${PROJECT_SOURCE_DIR}/abi/targets-and-components.txt"
)
set_property(
  GLOBAL PROPERTY
  ASC_CPP_HARDENING_HEADER_OWNERS
  "${PROJECT_SOURCE_DIR}/abi/header-owners.txt"
)
set_property(
  GLOBAL PROPERTY
  ASC_CPP_HARDENING_TARGET_OUTPUT
  "${ASC_CPP_HARDENING_TARGET_OUTPUT}"
)

function(_asc_cpp_hardening_run_deferred_target_inventory)
  get_property(
    _baseline
    GLOBAL PROPERTY ASC_CPP_HARDENING_TARGET_BASELINE
  )
  get_property(
    _header_owners
    GLOBAL PROPERTY ASC_CPP_HARDENING_HEADER_OWNERS
  )
  get_property(
    _output
    GLOBAL PROPERTY ASC_CPP_HARDENING_TARGET_OUTPUT
  )
  asc_cpp_hardening_check_target_inventory(
    BASELINE "${_baseline}"
    HEADER_OWNERS "${_header_owners}"
    OUTPUT "${_output}"
  )
endfunction()

cmake_language(
  DEFER
  DIRECTORY "${PROJECT_SOURCE_DIR}"
  CALL _asc_cpp_hardening_run_deferred_target_inventory
)
