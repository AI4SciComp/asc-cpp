add_library(asc_utilities)
add_library(ASC::utilities ALIAS asc_utilities)

target_sources(
  asc_utilities
  PRIVATE
    command_line.cc
    timer.cc
  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/utilities.h"
      "${PROJECT_SOURCE_DIR}/include/asc/utilities/command_line.h"
      "${PROJECT_SOURCE_DIR}/include/asc/utilities/export.h"
      "${PROJECT_SOURCE_DIR}/include/asc/utilities/timer.h"
)

set_target_properties(
  asc_utilities
  PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    EXPORT_NAME utilities
    OUTPUT_NAME asc_utilities
    VISIBILITY_INLINES_HIDDEN YES
)
if(APPLE)
  set_property(TARGET asc_utilities PROPERTY INSTALL_RPATH "@loader_path")
elseif(UNIX)
  set_property(TARGET asc_utilities PROPERTY INSTALL_RPATH "$ORIGIN")
endif()

target_compile_definitions(
  asc_utilities
  PRIVATE ASC_UTILITIES_BUILDING_LIBRARY
)
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(asc_utilities PUBLIC ASC_UTILITIES_STATIC_DEFINE)
endif()

target_link_libraries(asc_utilities PUBLIC ASC::core)
asc_target_enable_cxx20(TARGET asc_utilities PUBLIC)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_utilities AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_utilities)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_utilities
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
