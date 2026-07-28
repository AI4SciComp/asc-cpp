cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    ASCCPP_PACKAGE_DIR
    WORK_DIR
    EXPECT_CUDA
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)
if(NOT IS_DIRECTORY "${ASCCPP_PACKAGE_DIR}")
  message(FATAL_ERROR
    "ASCCPP_PACKAGE_DIR does not exist: ${ASCCPP_PACKAGE_DIR}"
  )
endif()
if(NOT DEFINED TEST_GENERATOR OR "${TEST_GENERATOR}" STREQUAL "")
  set(TEST_GENERATOR "Unix Makefiles")
endif()

set(_known_components
  core utilities expression dense sparse random random_dense random_sparse cpp
  core_cuda dense_cuda sparse_cuda random_cuda random_dense_cuda
  random_sparse_cuda
)
set(_available_components
  core utilities expression dense sparse random random_dense random_sparse cpp
)
if(EXPECT_CUDA)
  list(APPEND _available_components
    core_cuda dense_cuda sparse_cuda random_cuda random_dense_cuda
    random_sparse_cuda
  )
endif()

set(_closure_core core)
set(_closure_utilities core utilities)
set(_closure_expression core expression)
set(_closure_dense core dense expression)
set(_closure_sparse core expression sparse)
set(_closure_random core random)
set(_closure_random_dense core dense expression random random_dense)
set(_closure_random_sparse core expression random random_sparse sparse)
set(_closure_cpp
  core cpp dense expression random random_dense random_sparse sparse utilities
)
set(_closure_core_cuda core core_cuda)
set(_closure_dense_cuda core core_cuda dense dense_cuda expression)
set(_closure_sparse_cuda core core_cuda expression sparse sparse_cuda)
set(_closure_random_cuda core core_cuda random random_cuda)
set(_closure_random_dense_cuda
  core core_cuda dense expression random random_cuda random_dense
  random_dense_cuda
)
set(_closure_random_sparse_cuda
  core core_cuda expression random random_cuda random_sparse
  random_sparse_cuda sparse
)

