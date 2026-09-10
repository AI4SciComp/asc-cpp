cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR PRODUCER_BINARY_DIR WORK_DIR
    ASC_CPP_TEST_WORKSPACE_ROOT ASC_CPP_TEST_WORKSPACE_GUARD LAPACK_ROOT
    LAPACK_RUNTIMES CXX_COMPILER)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()
include("${SOURCE_DIR}/tests/cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}")

function(_run name expect_success)
  execute_process(COMMAND ${ARGN} RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
  file(WRITE "${WORK_DIR}/${name}.log" "${_stdout}\n${_stderr}")
  if(expect_success AND NOT _result EQUAL 0)
    message(FATAL_ERROR "${name} failed (${_result}); see ${WORK_DIR}/${name}.log")
  elseif(NOT expect_success AND _result EQUAL 0)
    message(FATAL_ERROR "${name} incorrectly accepted an unavailable provider")
  endif()
endfunction()

set(_prefix "${WORK_DIR}/original ASC prefix with spaces")
_run(install TRUE "${CMAKE_COMMAND}" --install "${PRODUCER_BINARY_DIR}"
  --prefix "${_prefix}" --config "${CONFIG}")
set(_relocated "${WORK_DIR}/relocated ASC prefix with spaces")
file(RENAME "${_prefix}" "${_relocated}")
file(GLOB_RECURSE _configs "${_relocated}/*/ASCCppConfig.cmake")
list(LENGTH _configs _config_count)
if(NOT _config_count EQUAL 1)
  message(FATAL_ERROR "Expected exactly one installed ASCCpp package")
endif()
list(GET _configs 0 _config)
cmake_path(GET _config PARENT_PATH _package)
file(READ "${_package}/ASCCppLapackProvider.json" _metadata)
file(GLOB _package_files "${_package}/*.cmake" "${_package}/*.json")
foreach(_file IN LISTS _package_files)
  file(READ "${_file}" _contents)
  foreach(_forbidden IN ITEMS "${SOURCE_DIR}" "${PRODUCER_BINARY_DIR}"
      "${LAPACK_ROOT}" "${_prefix}")
    string(FIND "${_contents}" "${_forbidden}" _position)
    if(NOT _position EQUAL -1)
      message(FATAL_ERROR "Installed metadata embeds a nonrelocatable path: ${_file}")
    endif()
  endforeach()
endforeach()

# The C++-only example performs factor-once/two-RHS solve, INFO reporting,
# printing, ASC file round-trip, residual verification and staged-read rollback.
file(COPY "${SOURCE_DIR}/examples/lapack_array_io/"
  DESTINATION "${WORK_DIR}/copied example")
file(WRITE "${WORK_DIR}/copied example/check_languages.cmake" [=[
get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
if(NOT _languages STREQUAL "CXX")
  message(FATAL_ERROR "Installed provider enabled foreign build languages: ${_languages}")
endif()
if(TARGET CUDA::cudart OR TARGET CUDA::cublas OR TARGET ASC::core_cuda)
  message(FATAL_ERROR "Reference-only lookup discovered CUDA")
endif()
]=])
file(READ "${WORK_DIR}/copied example/CMakeLists.txt" _example_project)
file(WRITE "${WORK_DIR}/copied example/CMakeLists.txt"
  "string(REPLACE \"|\" \";\" ASC_CPP_LAPACK_RUNTIME_LIBRARIES \"\${TEST_RUNTIMES}\")\n"
  "${_example_project}\n"
  "include(\"\${CMAKE_CURRENT_SOURCE_DIR}/check_languages.cmake\")\n")
set(_consumer_command "${CMAKE_COMMAND}"
  -S "${WORK_DIR}/copied example" -B "${WORK_DIR}/example build"
  "-DASCCpp_DIR=${_package}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
  "-DASC_CPP_LAPACK_ROOT=${LAPACK_ROOT}"
  "-DTEST_RUNTIMES=${LAPACK_RUNTIMES}"
  "-DASC_CPP_EXAMPLES_ENABLE_LAPACK=ON"
  -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE)
_run(example-configure TRUE ${_consumer_command})
_run(example-build TRUE "${CMAKE_COMMAND}" --build "${WORK_DIR}/example build"
  --config "${CONFIG}")
_run(example-test TRUE "${CMAKE_CTEST_COMMAND}" --test-dir
  "${WORK_DIR}/example build" -C "${CONFIG}" --output-on-failure --no-tests=error)

file(COPY "${SOURCE_DIR}/examples/positive_tridiagonal/"
  DESTINATION "${WORK_DIR}/copied PT example")
file(READ "${WORK_DIR}/copied PT example/CMakeLists.txt" _pt_project)
file(WRITE "${WORK_DIR}/copied PT example/CMakeLists.txt"
  "string(REPLACE \"|\" \";\" ASC_CPP_LAPACK_RUNTIME_LIBRARIES \"\${TEST_RUNTIMES}\")\n"
  "${_pt_project}\n")
_run(pt-example-configure TRUE "${CMAKE_COMMAND}"
  -S "${WORK_DIR}/copied PT example" -B "${WORK_DIR}/PT example build"
  "-DASCCpp_DIR=${_package}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
  "-DASC_CPP_LAPACK_ROOT=${LAPACK_ROOT}"
  "-DTEST_RUNTIMES=${LAPACK_RUNTIMES}"
  -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE)
_run(pt-example-build TRUE "${CMAKE_COMMAND}" --build
  "${WORK_DIR}/PT example build" --config "${CONFIG}")
_run(pt-example-test TRUE "${CMAKE_CTEST_COMMAND}" --test-dir
  "${WORK_DIR}/PT example build" -C "${CONFIG}"
  --output-on-failure --no-tests=error)

file(COPY "${SOURCE_DIR}/examples/mixed_general/"
  DESTINATION "${WORK_DIR}/copied mixed example")
file(READ "${WORK_DIR}/copied mixed example/CMakeLists.txt" _pt_project)
file(WRITE "${WORK_DIR}/copied mixed example/CMakeLists.txt"
  "string(REPLACE \"|\" \";\" ASC_CPP_LAPACK_RUNTIME_LIBRARIES \"\${TEST_RUNTIMES}\")\n"
  "${_pt_project}\n")
_run(mixed-example-configure TRUE "${CMAKE_COMMAND}"
  -S "${WORK_DIR}/copied mixed example" -B "${WORK_DIR}/mixed example build"
  "-DASCCpp_DIR=${_package}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
  "-DASC_CPP_LAPACK_ROOT=${LAPACK_ROOT}"
  "-DTEST_RUNTIMES=${LAPACK_RUNTIMES}"
  -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE)
_run(mixed-example-build TRUE "${CMAKE_COMMAND}" --build
  "${WORK_DIR}/mixed example build" --config "${CONFIG}")
_run(mixed-example-test TRUE "${CMAKE_CTEST_COMMAND}" --test-dir
  "${WORK_DIR}/mixed example build" -C "${CONFIG}"
  --output-on-failure --no-tests=error)

if(ROBUST_ENABLED)
  file(COPY "${SOURCE_DIR}/examples/robust_ppsvx/"
    DESTINATION "${WORK_DIR}/copied robust example")
  file(READ "${WORK_DIR}/copied robust example/CMakeLists.txt" _robust_project)
  file(WRITE "${WORK_DIR}/copied robust example/CMakeLists.txt"
    "string(REPLACE \"|\" \";\" ASC_CPP_LAPACK_RUNTIME_LIBRARIES \"\${TEST_RUNTIMES}\")\n"
    "${_robust_project}\n")
  _run(robust-example-configure TRUE "${CMAKE_COMMAND}"
    -S "${WORK_DIR}/copied robust example" -B "${WORK_DIR}/robust example build"
    "-DASCCpp_DIR=${_package}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DASC_CPP_LAPACK_ROOT=${LAPACK_ROOT}" "-DTEST_RUNTIMES=${LAPACK_RUNTIMES}"
    -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE)
  _run(robust-example-build TRUE "${CMAKE_COMMAND}" --build
    "${WORK_DIR}/robust example build" --config "${CONFIG}")
  _run(robust-example-test TRUE "${CMAKE_CTEST_COMMAND}" --test-dir
    "${WORK_DIR}/robust example build" -C "${CONFIG}"
    --output-on-failure --no-tests=error)
endif()

file(COPY "${SOURCE_DIR}/tests/dense_lapack/installed_lu/"
  DESTINATION "${WORK_DIR}/installed LU families")
_run(lu-families-configure TRUE "${CMAKE_COMMAND}"
  -S "${WORK_DIR}/installed LU families" -B "${WORK_DIR}/LU families build"
  "-DASCCpp_DIR=${_package}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
  "-DASC_CPP_LAPACK_ROOT=${LAPACK_ROOT}" "-DTEST_RUNTIMES=${LAPACK_RUNTIMES}"
  -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
  -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE
  -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE)
_run(lu-families-build TRUE "${CMAKE_COMMAND}" --build
  "${WORK_DIR}/LU families build" --config "${CONFIG}")
_run(lu-families-test TRUE "${CMAKE_CTEST_COMMAND}" --test-dir
  "${WORK_DIR}/LU families build" -C "${CONFIG}"
  --output-on-failure --no-tests=error)

set(_probe [=[
cmake_minimum_required(VERSION 3.25)
project(ASCCppProviderIsolation LANGUAGES CXX)
string(REPLACE "|" ";" ASC_CPP_LAPACK_RUNTIME_LIBRARIES "${TEST_RUNTIMES}")
if(TEST_MODE STREQUAL "base")
  find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS cpp dense sparse random)
elseif(TEST_MODE STREQUAL "optional")
  find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS core OPTIONAL_COMPONENTS dense_lapack)
  if(ASCCpp_dense_lapack_FOUND)
    message(FATAL_ERROR "A damaged optional provider was marked found")
  endif()
else()
  find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_lapack)
  if(NOT TARGET ASC::dense_lapack OR NOT TARGET ASC_INTERNAL_LAPACK::reference)
    message(FATAL_ERROR "Accepted provider is missing its targets")
  endif()
  # A negative required lookup must fail in find_package, not in the test's
  # later target-isolation assertion. The valid-provider control exercises this.
  return()
endif()
if(TARGET ASC::dense_lapack OR TARGET ASC_INTERNAL_LAPACK::reference)
  message(FATAL_ERROR "Unavailable or unrequested provider leaked a target")
endif()
get_property(_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
if(NOT _languages STREQUAL "CXX")
  message(FATAL_ERROR "Provider-free lookup enabled foreign build languages")
endif()
add_executable(consumer main.cc)
target_link_libraries(consumer PRIVATE ASC::core)
]=])
file(MAKE_DIRECTORY "${WORK_DIR}/probe")
file(WRITE "${WORK_DIR}/probe/CMakeLists.txt" "${_probe}")
file(WRITE "${WORK_DIR}/probe/main.cc"
  "#include <asc/core.h>\nint main() { return 0; }\n")
