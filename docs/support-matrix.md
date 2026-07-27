# ASCCpp support and evidence matrix

Status: unreleased `0.9.0` Milestone 8 candidate

Date: 2026-07-27

This matrix separates the candidate's required interfaces from evidence
collected on particular environments. It is not a support-window promise and
does not turn a locally tested configuration into a claim about another
compiler, standard library, operating system, CUDA toolkit, driver, or GPU.
The Milestone 8 Publication Checkpoint B is authoritative for the exact final
commands, results, and skips.

## Language, build, and package floor

| Item | Candidate contract | Evidence boundary |
| --- | --- | --- |
| C++ language | C++20, extensions disabled on ASCCpp targets | Every supported public header is parsed as C++20; no C++17 mode is supported |
| CMake producer and checked-in consumers | CMake 3.25 or newer | The final checkpoint names each CMake version actually run; a run on 4.1 does not prove every intervening version |
| asc-cmake | released ASCCMake `0.1.0` exactly | ASCCpp consumes the actual package APIs; ASCCpp does not install or imitate asc-cmake |
| linkage | static and shared through `BUILD_SHARED_LIBS` | Both modes require their own build, package, relocation, and consumer evidence |
| build modes | Debug and Release | A mode is covered only when its configure, build, and selected tests completed |
| install layout | standard relocatable CMake config package | Build-tree, copied build-tree, installed, copied/relocated prefix, and path-with-spaces cases are distinct checks |
| package registry | not required or used by validation | Consumers select an explicit package directory or prefix with the CMake user package registry disabled |

All product targets propagate `cxx_std_20`. The interface targets
`ASC::expression`, `ASC::random_dense`, `ASC::random_sparse`, and `ASC::cpp`
carry the same requirement without an empty compiled library.

## Local Milestone 8 environment

The approved local matrix is bounded by the tools and hardware present on the
Milestone 8 host:

| Layer | Locally available configuration |
| --- | --- |
| operating system | Linux |
| host compilers | GCC 11.4 and Clang 19 |
| CMake | 4.1.2 |
| CUDA compiler/toolkit | NVIDIA CUDA 12.9.86 / CUDA Toolkit 12.9 |
| GPU | NVIDIA GeForce RTX 3060 Laptop GPU, compute capability 8.6 |
| sanitizer tools | ASan, UBSan, supported additional host sanitizer selections, and Compute Sanitizer |

The required local combinations are GCC and Clang, Debug and Release, static
and shared, with CUDA disabled; CUDA-enabled combinations use the toolchain
pairings that CMake and the installed CUDA compiler support. A combination is
not a pass merely because its tools are installed.

The following remain outside the local evidence envelope and are reported as
`skipped`: Windows/MSVC, macOS/AppleClang, other standard libraries, other CPU
architectures, other CUDA toolkits and drivers, other GPU architectures,
multi-GPU and peer-access topologies, MIG, and non-CUDA accelerators. Hosted
CI evidence is also `skipped` until it has actually run on the candidate.

## Provider-free component support

| Component | Required platform surface | Provider discovery | Current implementation |
| --- | --- | --- | --- |
| `ASC::core` | standard C++20 plus local file and host allocation facilities | none | serial execution, host memory, configuration, byte I/O, status/result, checked metadata |
| `ASC::utilities` | `ASC::core` | none | command-line configuration parsing and steady-clock timer |
| `ASC::expression` | `ASC::core` | none | interface-only readable/placed/writable expression protocol |
| `ASC::dense` | `ASC::core`, `ASC::expression` | none | host dense storage, evaluation, reductions, and reference float/double algebra |
| `ASC::sparse` | `ASC::core`, `ASC::expression` | none | host coordinate/CSR/CSC storage, conversion, evaluation, and reference float/double CSR SpMV |
| `ASC::random` | `ASC::core` | none | Philox4x32-10 words and exact float/double Uniform01 transforms |
| `ASC::random_dense` | `ASC::random`, `ASC::dense` | none | interface-only host dense Uniform01 fill |
| `ASC::random_sparse` | `ASC::random`, `ASC::sparse` | none | interface-only host exact-count coordinate generation |
| `ASC::cpp` | all six provider-free modules and both random storage facets | none | provider-free aggregate only |

Provider-free configuration does not enable the CUDA language, discover
CUDAToolkit, import a CUDA target, or acquire a provider fallback. The CUDA
option defaults to `OFF`.

## Optional CUDA support

CUDA facets are built only with `ASC_CPP_ENABLE_CUDA=ON`. The candidate
requires a CUDA compiler with C++20 support, CUDA Toolkit 12 or newer, and the
standard CMake imported targets used by the requested provider closure.

| Component | Exact public ASC edges | Private provider edge | Capability boundary |
| --- | --- | --- | --- |
| `ASC::core_cuda` | `ASC::core` | `CUDA::cudart` | device inventory, explicit CUDA memory resources, stream-backed execution, copies, events |
| `ASC::dense_cuda` | `ASC::dense`, `ASC::core_cuda` | `CUDA::cublas` | bounded float/double pointwise evaluation and dense algebra |
| `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` | `CUDA::cusparse` | trusted CSR clone, CSR SpMV, bounded trusted sparse evaluation |
| `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | CUDA Runtime through `core_cuda` | raw Philox words |
| `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | CUDA Runtime through `core_cuda` | dense Uniform01 |
| `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | CUDA Runtime through `core_cuda` | exact-count coordinate Uniform01 |

No CUDA facet exposes a CUDA SDK type or native handle in a public signature.
There is no cuRAND, CUDA Driver, Thrust/CUB, cuSOLVER, HIP, SYCL, or implicit
CPU fallback dependency.

## GPU evidence vocabulary

GPU evidence uses exactly these labels:

| Label | Meaning |
| --- | --- |
| `configure-tested` | CMake enabled CUDA, selected the declared architecture, and found the required compiler/toolkit targets |
| `compile-tested` | the named CUDA target and its applicable consumers/contracts compiled and linked |
| `runtime-tested` | the named operation executed on a real GPU and passed its runtime checks |
| `parity-tested` | real-GPU output passed an independent CPU, numerical, structural, or bit oracle appropriate to the claim |
| `skipped` | the toolchain, hardware, topology, operation, or independent oracle was unavailable or outside the approved run |

The labels are not a progression that may be inferred. For example,
`compile-tested` does not imply `configure-tested` is the only interesting
evidence, and `runtime-tested` does not imply `parity-tested`.

Milestone 7 established local real-device evidence for the six CUDA facets on
the compute-8.6 device. Milestone 8 must revalidate the integrated candidate;
the final M8 checkpoint records the authoritative per-provider labels. The
trusted-device CSC evaluator success path remains `skipped` because no
approved producer creates trusted device CSC storage. Raw external CUDA
allocations also retain a caller-supplied allocation-bound contract because
the approved CUDA Runtime dependency has no general allocation-range query.

## Compatibility limits

The matrix does not claim:

- ABI compatibility across ASCCpp `0.x` minors, compilers, standard
  libraries, build types, sanitizer modes, linkage modes, CUDA toolkits, or
  platforms;
- identical floating reduction or provider algebra bits across different
  hardware/toolchains;
- provider support for an operation not named by that provider facet;
- compatibility with deleted `array` or `linalg` headers;
- a runtime-rank owner, general broadcasting, hidden materialization, provider
  registry, or automatic backend selection; or
- a `1.0` support or stability commitment.

See [API compatibility](api-compatibility.md), [package
capabilities](package-capabilities.md), and [performance](performance.md) for
the corresponding source/ABI/package/performance boundaries.
