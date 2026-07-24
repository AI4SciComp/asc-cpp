include_guard(GLOBAL)

if(ASC_CPP_ENABLE_OPENMP)
  find_package(OpenMP REQUIRED COMPONENTS CXX)
endif()

if(ASC_CPP_ENABLE_CUDA)
  find_package(CUDAToolkit REQUIRED)
endif()

if(ASC_CPP_ENABLE_EIGEN)
  find_package(Eigen3 3.3 REQUIRED NO_MODULE)
endif()

if(ASC_CPP_ENABLE_MKL)
  find_package(MKL CONFIG REQUIRED)
endif()
