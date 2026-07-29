# Optional backends

> [!WARNING]
> **Superseded historical document.** The body below records the deleted
> five-component implementation at commit
> `33b261ea33616a6395c4ad3b20646093103344f7`. It is retained only for audit
> history and does not describe the active API or package. See the
> [current documentation][current-docs] and the
> [approved Stage A architecture][stage-a].

[current-docs]: README.md
[stage-a]: development/asc-cpp-architecture/architecture-blueprint.md

Canonical Core/Array/Linalg/Random M1 and the MdeCpp-derived
compatibility runtime have different backend availability:

| Path | Serial | OpenMP | CUDA |
| --- | --- | --- | --- |
| Canonical `ExecutionContext` | Implemented | Not implemented in M1 | Not implemented in M1 |
| Canonical Array M1 owner/view/clone | Implemented, synchronous host | Not implemented | Not implemented |
| Canonical Linalg M1 seven-operation provider | Implemented compiled `serial-reference` | Not implemented | Not implemented |
| Canonical Random M1 fill | Implemented serial-reference | Not implemented | Not implemented |
| Legacy `Device`/`ForallWrap` | Implemented | Build option | Build option |

The backend enumerators and capability values in the canonical API do not
promise a provider. In M1, requesting an OpenMP or CUDA canonical context
returns an unavailable status. Enabling a legacy backend option does not turn
it into a canonical provider.

The broad compilation and link behavior described below is retained for
legacy array compatibility. Provider-specific translation-unit and dependency
isolation is planned for a later core milestone.

The implemented Array M1 contract accepts only host-accessible views and owners.
Its `Tensor::Clone` operation uses an explicit canonical context and is
synchronous serial work. It does not consult the legacy `Device`, follow
`UseDevice`, synchronize a legacy mirror, or become OpenMP/CUDA-capable when a
legacy backend option is enabled. Focused Array, dependency, and installed-
component tests verify this boundary.

## Canonical Linalg serial reference

Linalg M1 defines one compiled provider named `serial-reference`. A serial
context reports deterministic support for exactly `Copy`, `Scal`, `Axpy`,
`Dot`, `Nrm2`, `Gemv`, and `Gemm` on host-accessible canonical `float` and
`double` views. The provider is synchronous, performs no allocation or
transfer, and does not consult legacy OpenMP/CUDA, Eigen, or MKL switches.

`GetLinalgCapabilities(context)` is the capability source of truth. A build
option, backend enum, installed SDK, or compatibility header does not establish
a canonical numerical provider. Future BLAS/LAPACK, Eigen, MKL, and CUDA
providers require a private SPI and the same conformance suite, without SDK
types in common canonical headers.

## Canonical Random serial reference

Random M1 defines one synchronous host generation path. Philox4x32-10 has a
compiled integer implementation; `Uniform01<float/double>` and structural-view
traversal are header-visible. `FillRandom` accepts an explicit serial
`ExecutionContext` and never consults legacy OpenMP/CUDA switches, transfers
storage, or changes its sequence according to a linked SDK.

The M1 logical counter rule is the future provider conformance contract:
correctly addressed partitions must match one full fill bit for bit at every
logical coordinate. No OpenMP or CUDA Random provider, capability object,
public provider SPI, asynchronous event, or device-storage fill is implemented
in M1. A legacy backend build option does not add one.

Current optional-backend builds still apply the shared CUDA compilation policy
to compiled component sources and export OpenMP/CUDA requirements from
`ASC::core`. Since `ASC::random` consumes Core, those requirements remain
transitively visible even though canonical Random headers include no SDK and
select no such provider. Eliminating that package coupling requires the
cross-module Core/provider split.

## Legacy OpenMP path

Configure with `-DASC_CPP_ENABLE_OPENMP=ON`. The public targets then export
`OpenMP::OpenMP_CXX`, the generated configuration defines `ASC_USE_OPENMP`, and
`ForallWrap` may dispatch host work through `OmpWrap`. Loop bodies must avoid
unsynchronized shared writes.

This option affects the legacy runtime only. Canonical M1 code should create an
explicit serial `ExecutionContext` and inspect its capabilities.

## Legacy CUDA path

Configure with both the feature and the architecture required by the target
machine, for example:

```bash
cmake -S . -B build/cuda \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=80
```

The build enables CMake's CUDA language and links the runtime, cuBLAS, and
cuSPARSE. ASC library and test sources are compiled by the CUDA compiler so
extended device lambdas and the `ASC_FORALL` kernels are available.

A downstream translation unit that instantiates a device expression must also
be compiled as CUDA. Use a `.cu` extension or:

```cmake
set_source_files_properties(my_kernel.cc PROPERTIES LANGUAGE CUDA)
target_link_libraries(my_kernel PRIVATE ASC::array)
```

If a normal C++ translation unit requests the CUDA backend, ASC diagnoses that
the kernel was not device-compiled rather than silently running the wrong path.
Choose an architecture supported by both the installed toolkit and the compute
nodes; do not assume the login node's GPU is representative of the cluster.

This is compatibility behavior, not the target provider boundary. Core M1
does not provide a CUDA `MemoryResource`, CUDA `ExecutionContext`, asynchronous
CUDA `Event`, canonical copy operation, or public raw device-pointer accessor.
Ordinary use of the canonical serial core does not include CUDA headers when
the legacy CUDA build option is disabled.

The planned provider architecture will confine CUDA SDK headers and libraries
to provider implementation units. User-authored CUDA kernels will require an
explicit CUDA-compiled extension interface. Those statements describe future
work and are not M1 usage instructions.

## Eigen

Configure with `-DASC_CPP_ENABLE_EIGEN=ON`. `ASC::linalg` exports
`Eigen3::Eigen`, and `<asc/linalg/eigen.h>` enables:

- dense ASC/Eigen value conversion;
- zero-copy maps for compressed ASC sparse matrices;
- copied compressed sparse conversion in both directions;
- dense and sparse linear solver adapters.

The sparse storage order follows ASC's default layout, so the Eigen map uses the
same CSC or CSR arrays. The source ASC object must outlive a zero-copy map.

Eigen is a legacy Linalg compatibility adapter. It does not alter the canonical
Core API, the seven-operation Linalg M1 API, or the selected
`serial-reference` provider.

## MKL

`ASC_CPP_ENABLE_MKL=ON` requires `ASC_CPP_ENABLE_EIGEN=ON` and a discoverable
oneAPI MKL CMake package. It exports `MKL::MKL` through `ASC::linalg` and enables
Eigen's MKL integration. Use one compiler/runtime family consistently across
ASC, Eigen, MKL, OpenMP, and downstream code.

MKL enables the legacy Eigen integration path. It is not a canonical Linalg M1
provider and does not alter the canonical Core or seven-operation Linalg API.

## Precision and ABI

`ASC_CPP_PRECISION=single|double` selects `asc::real_t`. Backend and precision
choices are written into the installed configuration header; do not combine
objects built against incompatible asc-cpp configurations in one process.

Canonical metadata types and status/context declarations do not change with
the precision option. `real_t` is a convenience default rather than the scalar
contract for generic algorithms. Canonical Random M1 supports both
`Uniform01<float>` and `Uniform01<double>` in every precision configuration;
it does not select its output type from `real_t`.
