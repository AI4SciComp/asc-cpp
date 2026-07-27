add_library(asc_expression INTERFACE)
add_library(ASC::expression ALIAS asc_expression)

target_sources(
  asc_expression
  INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
      "${PROJECT_SOURCE_DIR}/include/asc/expression.h"
      "${PROJECT_SOURCE_DIR}/include/asc/expression/expression.h"
      "${PROJECT_SOURCE_DIR}/include/asc/expression/writable.h"
)

set_target_properties(
  asc_expression
  PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    EXPORT_NAME expression
)

target_link_libraries(asc_expression INTERFACE ASC::core)
# Released ASCCMake v0.1.0 deliberately rejects interface libraries in
# asc_target_enable_cxx20. Use the exact underlying standard CMake requirement
# instead of inventing an asc-cmake API.
target_compile_features(asc_expression INTERFACE cxx_std_20)