function(_run_configure_case)
  set(_options EXPECT_SUCCESS DISABLE_CUDA_DISCOVERY)
  set(_one_value NAME CONTENT_VARIABLE)
  cmake_parse_arguments(CASE "${_options}" "${_one_value}" "" ${ARGN})
  if(NOT DEFINED CASE_NAME OR "${CASE_NAME}" STREQUAL "")
    message(FATAL_ERROR "_run_configure_case requires NAME")
  endif()
  if(NOT DEFINED CASE_CONTENT_VARIABLE
     OR "${CASE_CONTENT_VARIABLE}" STREQUAL "")
    message(FATAL_ERROR
      "_run_configure_case requires CONTENT_VARIABLE"
    )
  endif()
  set(_case_dir "${WORK_DIR}/${CASE_NAME}")
  asc_cpp_prepare_test_workspace(
    WORK_DIR "${_case_dir}"
    ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
    GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
  )
  set(_source "${_case_dir}/source")
  set(_build "${_case_dir}/build")
  file(MAKE_DIRECTORY "${_source}")
  file(WRITE "${_source}/CMakeLists.txt"
    "${${CASE_CONTENT_VARIABLE}}"
  )

  set(_command
    "${CMAKE_COMMAND}"
    -S "${_source}"
    -B "${_build}"
    -G "${TEST_GENERATOR}"
    "-DASCCpp_DIR=${ASCCPP_PACKAGE_DIR}"
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
  )
  if(DEFINED CXX_COMPILER AND NOT "${CXX_COMPILER}" STREQUAL "")
    list(APPEND _command "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
  endif()
  if(DEFINED CMAKE_PREFIX_PATH_ARGUMENT
     AND NOT "${CMAKE_PREFIX_PATH_ARGUMENT}" STREQUAL "")
    list(APPEND _command
      "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH_ARGUMENT}"
    )
  endif()
  if(CASE_DISABLE_CUDA_DISCOVERY)
    list(APPEND _command -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE)
  endif()

  execute_process(
    COMMAND ${_command}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(CASE_EXPECT_SUCCESS AND NOT _result EQUAL 0)
    message(FATAL_ERROR
      "Package case '${CASE_NAME}' unexpectedly failed (${_result}).\n"
      "stdout:\n${_stdout}\n"
      "stderr:\n${_stderr}"
    )
  elseif(NOT CASE_EXPECT_SUCCESS AND _result EQUAL 0)
    message(FATAL_ERROR
      "Package case '${CASE_NAME}' unexpectedly succeeded"
    )
  endif()
endfunction()

set(KNOWN_COMPONENTS "${_known_components}")
set(AVAILABLE_COMPONENTS "${_available_components}")
set(EXPECT_SHARED_VALUE "${EXPECT_SHARED}")
set(_metadata_template [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8Metadata LANGUAGES CXX)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS @AVAILABLE_COMPONENTS@)
if(NOT ASCCpp_VERSION STREQUAL "0.9.0")
  message(FATAL_ERROR "Unexpected ASCCpp_VERSION='${ASCCpp_VERSION}'")
endif()
set(_expected_known "@KNOWN_COMPONENTS@")
set(_expected_available "@AVAILABLE_COMPONENTS@")
if(NOT "${ASCCpp_KNOWN_COMPONENTS}" STREQUAL "${_expected_known}")
  message(FATAL_ERROR
    "Known component mismatch: '${ASCCpp_KNOWN_COMPONENTS}'"
  )
endif()
if(NOT "${ASCCpp_AVAILABLE_COMPONENTS}" STREQUAL "${_expected_available}")
  message(FATAL_ERROR
    "Available component mismatch: '${ASCCpp_AVAILABLE_COMPONENTS}'"
  )
endif()
foreach(_component IN LISTS _expected_available)
  if(NOT ASCCpp_${_component}_FOUND)
    message(FATAL_ERROR
      "Available requested component '${_component}' is not marked found"
    )
  endif()
endforeach()

set(_expected_links_core "")
set(_expected_links_utilities "ASC::core")
set(_expected_links_expression "ASC::core")
set(_expected_links_dense "ASC::core;ASC::expression")
set(_expected_links_sparse "ASC::core;ASC::expression")
set(_expected_links_random "ASC::core")
set(_expected_links_random_dense "ASC::random;ASC::dense")
set(_expected_links_random_sparse "ASC::random;ASC::sparse")
set(_expected_links_cpp
  "ASC::core;ASC::utilities;ASC::expression;ASC::dense;ASC::sparse;ASC::random;ASC::random_dense;ASC::random_sparse"
)
set(_expected_links_core_cuda "ASC::core")
set(_expected_links_dense_cuda "ASC::dense;ASC::core_cuda")
set(_expected_links_sparse_cuda "ASC::sparse;ASC::core_cuda")
set(_expected_links_random_cuda "ASC::random;ASC::core_cuda")
set(_expected_links_random_dense_cuda
  "ASC::random_dense;ASC::random_cuda;ASC::core_cuda"
)
set(_expected_links_random_sparse_cuda
  "ASC::random_sparse;ASC::random_cuda;ASC::core_cuda"
)
set(_static_macro_core ASC_CORE_STATIC_DEFINE)
set(_static_macro_utilities ASC_UTILITIES_STATIC_DEFINE)
set(_static_macro_dense ASC_DENSE_STATIC_DEFINE)
set(_static_macro_sparse ASC_SPARSE_STATIC_DEFINE)
set(_static_macro_random ASC_RANDOM_STATIC_DEFINE)
set(_static_macro_core_cuda ASC_CORE_CUDA_STATIC_DEFINE)
set(_static_macro_dense_cuda ASC_DENSE_CUDA_STATIC_DEFINE)
set(_static_macro_sparse_cuda ASC_SPARSE_CUDA_STATIC_DEFINE)
set(_static_macro_random_cuda ASC_RANDOM_CUDA_STATIC_DEFINE)
set(_static_macro_random_dense_cuda ASC_RANDOM_DENSE_CUDA_STATIC_DEFINE)
set(_static_macro_random_sparse_cuda ASC_RANDOM_SPARSE_CUDA_STATIC_DEFINE)
set(_interface_components expression random_dense random_sparse cpp)
foreach(_component IN LISTS _expected_available)
  if(NOT TARGET "ASC::${_component}")
    message(FATAL_ERROR "Missing target ASC::${_component}")
  endif()
  get_target_property(_features "ASC::${_component}"
    INTERFACE_COMPILE_FEATURES
  )
  if(NOT "cxx_std_20" IN_LIST _features)
    message(FATAL_ERROR "ASC::${_component} does not propagate cxx_std_20")
  endif()
  get_target_property(_type "ASC::${_component}" TYPE)
  if(_component IN_LIST _interface_components)
    if(NOT _type STREQUAL "INTERFACE_LIBRARY")
      message(FATAL_ERROR "ASC::${_component} must be an interface library")
    endif()
  elseif("@EXPECT_SHARED_VALUE@" STREQUAL "ON")
    if(NOT _type STREQUAL "SHARED_LIBRARY")
      message(FATAL_ERROR "ASC::${_component} must be a shared library")
    endif()
  elseif("@EXPECT_SHARED_VALUE@" STREQUAL "OFF")
    if(NOT _type STREQUAL "STATIC_LIBRARY")
      message(FATAL_ERROR "ASC::${_component} must be a static library")
    endif()
  endif()
  if(NOT _component IN_LIST _interface_components
     AND NOT "@EXPECT_SHARED_VALUE@" STREQUAL "")
    get_target_property(_definitions "ASC::${_component}"
      INTERFACE_COMPILE_DEFINITIONS
    )
    if(NOT _definitions)
      set(_definitions)
    endif()
    if("@EXPECT_SHARED_VALUE@" STREQUAL "OFF")
      if(NOT "${_static_macro_${_component}}" IN_LIST _definitions)
        message(FATAL_ERROR
          "ASC::${_component} does not propagate "
          "${_static_macro_${_component}} in a static package"
        )
      endif()
    elseif("${_static_macro_${_component}}" IN_LIST _definitions)
      message(FATAL_ERROR
        "ASC::${_component} propagates ${_static_macro_${_component}} "
        "from a shared package"
      )
    endif()
  endif()

  get_target_property(_links "ASC::${_component}" INTERFACE_LINK_LIBRARIES)
  if(NOT _links)
    set(_links)
  endif()
  set(_asc_links)
  foreach(_link IN LISTS _links)
    if(_link MATCHES "^ASC::")
      list(APPEND _asc_links "${_link}")
    endif()
  endforeach()
  if(NOT "${_asc_links}" STREQUAL "${_expected_links_${_component}}")
    message(FATAL_ERROR
      "ASC::${_component} direct ASC link mismatch: '${_asc_links}'"
    )
  endif()
endforeach()
]=])
string(CONFIGURE "${_metadata_template}" _metadata_content @ONLY)
_run_configure_case(
  NAME metadata
  CONTENT_VARIABLE _metadata_content
  EXPECT_SUCCESS
)

