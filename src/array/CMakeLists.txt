add_library(asc_array dsmarray.cc uarray.cc)
add_library(ASC::array ALIAS asc_array)
asc_cpp_configure_compiled_target(asc_array array)
target_link_libraries(asc_array PUBLIC asc_core)
target_sources(
  asc_array
  PUBLIC FILE_SET public_headers TYPE HEADERS
  BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
  FILES
    "${PROJECT_SOURCE_DIR}/include/asc/array.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/accessor.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/carray.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/concepts.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/dsmarray.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/dsmarray_impl.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/extents.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/expr.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/forwards.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/layout.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/marray.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/mindex.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/miterator.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/mlayout.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/mobject.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/mobject_impl.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/mshape.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/spmarray.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/spmarray_impl.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/spmindex.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/spmiterator.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/spmlayout.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/tensor.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/tensor_concepts.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/tensor_view.h"
    "${PROJECT_SOURCE_DIR}/include/asc/array/uarray.h"
)
