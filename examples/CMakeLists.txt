cmake_minimum_required(VERSION 3.25)

project(ASCCppInstalledExamples LANGUAGES CXX)

include(CTest)
option(
  ASC_CPP_EXAMPLES_ENABLE_CUDA
  "Build the experimental CUDA installed-package example"
  OFF
)

set(_asc_cpp_example_components
  core dense sparse random random_dense random_sparse
)
if(ASC_CPP_EXAMPLES_ENABLE_CUDA)
  list(APPEND _asc_cpp_example_components core_cuda)
endif()
find_package(
  ASCCpp 0.9.0 EXACT CONFIG REQUIRED
  COMPONENTS ${_asc_cpp_example_components}
)

function(_asc_cpp_add_example name component runtime_component)
  add_executable("asc_cpp_example_${name}" "${name}.cc")
  target_compile_features("asc_cpp_example_${name}" PRIVATE cxx_std_20)
  set_target_properties(
    "asc_cpp_example_${name}"
    PROPERTIES CXX_EXTENSIONS OFF
  )
  target_link_libraries(
    "asc_cpp_example_${name}"
    PRIVATE "ASC::${component}"
  )
  add_test(
    NAME "asc_cpp.example.${name}"
    COMMAND "asc_cpp_example_${name}"
  )
  if(WIN32)
    set_tests_properties(
      "asc_cpp.example.${name}"
      PROPERTIES
        ENVIRONMENT_MODIFICATION
          "PATH=path_list_prepend:$<TARGET_FILE_DIR:ASC::${runtime_component}>"
    )
  endif()
endfunction()

_asc_cpp_add_example(core core core)
_asc_cpp_add_example(dense_blas dense dense)
_asc_cpp_add_example(sparse_blas sparse sparse)
_asc_cpp_add_example(random random random)
_asc_cpp_add_example(random_storage random_dense random)
target_link_libraries(
  asc_cpp_example_random_storage
  PRIVATE ASC::random_sparse
)

add_subdirectory(lapack_array_io)
add_subdirectory(sparse_array_io)
add_subdirectory(dense_matrix_market)
add_subdirectory(sparse_matrix_market)
add_subdirectory(dense_factorizations)

if(ASC_CPP_EXAMPLES_ENABLE_CUDA)
  _asc_cpp_add_example(cuda core_cuda core_cuda)
endif()
