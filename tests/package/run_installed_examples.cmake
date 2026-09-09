cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS
    ASCCPP_PACKAGE_DIR
    CONFIG
    CXX_COMPILER
    EXAMPLES_SOURCE_DIR
    GENERATOR
    WORK_DIR
    ASC_CPP_TEST_WORKSPACE_ROOT
    ASC_CPP_TEST_WORKSPACE_GUARD
)
  if(NOT DEFINED "${_required}" OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/PrepareTestWorkspace.cmake")
asc_cpp_prepare_test_workspace(
  WORK_DIR "${WORK_DIR}"
  ROOT "${ASC_CPP_TEST_WORKSPACE_ROOT}"
  GUARD "${ASC_CPP_TEST_WORKSPACE_GUARD}"
)

function(_run description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${_result}).\nstdout:\n${_stdout}\nstderr:\n${_stderr}"
    )
  endif()
  message(STATUS "${description}\n${_stdout}${_stderr}")
endfunction()

find_program(_python NAMES python3 python REQUIRED)
set(_junit_check "${CMAKE_CURRENT_LIST_DIR}/../../tools/ci/check_ctest_junit.py")
function(_runtime description build)
  _run("${description}" "${CMAKE_CTEST_COMMAND}" --test-dir "${build}"
    -C "${CONFIG}" --no-tests=error --verbose
    --output-junit "${build}/ctest.xml")
  _run("${description} JUnit validation" "${_python}" -B "${_junit_check}"
    "${build}/ctest.xml")
endfunction()

set(_generator_arguments -G "${GENERATOR}")
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_arguments -A "${GENERATOR_PLATFORM}")
endif()
if(DEFINED GENERATOR_TOOLSET AND NOT GENERATOR_TOOLSET STREQUAL "")
  list(APPEND _generator_arguments -T "${GENERATOR_TOOLSET}")
endif()
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
  list(APPEND _generator_arguments
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=${MAKE_PROGRAM}"
  )
endif()

set(_build_dir "${WORK_DIR}/build with spaces")
# Copy first-party sources away from the checkout. All ASC headers must come
# from the relocated exported targets, even for standalone example builds.
set(_copied_examples "${WORK_DIR}/copied examples")
file(COPY "${EXAMPLES_SOURCE_DIR}/" DESTINATION "${_copied_examples}")
_run(
  "installed examples configure"
  "${CMAKE_COMMAND}"
  -S "${_copied_examples}"
  -B "${_build_dir}"
  ${_generator_arguments}
  "-DASCCpp_DIR:PATH=${ASCCPP_PACKAGE_DIR}"
  "-DCMAKE_CXX_COMPILER:FILEPATH=${CXX_COMPILER}"
  "-DCMAKE_BUILD_TYPE:STRING=${CONFIG}"
  "-DBUILD_TESTING:BOOL=ON"
  "-DASC_CPP_EXAMPLES_ENABLE_CUDA:BOOL=OFF"
  "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
  "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
)
_run(
  "installed examples build"
  "${CMAKE_COMMAND}" --build "${_build_dir}" --config "${CONFIG}"
)
_runtime("installed examples runtime" "${_build_dir}")

# Configure consumers independently: aggregate examples intentionally load both
# storage components and cannot prove that a single consumer stays isolated.
foreach(_example IN ITEMS
    lapack_array_io sparse_array_io
    dense_matrix_market sparse_matrix_market dense_factorizations)
  set(_standalone_build "${WORK_DIR}/${_example} standalone")
  _run(
    "${_example} standalone configure"
    "${CMAKE_COMMAND}"
    -S "${_copied_examples}/${_example}"
    -B "${_standalone_build}"
    ${_generator_arguments}
    "-DASCCpp_DIR:PATH=${ASCCPP_PACKAGE_DIR}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE:STRING=${CONFIG}"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON"
    "-DBUILD_TESTING:BOOL=ON"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF"
  )
  _run(
    "${_example} standalone build"
    "${CMAKE_COMMAND}" --build "${_standalone_build}" --config "${CONFIG}"
  )
  _runtime("${_example} standalone runtime" "${_standalone_build}")
  # GCC/Clang Makefile dependency files prove the consumed headers; exported
  # target isolation is checked by each consumer on every generator/platform.
  file(GLOB_RECURSE _dependencies "${_standalone_build}/*.o.d")
  foreach(_dependency IN LISTS _dependencies)
    file(READ "${_dependency}" _contents)
    get_filename_component(_checkout "${EXAMPLES_SOURCE_DIR}" DIRECTORY)
    foreach(_forbidden IN ITEMS "${_checkout}/include" "${_checkout}/src")
      string(FIND "${_contents}" "${_forbidden}" _position)
      if(NOT _position EQUAL -1)
        message(FATAL_ERROR "${_example} consumed checkout headers: ${_dependency}")
      endif()
    endforeach()
  endforeach()