function(_case name mode root runtimes expect_success)
  _run("${name}-configure" "${expect_success}" "${CMAKE_COMMAND}"
    -S "${WORK_DIR}/probe" -B "${WORK_DIR}/${name}"
    "-DASCCpp_DIR=${_package}" "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DTEST_MODE=${mode}" "-DASC_CPP_LAPACK_ROOT=${root}"
    "-DTEST_RUNTIMES=${runtimes}"
    -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
    -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE
    -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE)
  if(ARGN)
    file(READ "${WORK_DIR}/${name}-configure.log" _case_output)
    if(NOT _case_output MATCHES "${ARGN}")
      message(FATAL_ERROR "${name} did not reach the intended failure: ${ARGN}")
    endif()
  endif()
  if(expect_success)
    _run("${name}-build" TRUE "${CMAKE_COMMAND}" --build "${WORK_DIR}/${name}")
  endif()
endfunction()

_case(base-lazy base "${WORK_DIR}/does not exist" "" TRUE)
_case(valid-required-control required "${LAPACK_ROOT}" "${LAPACK_RUNTIMES}" TRUE)
_case(missing-required required "${WORK_DIR}/does not exist" "" FALSE)
_case(missing-optional optional "${WORK_DIR}/does not exist" "" TRUE)
_case(runtime-required required "${LAPACK_ROOT}" "" FALSE)
_case(runtime-optional optional "${LAPACK_ROOT}" "" TRUE)
file(MAKE_DIRECTORY "${WORK_DIR}/partial prefix")
_case(partial-required required "${WORK_DIR}/partial prefix" "${LAPACK_RUNTIMES}" FALSE)

