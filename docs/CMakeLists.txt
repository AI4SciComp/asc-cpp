find_package(Doxygen 1.9.8 REQUIRED COMPONENTS dot)
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

set(_asc_cpp_doxygen_output "${PROJECT_BINARY_DIR}/docs/doxygen")
set(_asc_cpp_doxygen_warning_log
  "${_asc_cpp_doxygen_output}/doxygen-warnings.log"
)
set(_asc_cpp_doxyfile "${PROJECT_BINARY_DIR}/docs/Doxyfile")
configure_file(Doxyfile.in "${_asc_cpp_doxyfile}" @ONLY)

add_custom_target(
  asc_cpp_docs
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${_asc_cpp_doxygen_output}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${_asc_cpp_doxygen_output}"
  COMMAND Doxygen::doxygen "${_asc_cpp_doxyfile}"
  COMMAND "${CMAKE_COMMAND}" -E rm -f
          "${_asc_cpp_doxygen_output}/xml/Doxyfile.xml"
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

unset(_asc_cpp_doxyfile)
unset(_asc_cpp_doxygen_output)
unset(_asc_cpp_doxygen_warning_log)