endforeach()

# Independent wire checks are portable and run for both static/shared installs.
set(_wire_source "${WORK_DIR}/copied binary interop")
set(_wire_tools "${WORK_DIR}/copied binary tools")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/../array_io/binary_interop/"
  DESTINATION "${_wire_source}")
file(COPY
  "${CMAKE_CURRENT_LIST_DIR}/../../tools/array_io/check_binary_interop.py"
  "${CMAKE_CURRENT_LIST_DIR}/../../tools/array_io/prepare_file_close_cases.py"
  "${CMAKE_CURRENT_LIST_DIR}/../../tools/array_io/reference_codec.py"
  DESTINATION "${_wire_tools}")
foreach(_component IN ITEMS dense sparse)
  set(_wire_build "${WORK_DIR}/${_component} binary interop build")
  _run("${_component} independent binary configure" "${CMAKE_COMMAND}"
    -S "${_wire_source}" -B "${_wire_build}" ${_generator_arguments}
    "-DASCCpp_DIR:PATH=${ASCCPP_PACKAGE_DIR}"
    "-DCMAKE_CXX_COMPILER:FILEPATH=${CXX_COMPILER}"
    "-DCMAKE_BUILD_TYPE:STRING=${CONFIG}"
    "-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=ON"
    "-DASC_BINARY_COMPONENT:STRING=${_component}"
    "-DASC_BINARY_TOOLS:PATH=${_wire_tools}"
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF")
  _run("${_component} independent binary build" "${CMAKE_COMMAND}"
    --build "${_wire_build}" --config "${CONFIG}" --parallel 2)
  _runtime("${_component} independent binary runtime" "${_wire_build}")
  file(GLOB_RECURSE _wire_dependencies "${_wire_build}/*.o.d")
  get_filename_component(_checkout "${EXAMPLES_SOURCE_DIR}" DIRECTORY)
  foreach(_dependency IN LISTS _wire_dependencies)
    file(READ "${_dependency}" _contents)
    foreach(_forbidden IN ITEMS "${_checkout}/include" "${_checkout}/src")
      string(FIND "${_contents}" "${_forbidden}" _position)
      if(NOT _position EQUAL -1)
        message(FATAL_ERROR "Independent binary consumer used checkout headers.")
      endif()
    endforeach()
  endforeach()
endforeach()

if(ASC_FILE_CLOSE_ENABLED)
  set(_close_source "${WORK_DIR}/copied cleanup tests")
  file(COPY "${CMAKE_CURRENT_LIST_DIR}/../array_io/file_close/"
    DESTINATION "${_close_source}")
  set(_close_fixtures "${WORK_DIR}/cleanup fixtures")
  _run("prepare all 60 cleanup fixtures" "${_python}" -B
    "${CMAKE_CURRENT_LIST_DIR}/../../tools/array_io/prepare_file_close_cases.py"
    --output-dir "${_close_fixtures}")
  foreach(_component IN ITEMS dense sparse)
    set(_close_build "${WORK_DIR}/${_component} cleanup build")
    _run("${_component} installed cleanup configure" "${CMAKE_COMMAND}"
      -S "${_close_source}" -B "${_close_build}" ${_generator_arguments}
      "-DASCCpp_DIR:PATH=${ASCCPP_PACKAGE_DIR}"
      "-DCMAKE_CXX_COMPILER:FILEPATH=${CXX_COMPILER}"
      "-DCMAKE_BUILD_TYPE:STRING=${CONFIG}"
      "-DASC_FILE_CLOSE_COMPONENT:STRING=${_component}"
      "-DASC_FILE_CLOSE_FIXTURES:PATH=${_close_fixtures}"
      "-DCMAKE_FIND_USE_PACKAGE_REGISTRY:BOOL=OFF"
      "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY:BOOL=OFF")
    _run("${_component} installed cleanup build" "${CMAKE_COMMAND}"
      --build "${_close_build}" --config "${CONFIG}" --parallel 2)
    _runtime("${_component} installed cleanup tests" "${_close_build}")
  endforeach()
else()
  message(STATUS "File-close fault injection remains pending for this platform/linkage; ordinary installed examples execute.")
endif()