# Mutate only the dedicated installed test copy, never the provider prefix or
# source/build metadata. Each case starts from the original immutable record.
foreach(_mutation IN ITEMS malformed wrong-abi wrong-hash wrong-identity
    escaping-path duplicate-runtime)
  set(_damaged "${_metadata}")
  if(_mutation STREQUAL "malformed")
    set(_damaged "{invalid")
  elseif(_mutation STREQUAL "wrong-abi")
    string(JSON _bits GET "${_metadata}" integer_bits)
    math(EXPR _other_bits "96 - ${_bits}")
    string(JSON _damaged SET "${_metadata}" integer_bits "${_other_bits}")
  elseif(_mutation STREQUAL "wrong-hash")
    string(JSON _damaged SET "${_metadata}" libraries 0 sha256
      "\"0000000000000000000000000000000000000000000000000000000000000000\"")
  elseif(_mutation STREQUAL "wrong-identity")
    string(JSON _damaged SET "${_metadata}" identity_sha256
      "\"0000000000000000000000000000000000000000000000000000000000000000\"")
  elseif(_mutation STREQUAL "escaping-path")
    string(JSON _damaged SET "${_metadata}" libraries 0 path "\"../liblapacke.a\"")
  else()
    string(JSON _runtime GET "${_metadata}" runtimes 0)
    string(JSON _damaged SET "${_metadata}" runtimes 1 "${_runtime}")
  endif()
  file(WRITE "${_package}/ASCCppLapackProvider.json" "${_damaged}")
  _case("${_mutation}-required" required "${LAPACK_ROOT}" "${LAPACK_RUNTIMES}" FALSE)
  _case("${_mutation}-optional" optional "${LAPACK_ROOT}" "${LAPACK_RUNTIMES}" TRUE)
  _case("${_mutation}-base" base "${WORK_DIR}/does not exist" "" TRUE)
