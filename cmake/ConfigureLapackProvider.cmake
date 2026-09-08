# Invoked only by ASC_CPP_ENABLE_LAPACK. This is an explicit development subset,
# never a source of full-profile coverage claims.
include_guard(GLOBAL)
cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH _asc_cpp_lapack_source_dir)

if(NOT ASC_CPP_LAPACK_INTEGER_BITS MATCHES "^(32|64)$")
  message(FATAL_ERROR "ASC_CPP_LAPACK_INTEGER_BITS must be exactly 32 or 64.")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
   OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$"
   OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   OR NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "11.4.0")
  message(FATAL_ERROR
    "The incremental reference facet currently requires its audited Linux x86_64 GCC 11.4.0 ABI. "
    "Other compiler/platform gates remain required, not verified."
  )
endif()
if(BUILD_SHARED_LIBS)
  message(FATAL_ERROR
    "Shared reference-facet symbol/runtime isolation is not yet verified; use the explicit static subset."
  )
endif()
if(NOT EXISTS "${ASC_CPP_LAPACK_ATTESTATION}")
  message(FATAL_ERROR "ASC_CPP_LAPACK_ATTESTATION must name an exact external build record.")
endif()
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -B
    "${_asc_cpp_lapack_source_dir}/tools/lapack/attest_provider.py"
    --attestation "${ASC_CPP_LAPACK_ATTESTATION}"
    --prefix "${ASC_CPP_LAPACK_ROOT}"
    --inventory "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-upstream-inventory.json"
    --provider-lock "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-provider-lock.json"
    --integer-bits "${ASC_CPP_LAPACK_INTEGER_BITS}"
  RESULT_VARIABLE _attestation_result
  OUTPUT_VARIABLE _attestation_output ERROR_VARIABLE _attestation_error
)
if(NOT _attestation_result EQUAL 0)
  message(FATAL_ERROR "Invalid reference provider: ${_attestation_error}")
endif()
file(READ "${ASC_CPP_LAPACK_ATTESTATION}" _attestation)
string(JSON ASC_CPP_LAPACK_BUILD_ID GET "${_attestation}" identity_sha256)
file(READ "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-provider-lock.json" _lock)
string(JSON _source_digest GET "${_lock}" verification source_input_manifest_sha256)
set(ASC_CPP_LAPACK_CONFIG_DIR "${PROJECT_BINARY_DIR}/dense-lapack-config")
file(MAKE_DIRECTORY "${ASC_CPP_LAPACK_CONFIG_DIR}")
file(WRITE "${ASC_CPP_LAPACK_CONFIG_DIR}/lapack_build_config.h"
  "#ifndef ASC_INTERNAL_LAPACK_BUILD_CONFIG_H_\n#define ASC_INTERNAL_LAPACK_BUILD_CONFIG_H_\n"
  "#define ASC_LAPACK_INTEGER_BITS ${ASC_CPP_LAPACK_INTEGER_BITS}\n"
  "#define ASC_LAPACK_SOURCE_SHA256 \"${_source_digest}\"\n"
  "#define ASC_LAPACK_BUILD_SHA256 \"${ASC_CPP_LAPACK_BUILD_ID}\"\n#endif\n"
)

# The relocation contract contains no original source/build/install paths.
set(ASC_CPP_LAPACK_PROFILE "incremental-lapack-v18")
file(SHA256 "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-upstream-inventory.json" _inventory_digest)
set(_metadata "{\"schema_version\":1,\"identity_sha256\":\"${ASC_CPP_LAPACK_BUILD_ID}\",\"source_input_sha256\":\"${_source_digest}\",\"inventory_sha256\":\"${_inventory_digest}\",\"profile\":\"${ASC_CPP_LAPACK_PROFILE}\",\"integer_bits\":${ASC_CPP_LAPACK_INTEGER_BITS},\"libraries\":[],\"runtimes\":[]}")
set(_suffix)
if(ASC_CPP_LAPACK_INTEGER_BITS STREQUAL "64")
  set(_suffix "64")
endif()
string(JSON _file_count LENGTH "${_attestation}" payload installed_files)
math(EXPR _last_file "${_file_count} - 1")
set(_library_index 0)
foreach(_stem IN ITEMS lapacke lapack blas)
  set(_found FALSE)
  foreach(_index RANGE 0 ${_last_file})
    string(JSON _path GET "${_attestation}" payload installed_files ${_index} path)
    if(_path MATCHES "(^|/)lib${_stem}${_suffix}\\.a$")
      string(JSON _record GET "${_attestation}" payload installed_files ${_index})
      string(JSON _metadata SET "${_metadata}" libraries ${_library_index} "${_record}")
      set(_found TRUE)
    endif()
  endforeach()
  if(NOT _found)
    message(FATAL_ERROR "The static reference subset needs attested lib${_stem}${_suffix}.a.")
  endif()
  math(EXPR _library_index "${_library_index} + 1")
endforeach()
set(_runtime_index 0)
set(_runtime_kinds)
foreach(_runtime IN LISTS ASC_CPP_LAPACK_RUNTIME_LIBRARIES)
  if(NOT IS_ABSOLUTE "${_runtime}" OR NOT EXISTS "${_runtime}")
    message(FATAL_ERROR "Reference runtimes must be explicit existing absolute paths.")
  endif()
  file(REAL_PATH "${_runtime}" _runtime_real)
  cmake_path(GET _runtime_real FILENAME _runtime_name)
  if(NOT _runtime_name MATCHES "^lib(gfortran|quadmath)\\.so\\.[0-9.]+$")
    message(FATAL_ERROR "Unaudited reference runtime: ${_runtime_name}")
  endif()
  list(APPEND _runtime_kinds "${CMAKE_MATCH_1}")
  file(SHA256 "${_runtime_real}" _runtime_hash)
  string(JSON _metadata SET "${_metadata}" runtimes ${_runtime_index}
    "{\"name\":\"${_runtime_name}\",\"sha256\":\"${_runtime_hash}\"}"
  )
  math(EXPR _runtime_index "${_runtime_index} + 1")