foreach(_component IN LISTS _available_components)
  set(EXPLICIT_COMPONENT "${_component}")
  set(EXPECTED_TARGETS "${_closure_${_component}}")
  set(_closure_template [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8Closure LANGUAGES CXX)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
find_package(
  ASCCpp 0.9 CONFIG REQUIRED COMPONENTS @EXPLICIT_COMPONENT@
)
get_property(_imported DIRECTORY PROPERTY IMPORTED_TARGETS)
set(_actual)
foreach(_target IN LISTS _imported)
  if(_target MATCHES "^ASC::")
    list(APPEND _actual "${_target}")
  endif()
endforeach()
list(SORT _actual)
set(_expected_components "@EXPECTED_TARGETS@")
set(_expected)
foreach(_component IN LISTS _expected_components)
  list(APPEND _expected "ASC::${_component}")
endforeach()
list(SORT _expected)
if(NOT "${_actual}" STREQUAL "${_expected}")
  message(FATAL_ERROR
    "Imported closure mismatch for @EXPLICIT_COMPONENT@. "
    "Expected '${_expected}', actual '${_actual}'"
  )
endif()
]=])
  string(CONFIGURE "${_closure_template}" _closure_content @ONLY)
  _run_configure_case(
    NAME "closure-${_component}"
    CONTENT_VARIABLE _closure_content
    EXPECT_SUCCESS
  )
endforeach()

