find_package(Doxygen 1.9.8 REQUIRED COMPONENTS dot)
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

set(_asc_cpp_doxygen_output "${PROJECT_BINARY_DIR}/docs/doxygen")
set(_asc_cpp_doxygen_warning_log
  "${_asc_cpp_doxygen_output}/doxygen-warnings.log"
)

add_custom_target(
  asc_cpp_docs
  COMMAND
    "${CMAKE_COMMAND}"
    "-DASC_CPP_DOCUMENTATION_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
    "-DASC_CPP_DOCUMENTATION_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DASC_CPP_DOCUMENTATION_VERSION=${PROJECT_VERSION}"
    "-DASC_CPP_DOCUMENTATION_DOXYGEN_EXECUTABLE=$<TARGET_FILE:Doxygen::doxygen>"
    -P
    "${PROJECT_SOURCE_DIR}/tools/documentation/BuildDoxygen.cmake"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  BYPRODUCTS
    "${_asc_cpp_doxygen_output}/html/index.html"
    "${_asc_cpp_doxygen_output}/xml/index.xml"
    "${_asc_cpp_doxygen_output}/ASCCpp.tag"
    "${_asc_cpp_doxygen_warning_log}"
  COMMENT "Generating strict ASCCpp public API documentation"
  VERBATIM
)

add_custom_target(
  asc_cpp_docs_check
  COMMAND
    "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/documentation/check_doxygen.py"
    --source "${PROJECT_SOURCE_DIR}"
    --build "${PROJECT_BINARY_DIR}"
    --xml "${_asc_cpp_doxygen_output}/xml"
    --html "${_asc_cpp_doxygen_output}/html"
    --tagfile "${_asc_cpp_doxygen_output}/ASCCpp.tag"
    --warnings "${_asc_cpp_doxygen_warning_log}"
  COMMAND
    "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/tools/documentation/check_links.py"
    --root "${PROJECT_SOURCE_DIR}"
  DEPENDS asc_cpp_docs
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Checking Doxygen coverage, links, and reproducibility"
  VERBATIM
)

unset(_asc_cpp_doxygen_output)
unset(_asc_cpp_doxygen_warning_log)