endforeach()
list(SORT _runtime_kinds)
if(NOT _runtime_index EQUAL 2 OR NOT _runtime_kinds STREQUAL "gfortran;quadmath")
  message(FATAL_ERROR "The audited GNU route requires explicit gfortran and quadmath runtimes.")
endif()
set(ASC_CPP_LAPACK_METADATA "${ASC_CPP_LAPACK_CONFIG_DIR}/ASCCppLapackProvider.json")
file(WRITE "${ASC_CPP_LAPACK_METADATA}" "${_metadata}\n")
file(SHA256 "${ASC_CPP_LAPACK_METADATA}" ASC_CPP_LAPACK_METADATA_SHA256)
include("${CMAKE_CURRENT_LIST_DIR}/ASCCppLapackDependency.cmake")
asc_internal_lapack_dependencies(
  "${ASC_CPP_LAPACK_METADATA}" "${ASC_CPP_LAPACK_ROOT}"
  "${ASC_CPP_LAPACK_RUNTIME_LIBRARIES}" _dependency_error
  "${ASC_CPP_LAPACK_BUILD_ID}"
  "${ASC_CPP_LAPACK_METADATA_SHA256}"
)
if(_dependency_error)
  message(FATAL_ERROR "${_dependency_error}")
endif()

# Compile and execute the private language/library probe. C/Fortran are build
# checks only; neither language is enabled by an installed package consumer.
enable_language(C Fortran)
if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU"
   OR NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL "11.4.0")
  message(FATAL_ERROR "The reference ABI probe requires the recorded GNU C 11.4.0 toolchain.")
endif()
if(NOT CMAKE_Fortran_COMPILER_ID STREQUAL "GNU"
   OR NOT CMAKE_Fortran_COMPILER_VERSION VERSION_EQUAL "11.4.0")
  message(FATAL_ERROR "The reference ABI probe requires the recorded GNU Fortran 11.4.0 toolchain.")
endif()
string(JSON _fortran_flags GET "${_attestation}" payload options CMAKE_Fortran_FLAGS)
if(ASC_CPP_LAPACK_INTEGER_BITS STREQUAL "64")
  string(APPEND _fortran_flags " -fdefault-integer-8")
endif()
# try_compile propagates the caller's language flags after CMAKE_FLAGS. Bind
# this probe's true integer ABI in the caller scope, then restore it below.
set(_saved_fortran_flags "${CMAKE_Fortran_FLAGS}")
set(CMAKE_Fortran_FLAGS "${_fortran_flags}")
try_run(_probe_exit _probe_compiled
  "${PROJECT_BINARY_DIR}/dense-lapack-abi-probe"
  SOURCES
    "${_asc_cpp_lapack_source_dir}/tests/dense_lapack/abi_probe.cc"
    "${_asc_cpp_lapack_source_dir}/tests/dense_lapack/abi_probe.c"
    "${_asc_cpp_lapack_source_dir}/tests/dense_lapack/abi_probe.f90"
  CMAKE_FLAGS
    "-DCMAKE_CXX_STANDARD:STRING=20"
    "-DCMAKE_CXX_STANDARD_REQUIRED:BOOL=ON"
    "-DCMAKE_C_STANDARD:STRING=11"
    "-DCMAKE_Fortran_FLAGS:STRING=${_fortran_flags}"
    "-DINCLUDE_DIRECTORIES:STRING=${ASC_CPP_LAPACK_CONFIG_DIR};${ASC_CPP_LAPACK_ROOT}/include"
  LINK_LIBRARIES ASC_INTERNAL_LAPACK::reference
  COMPILE_OUTPUT_VARIABLE _probe_compile_output
  RUN_OUTPUT_VARIABLE _probe_run_output
)
set(CMAKE_Fortran_FLAGS "${_saved_fortran_flags}")
file(WRITE "${ASC_CPP_LAPACK_CONFIG_DIR}/abi-probe.log"
  "compiled=${_probe_compiled}\nexit=${_probe_exit}\n"
  "${_probe_compile_output}\n${_probe_run_output}\n"
)
if(NOT _probe_compiled OR NOT "${_probe_exit}" STREQUAL "0")
  message(FATAL_ERROR "Reference language/library ABI probe failed; see dense-lapack-config/abi-probe.log.")
endif()
if(ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE)
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" -B "${_asc_cpp_lapack_source_dir}/tools/lapack/validate_coverage.py"
      --inventory "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-upstream-inventory.json"
      --mapping "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-coverage.yaml"
      --provider-lock "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-provider-lock.json"
      --evidence "${_asc_cpp_lapack_source_dir}/docs/contracts/lapack-evidence.json"
      --source-root "${_asc_cpp_lapack_source_dir}" --evidence-root "${ASC_CPP_LAPACK_EVIDENCE_ROOT}"
      --require-full
    RESULT_VARIABLE _coverage_result OUTPUT_VARIABLE _coverage_output ERROR_VARIABLE _coverage_error
  )
  if(NOT _coverage_result EQUAL 0)
    message(FATAL_ERROR "Full reference profile is incomplete: ${_coverage_output}${_coverage_error}")
  endif()
endif()
message(STATUS "Explicit reference provider ${ASC_CPP_LAPACK_BUILD_ID}; incremental capability, not full-profile verification.")
