cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS SOURCE_DIR WORK_DIR ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD ASCCMAKE_DIR CXX_COMPILER INTEGER_BITS
    LAPACK_ROOT LAPACK_ATTESTATION LAPACK_RUNTIMES FORTRAN_COMPILER C_COMPILER)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()
include("${SOURCE_DIR}/tests/cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}")
string(REPLACE "|" ";" _runtimes "${LAPACK_RUNTIMES}")
file(WRITE "${WORK_DIR}/provider-inputs.cmake"
  "set(ASC_CPP_LAPACK_RUNTIME_LIBRARIES [==[${_runtimes}]==] CACHE STRING \"Explicit test runtimes\")\n"
  "set(CMAKE_Fortran_FLAGS [==[${FORTRAN_FLAGS}]==] CACHE STRING \"Audited test driver flags\")\n")
math(EXPR _other_bits "96 - ${INTEGER_BITS}")

foreach(_case IN ITEMS disabled-lazy full-disabled mismatched-linkage wrong-width
    missing-attestation full-profile robust-disabled)
  set(_command "${CMAKE_COMMAND}" -S "${SOURCE_DIR}"
    -B "${WORK_DIR}/${_case}" "-DASCCMake_DIR=${ASCCMAKE_DIR}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DBUILD_SHARED_LIBS=${PRODUCER_SHARED}"
    -DASC_CPP_BUILD_TESTING=OFF -DBUILD_TESTING=OFF
    -DASC_CPP_BUILD_DOCUMENTATION=OFF -DASC_CPP_INSTALL=ON
    -DASC_CPP_FETCH_ASCCMAKE=OFF -DASC_CPP_ENABLE_CUDA=OFF
    -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=TRUE
    "-DASC_CPP_LAPACK_ROOT=${LAPACK_ROOT}"
    "-DASC_CPP_LAPACK_ATTESTATION=${LAPACK_ATTESTATION}"
    "-DASC_CPP_LAPACK_INTEGER_BITS=${INTEGER_BITS}")
  if(_case STREQUAL "disabled-lazy")
    list(APPEND _command -DASC_CPP_ENABLE_LAPACK=OFF
      "-DASC_CPP_LAPACK_ROOT=${WORK_DIR}/does not exist"
      "-DASC_CPP_LAPACK_ATTESTATION=${WORK_DIR}/does not exist.json"
      -DCMAKE_DISABLE_FIND_PACKAGE_Python3=TRUE
      -DCMAKE_DISABLE_FIND_PACKAGE_LAPACK=TRUE
      -DCMAKE_DISABLE_FIND_PACKAGE_BLAS=TRUE)
  elseif(_case STREQUAL "full-disabled")
    list(APPEND _command -DASC_CPP_ENABLE_LAPACK=OFF
      -DASC_CPP_LAPACK_REQUIRE_FULL_PROFILE=ON)
    set(_expected "full LAPACK profile requires")
  elseif(_case STREQUAL "robust-disabled")
    list(APPEND _command -DASC_CPP_ENABLE_LAPACK=OFF
      -DASC_CPP_ENABLE_EXPERIMENTAL_ROBUST_PPSVX=ON)
    set(_expected "Experimental RobustPpsvx uses the explicit Dense LAPACK context")
  else()
    list(APPEND _command -DASC_CPP_ENABLE_LAPACK=ON
      "-DCMAKE_C_COMPILER=${C_COMPILER}")
    if(_case STREQUAL "mismatched-linkage")
      if(PRODUCER_SHARED)
        list(APPEND _command -DBUILD_SHARED_LIBS=OFF)
        set(_expected "Static ASC requires its admitted static provider profile")
      else()
        list(APPEND _command -DBUILD_SHARED_LIBS=ON)
        set(_expected "Shared ASC requires a separately attested shared provider")
      endif()
    elseif(_case STREQUAL "wrong-width")
      list(APPEND _command "-DASC_CPP_LAPACK_INTEGER_BITS=${_other_bits}")
      set(_expected "Invalid reference provider")
    elseif(_case STREQUAL "missing-attestation")
      list(APPEND _command
        "-DASC_CPP_LAPACK_ATTESTATION=${WORK_DIR}/does not exist.json")
      set(_expected "must name an exact external build record")
    else()
      list(APPEND _command -DASC_CPP_LAPACK_REQUIRE_FULL_PROFILE=ON
        "-DCMAKE_Fortran_COMPILER=${FORTRAN_COMPILER}"
        "-DASC_CPP_LAPACK_EVIDENCE_ROOT=${WORK_DIR}/no full evidence"
        -C "${WORK_DIR}/provider-inputs.cmake")
      set(_expected "Full reference profile is incomplete")
    endif()
  endif()
  execute_process(COMMAND ${_command} RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
  file(WRITE "${WORK_DIR}/${_case}.log" "${_stdout}\n${_stderr}")
  if(_case STREQUAL "disabled-lazy")
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "Provider-free lazy configure failed; see ${WORK_DIR}/${_case}.log")
    endif()
    file(READ "${WORK_DIR}/${_case}/CMakeCache.txt" _cache)
    if(_cache MATCHES "CMAKE_(C|Fortran)_COMPILER:[A-Z]+="
       OR _cache MATCHES "_Python3_EXECUTABLE:[A-Z]+=.*python")
      message(FATAL_ERROR "Provider-free configure discovered a foreign compiler/interpreter")
    endif()
  elseif(_result EQUAL 0 OR NOT "${_stdout}\n${_stderr}" MATCHES "${_expected}")
    message(FATAL_ERROR "${_case} did not fail its intended gate; see ${WORK_DIR}/${_case}.log")
  endif()
endforeach()
message(STATUS "Seven configuration gates passed; full and mismatched-linkage profiles remain rejected")
