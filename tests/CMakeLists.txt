find_package(GTest REQUIRED)

function(asc_cpp_add_mdecpp_test_group group component)
  file(GLOB sources CONFIGURE_DEPENDS
       "${CMAKE_CURRENT_SOURCE_DIR}/${group}/unit_test_*.cc")
  if(group STREQUAL "random")
    list(FILTER sources EXCLUDE REGEX
         "/unit_test_(engine|distribution|fill)\\.cc$")
  endif()
  if(ASC_CPP_ENABLE_CUDA)
    set_source_files_properties(unit_test_main.cc ${sources} PROPERTIES LANGUAGE CUDA)
  endif()
  add_executable("asc_cpp_test_${group}" unit_test_main.cc ${sources})
  target_include_directories(
    "asc_cpp_test_${group}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  target_link_libraries(
    "asc_cpp_test_${group}"
    PRIVATE "ASC::${component}" GTest::gtest asc_cpp_project_options
  )
  asc_register_test(
    NAME "asc-cpp.${group}"
    TARGET "asc_cpp_test_${group}"
    LABELS "unit;${component};mdecpp-migration"
    TIMEOUT 180
  )
endfunction()

asc_cpp_add_mdecpp_test_group(generic cpp)
asc_cpp_add_mdecpp_test_group(algebra linalg)
asc_cpp_add_mdecpp_test_group(random cpp)
add_subdirectory(core)
add_subdirectory(utilities)
add_subdirectory(array)
add_subdirectory(linalg)
add_subdirectory(random)

function(asc_cpp_add_header_isolation header component)
  string(MAKE_C_IDENTIFIER "${header}" _asc_cpp_header_id)
  set(_asc_cpp_header_source
      "${CMAKE_CURRENT_BINARY_DIR}/header_${_asc_cpp_header_id}.cc")
  file(GENERATE OUTPUT "${_asc_cpp_header_source}"
       CONTENT "#include <${header}>\n")
  if(ASC_CPP_ENABLE_CUDA)
    set_source_files_properties(
      "${_asc_cpp_header_source}" PROPERTIES LANGUAGE CUDA
    )
  endif()
  add_library("asc_cpp_header_${_asc_cpp_header_id}" OBJECT
              "${_asc_cpp_header_source}")
  target_link_libraries(
    "asc_cpp_header_${_asc_cpp_header_id}"
    PRIVATE "ASC::${component}" asc_cpp_project_options
  )
endfunction()

set(_asc_cpp_core_headers
  asc/core.h
  asc/core/buffer.h
  asc/core/casts.h
  asc/core/config.h
  asc/core/contracts.h
  asc/core/cuda.h
  asc/core/device.h
  asc/core/error.h
  asc/core/event.h
  asc/core/execution_context.h
  asc/core/forall.h
  asc/core/globals.h
  asc/core/math.h
  asc/core/memory.h
  asc/core/memory_impl.h
  asc/core/memory_resource.h
  asc/core/memory_space.h
  # Transitional forwarding header retained by legacy Array compatibility.
  asc/core/numeric.h
  asc/core/operators.h
  asc/core/status.h
  asc/core/string.h
  asc/core/types.h
)
foreach(_asc_cpp_header IN LISTS _asc_cpp_core_headers)
  asc_cpp_add_header_isolation("${_asc_cpp_header}" core)
endforeach()

set(_asc_cpp_headers
  asc/asc.h
  asc/cpp.h
  asc/array/carray.h
  asc/array/concepts.h
  asc/array/dsmarray.h
  asc/array/dsmarray_impl.h
  asc/array/expr.h
  asc/array/forwards.h
  asc/array/marray.h
  asc/array/mindex.h
  asc/array/miterator.h
  asc/array/mlayout.h
  asc/array/mobject.h
  asc/array/mobject_impl.h
  asc/array/mshape.h
  asc/array/spmarray.h
  asc/array/spmarray_impl.h
  asc/array/spmindex.h
  asc/array/spmiterator.h
  asc/array/spmlayout.h
  asc/array/uarray.h
  asc/linalg/blas.h
  asc/linalg/decomp.h
  asc/linalg/lapack.h
  asc/random/generator.h
  asc/random/halton.h
  asc/random/hammersley.h
  asc/random/latin.h
  asc/random/normal.h
  asc/random/permutation.h
  asc/random/pseudo.h
  asc/random/sampler.h
  asc/random/sobol.h
  asc/random/spherical.h
)
if(ASC_CPP_ENABLE_EIGEN)
  list(APPEND _asc_cpp_headers asc/linalg/eigen.h)
