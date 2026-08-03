# ASCCpp 0.9.0 {#mainpage}

ASCCpp is a portable C++20 scientific-computing foundation with explicit
ownership, memory placement, execution, failure, numerical, and package
contracts.

The supported 0.9.0 profile is provider-free CPU Core, Utilities, Expression,
Dense, Sparse, Random, and Random Dense/Sparse storage facets. CPU BLAS is a
correctness reference, not an optimized provider. CUDA provider components are
experimental unless the exact release commit passes the complete hosted
real-NVIDIA gate.

## Modules

- @ref asc_core "Core": status/result, contracts, configuration, I/O, types,
  memory resources, buffers, execution, and copies.
- @ref asc_utilities "Utilities": command-line parsing and monotonic timing.
- @ref asc_expression "Expression": storage-neutral reading, writing,
  placement, aliasing, and evaluation customization.
- @ref asc_dense "Dense" and @ref asc_dense_blas "Dense BLAS": owning arrays,
  layouts/views, evaluation, and BLAS Levels 1--3.
- @ref asc_sparse "Sparse" and @ref asc_sparse_blas "Sparse BLAS": coordinate,
  CSR/CSC, conversion, evaluation, and sparse vector/matrix operations.
- @ref asc_random "Random": deterministic engines, distributions, QMC, and
  Dense/Sparse storage adapters.
- @ref asc_cuda "CUDA providers": explicit experimental device contexts,
  memory, streams/events, completion, and provider operations.

Read the @ref api_conventions "API conventions", @ref md_docs_2installation
"installation guide", @ref md_docs_2support-matrix "support matrix", and
@ref md_docs_2package-capabilities "package capabilities" before relying on a
provider or compatibility claim.

## Compiled installed-package examples

The following are the exact sources compiled by the package/example gate:

@include core.cc

@include dense_blas.cc

@include sparse_blas.cc

@include random.cc

@include random_storage.cc

The experimental CUDA evidence job also compiles:

@include cuda.cc