endforeach()
file(WRITE "${_package}/ASCCppLapackProvider.json" "${_metadata}")

# Simulate a mixed record whose changed archive hash is internally consistent
# but still carries the original identity label. Only the dedicated copy is
# modified. The trusted generated config must reject it before JSON parsing.
set(_alternate "${WORK_DIR}/alternate provider")
foreach(_index RANGE 0 2)
  string(JSON _relative GET "${_metadata}" libraries ${_index} path)
  set(_destination "${_alternate}/${_relative}")
  cmake_path(GET _destination PARENT_PATH _directory)
  file(MAKE_DIRECTORY "${_directory}")
  file(COPY_FILE "${LAPACK_ROOT}/${_relative}" "${_destination}")
endforeach()
string(JSON _relative GET "${_metadata}" libraries 0 path)
file(APPEND "${_alternate}/${_relative}" "ASC mixed-record negative fixture\n")
file(SHA256 "${_alternate}/${_relative}" _alternate_hash)
string(JSON _damaged SET "${_metadata}" libraries 0 sha256 "\"${_alternate_hash}\"")
file(WRITE "${_package}/ASCCppLapackProvider.json" "${_damaged}")
_case(mixed-record-required required "${_alternate}" "${LAPACK_RUNTIMES}" FALSE
  "Reference provider metadata digest mismatch")
_case(mixed-record-optional optional "${_alternate}" "${LAPACK_RUNTIMES}" TRUE)
_case(mixed-record-base base "${WORK_DIR}/does not exist" "" TRUE)
file(WRITE "${_package}/ASCCppLapackProvider.json" "${_metadata}")
message(STATUS "Installed reference example and 28 provider isolation/control cases passed")