endif()
foreach(_asc_cpp_header IN LISTS _asc_cpp_headers)
  asc_cpp_add_header_isolation("${_asc_cpp_header}" cpp)
endforeach()

set(_asc_cpp_array_headers
  asc/array.h
  asc/array/accessor.h
  asc/array/extents.h
  asc/array/layout.h
  asc/array/tensor.h
  asc/array/tensor_concepts.h
  asc/array/tensor_view.h
)
foreach(_asc_cpp_header IN LISTS _asc_cpp_array_headers)
  asc_cpp_add_header_isolation("${_asc_cpp_header}" array)
endforeach()

set(_asc_cpp_utilities_headers
  asc/utilities.h
  asc/utilities/cli.h
  asc/utilities/config.h
  asc/utilities/optparser.h
  asc/utilities/timer.h
)
foreach(_asc_cpp_header IN LISTS _asc_cpp_utilities_headers)
  asc_cpp_add_header_isolation("${_asc_cpp_header}" utilities)
endforeach()

set(_asc_cpp_linalg_headers
  asc/linalg.h
  asc/linalg/blas1.h
  asc/linalg/blas2.h
  asc/linalg/blas3.h
  asc/linalg/capabilities.h
  asc/linalg/concepts.h
  asc/linalg/detail/reference_kernels.h
  asc/linalg/types.h
)
foreach(_asc_cpp_header IN LISTS _asc_cpp_linalg_headers)
  asc_cpp_add_header_isolation("${_asc_cpp_header}" linalg)
endforeach()

set(_asc_cpp_random_headers
  asc/random.h
  asc/random/counter_engine.h
  asc/random/detail/fill_validation.h
  asc/random/distribution.h
  asc/random/fill.h
  asc/random/types.h
)
foreach(_asc_cpp_header IN LISTS _asc_cpp_random_headers)
  asc_cpp_add_header_isolation("${_asc_cpp_header}" random)
endforeach()

set(_asc_cpp_odr_a "${CMAKE_CURRENT_BINARY_DIR}/odr_a.cc")
set(_asc_cpp_odr_b "${CMAKE_CURRENT_BINARY_DIR}/odr_b.cc")
set(_asc_cpp_odr_main "${CMAKE_CURRENT_BINARY_DIR}/odr_main.cc")
file(GENERATE OUTPUT "${_asc_cpp_odr_a}" CONTENT
  "#include <asc/cpp.h>\nint OdrA() { return asc::Factorial(3); }\n")
file(GENERATE OUTPUT "${_asc_cpp_odr_b}" CONTENT
  "#include <asc/cpp.h>\nint OdrB() { return asc::Factorial(4); }\n")
file(GENERATE OUTPUT "${_asc_cpp_odr_main}" CONTENT
  "int OdrA();\nint OdrB();\nint main() { return OdrA() + OdrB() == 30 ? 0 : 1; }\n")
if(ASC_CPP_ENABLE_CUDA)
  set_source_files_properties(
    "${_asc_cpp_odr_a}" "${_asc_cpp_odr_b}" "${_asc_cpp_odr_main}"
    PROPERTIES LANGUAGE CUDA
  )
endif()
add_executable(
  asc_cpp_test_odr "${_asc_cpp_odr_a}" "${_asc_cpp_odr_b}"
  "${_asc_cpp_odr_main}"
)
target_link_libraries(asc_cpp_test_odr PRIVATE ASC::cpp asc_cpp_project_options)
asc_register_test(
  NAME asc-cpp.odr
  TARGET asc_cpp_test_odr
  LABELS "compile;odr"
  TIMEOUT 30
)

if(NOT ASC_CPP_ENABLE_SANITIZERS)
  add_test(
    NAME asc-cpp.package-relocation
    COMMAND "${CMAKE_COMMAND}"
      "-DASC_CPP_BINARY_DIR=${CMAKE_BINARY_DIR}"
      "-DASC_CPP_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/consumer"
      "-DASC_CPP_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-test"
      "-DASC_CPP_GENERATOR=${CMAKE_GENERATOR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/package_test.cmake"
  )
  set_property(TEST asc-cpp.package-relocation
               PROPERTY LABELS "integration;package")
  set_property(TEST asc-cpp.package-relocation PROPERTY TIMEOUT 300)
endif()
