cmake_minimum_required(VERSION 3.25)

foreach(_required IN ITEMS NM_EXECUTABLE CXXFILT_EXECUTABLE OUTPUT_FILE
                           CORE_LIBRARY UTILITIES_LIBRARY DENSE_LIBRARY
                           SPARSE_LIBRARY RANDOM_LIBRARY)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

set(_library_names core utilities dense sparse random)
if(DEFINED CORE_CUDA_LIBRARY AND NOT "${CORE_CUDA_LIBRARY}" STREQUAL "")
  list(APPEND _library_names
    core_cuda dense_cuda sparse_cuda random_cuda random_dense_cuda
    random_sparse_cuda
  )
  foreach(_cuda_library IN ITEMS
      CORE_CUDA_LIBRARY DENSE_CUDA_LIBRARY SPARSE_CUDA_LIBRARY
      RANDOM_CUDA_LIBRARY RANDOM_DENSE_CUDA_LIBRARY
      RANDOM_SPARSE_CUDA_LIBRARY)
    if(NOT DEFINED ${_cuda_library} OR "${${_cuda_library}}" STREQUAL "")
      message(FATAL_ERROR
        "All six CUDA library paths are required when CORE_CUDA_LIBRARY is set"
      )
    endif()
  endforeach()
endif()

set(_expected_core
  "asc::MemorySpaceName\\("
  "asc::CompletionEvent::CompletionEvent\\(bool\\)"
)
set(_expected_utilities "asc::Timer::Start\\(\\)")
set(_expected_dense "asc::Gemv\\(")
set(_expected_sparse "asc::internal_sparse_blas::SpmvReference\\(")
set(_expected_random "asc::GeneratePhilox4x32Word\\(")
set(_expected_core_cuda "asc::CudaMemoryResource::Create\\(")
set(_expected_dense_cuda "asc::DenseCudaContext::Create\\(")
set(_expected_sparse_cuda "asc::SparseCudaContext::Create\\(")
set(_expected_random_cuda
  "asc::CudaFillPhilox4x32\\(asc::ExecutionContext const&, asc::MutableMemoryView"
)
set(_expected_random_dense_cuda
  "asc::internal_random_dense_cuda::FillDenseUniform01Erased\\("
)
set(_expected_random_sparse_cuda
  "asc::internal_random_sparse_cuda::GenerateSparseUniform01Erased\\("
)

set(_variable_core CORE_LIBRARY)
set(_variable_utilities UTILITIES_LIBRARY)
set(_variable_dense DENSE_LIBRARY)
set(_variable_sparse SPARSE_LIBRARY)
set(_variable_random RANDOM_LIBRARY)
set(_variable_core_cuda CORE_CUDA_LIBRARY)
set(_variable_dense_cuda DENSE_CUDA_LIBRARY)
set(_variable_sparse_cuda SPARSE_CUDA_LIBRARY)
set(_variable_random_cuda RANDOM_CUDA_LIBRARY)
set(_variable_random_dense_cuda RANDOM_DENSE_CUDA_LIBRARY)
set(_variable_random_sparse_cuda RANDOM_SPARSE_CUDA_LIBRARY)

get_filename_component(_output_directory "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")
file(WRITE "${OUTPUT_FILE}"
  "kind=asc-cpp-m8-bounded-elf-shared-symbol-observation\n"
  "scope=one local ELF toolchain/configuration; not an ABI promise\n"
  "nm=${NM_EXECUTABLE}\n"
  "demangler=${CXXFILT_EXECUTABLE}\n"
)

foreach(_name IN LISTS _library_names)
  set(_variable "${_variable_${_name}}")
  set(_library "${${_variable}}")
  if(NOT EXISTS "${_library}")
    message(FATAL_ERROR "${_name} library does not exist: ${_library}")
  endif()
  execute_process(
    COMMAND "${NM_EXECUTABLE}" -D --defined-only "${_library}"
    COMMAND "${CXXFILT_EXECUTABLE}"
    RESULT_VARIABLE _nm_result
    OUTPUT_VARIABLE _symbols
    ERROR_VARIABLE _nm_error
  )
  if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR
      "Symbol inspection failed for ${_library}: ${_nm_error}"
    )
  endif()
  foreach(_pattern IN LISTS _expected_${_name})
    if(NOT _symbols MATCHES "${_pattern}")
      message(FATAL_ERROR
        "${_name} is missing representative dynamic symbol /${_pattern}/"
      )
    endif()
  endforeach()
  if(_name STREQUAL "random_cuda")
    if(_symbols MATCHES
       "CudaFillPhilox4x32\\([^\\n]*(unsigned int|void) \\*")
      message(FATAL_ERROR
        "random_cuda exposes a forbidden pointer-plus-count overload"
      )
    endif()
  endif()

  string(REPLACE "\r\n" "\n" _symbols "${_symbols}")
  string(SHA256 _symbol_sha256 "${_symbols}")
  string(REGEX MATCHALL "\n[^\\n]* asc::" _asc_symbol_prefixes
    "\n${_symbols}"
  )
  list(LENGTH _asc_symbol_prefixes _symbol_count)
  file(APPEND "${OUTPUT_FILE}"
    "library=${_name}\n"
    "path=${_library}\n"
    "asc_symbol_count=${_symbol_count}\n"
    "asc_symbol_sha256=${_symbol_sha256}\n"
    "symbols_begin\n${_symbols}symbols_end\n"
  )
endforeach()

message(STATUS
  "Verified representative shared symbols and wrote ${OUTPUT_FILE}"
)
