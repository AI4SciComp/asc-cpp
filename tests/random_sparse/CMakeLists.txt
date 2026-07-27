find_package(Threads REQUIRED)

add_executable(asc_random_sparse_test random_sparse_test.cc)
target_include_directories(
  asc_random_sparse_test
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
  asc_random_sparse_test
  PRIVATE
    ASC::random_sparse
    Threads::Threads
)
asc_target_enable_cxx20(TARGET asc_random_sparse_test)
if(ASC_CPP_WARNINGS_AS_ERRORS)
  asc_target_enable_warnings(TARGET asc_random_sparse_test AS_ERRORS)
else()
  asc_target_enable_warnings(TARGET asc_random_sparse_test)
endif()
if(ASC_CPP_SANITIZER_ARGUMENTS)
  asc_target_enable_sanitizers(
    TARGET asc_random_sparse_test
    ${ASC_CPP_SANITIZER_ARGUMENTS}
  )
endif()
asc_register_test(
  NAME asc_cpp.random_sparse.runtime
  TARGET asc_random_sparse_test
  LABELS "random;random_sparse;milestone-5;runtime"
)