set(_default_content [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8Default LANGUAGES CXX)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
find_package(ASCCpp 0.9 CONFIG REQUIRED)
get_property(_imported DIRECTORY PROPERTY IMPORTED_TARGETS)
set(_actual)
foreach(_target IN LISTS _imported)
  if(_target MATCHES "^ASC::")
    list(APPEND _actual "${_target}")
  endif()
endforeach()
list(SORT _actual)
set(_expected
  ASC::core ASC::cpp ASC::dense ASC::expression ASC::random
  ASC::random_dense ASC::random_sparse ASC::sparse ASC::utilities
)
list(SORT _expected)
if(NOT "${_actual}" STREQUAL "${_expected}")
  message(FATAL_ERROR
    "Default cpp closure mismatch: expected '${_expected}', got '${_actual}'"
  )
endif()
foreach(_cuda_target IN ITEMS
    CUDA::cudart CUDA::cublas CUDA::cusparse
    ASC::core_cuda ASC::dense_cuda ASC::sparse_cuda ASC::random_cuda
    ASC::random_dense_cuda ASC::random_sparse_cuda)
  if(TARGET "${_cuda_target}")
    message(FATAL_ERROR
      "Provider-free default lookup created target ${_cuda_target}"
    )
  endif()
endforeach()
]=])
_run_configure_case(
  NAME default-provider-free
  CONTENT_VARIABLE _default_content
  EXPECT_SUCCESS
  DISABLE_CUDA_DISCOVERY
)

set(_optional_content [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8Optional LANGUAGES NONE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
find_package(
  ASCCpp 0.9 CONFIG QUIET
  COMPONENTS core
  OPTIONAL_COMPONENTS imaginary
)
if(NOT ASCCpp_FOUND OR NOT ASCCpp_core_FOUND)
  message(FATAL_ERROR "Optional miss made ASCCpp/core unavailable")
endif()
if(ASCCpp_imaginary_FOUND)
  message(FATAL_ERROR "Unknown optional component was marked found")
endif()
]=])
_run_configure_case(
  NAME optional-unknown
  CONTENT_VARIABLE _optional_content
  EXPECT_SUCCESS
)

set(_unknown_required_content [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8UnknownRequired LANGUAGES NONE)
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS imaginary)
]=])
_run_configure_case(
  NAME unknown-required
  CONTENT_VARIABLE _unknown_required_content
)

if(NOT EXPECT_CUDA)
  set(_unavailable_required_content [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8UnavailableRequired LANGUAGES NONE)
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core_cuda)
]=])
  _run_configure_case(
    NAME unavailable-required
    CONTENT_VARIABLE _unavailable_required_content
    DISABLE_CUDA_DISCOVERY
  )
endif()

foreach(_version_case IN ITEMS
    "minor-compatible|0.9|REQUIRED|success"
    "exact|0.9.0|EXACT REQUIRED|success"
    "older-minor|0.8|REQUIRED|failure"
    "newer-minor|0.10|REQUIRED|failure"
    "different-major|1.0|REQUIRED|failure"
    "newer-patch|0.9.1|REQUIRED|failure")
  string(REPLACE "|" ";" _parts "${_version_case}")
  list(GET _parts 0 _name)
  list(GET _parts 1 _version)
  list(GET _parts 2 _qualifiers)
  list(GET _parts 3 _expectation)
  set(VERSION_VALUE "${_version}")
  set(VERSION_QUALIFIERS "${_qualifiers}")
  set(_version_template [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppM8Version LANGUAGES NONE)
set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
find_package(
  ASCCpp @VERSION_VALUE@ @VERSION_QUALIFIERS@ CONFIG COMPONENTS core
)
]=])
  string(CONFIGURE "${_version_template}" _version_content @ONLY)
  if(_expectation STREQUAL "success")
    _run_configure_case(
      NAME "version-${_name}"
      CONTENT_VARIABLE _version_content
      EXPECT_SUCCESS
    )
  else()
    _run_configure_case(
      NAME "version-${_name}"
      CONTENT_VARIABLE _version_content
    )
  endif()
endforeach()

message(STATUS
  "Verified ASCCpp 0.9 package metadata, versions, and component closures "
  "(CUDA enabled: ${EXPECT_CUDA})"
)
