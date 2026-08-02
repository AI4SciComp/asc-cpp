# ASCCpp support and evidence matrix

Status: unreleased `0.9.0` Issue 14 Feature Gate B candidate

Date: 2026-08-02

This matrix separates the candidate contract from evidence collected in a
particular environment. It is not a support-window promise. The Milestone 8
Publication Checkpoint B report remains authoritative for its historical
commands, counts, failures, and skips; the Issue 9 Feature Gate B report adds
only the Level 3 evidence recorded below, and the Issue 10 report adds only the
Sparse BLAS evidence recorded below. Issue 11 audits those recorded boundaries
without widening them. Issue 12 freezes the Random contract; Issue 13 adds
only the CPU engine/distribution evidence recorded below. Issue 14 adds the
storage-neutral CPU QMC and licensed direction-data evidence recorded below.

The matrix below records the clean post-checkpoint correction reruns. Earlier
revision-2 counts are historical and are not substituted for these corrected
results.

## Issue 14 quasi-random sampler evidence

Issue 14 implements the storage-neutral CPU portions of crosswalk rows
`RND-011` through `RND-014` and `RND-018` through `RND-022`: prime and
radical-inverse helpers, explicit digit permutations, Latin midpoint and
jittered sampling, Halton, Hammersley, and Sobol through dimension 21201. The
four sampler rows remain machine-classified `incomplete` because their same
rows also declare Issue 15 Dense adapters; this milestone does not claim that
unimplemented adapter scope. The complete 33-row manifest therefore records
14 equivalent, two clean-room-required, six incomplete, and 11 rejected rows.

On Linux/WSL2 6.18 with GNU C++ 11.4, CMake 4.1.2, and the installed
ASCCMake 0.1.0 package, the warnings-as-errors Debug/static complete suite
passed 214/214 in 140.57 seconds. It includes QMC known-answer, invalid-input,
endpoint, maximum-index, deterministic discrepancy, explicit-state,
concurrency, and zero-allocation checks; all 21201 compiled Sobol direction
rows are covered by an independent checksum. It also includes the source,
build-tree, install, relocation, consumer, public-header, exceptions-disabled,
documentation, and provenance-artifact checks.

The corresponding Debug/shared build passed 51/51 affected architecture,
compile, Random, package/consumer, public-surface, and documentation tests. A
Clang 19 warnings-as-errors Debug/static build passed the two new public-header
checks, QMC suite, provenance-artifact check, and benchmark (5/5). A GNU 11.4
AddressSanitizer plus UndefinedBehaviorSanitizer build passed the QMC runtime
suite. The Release/static indexed Sobol benchmark evaluated 100,000 direction
words at zero-based dimension 3 in 10,260,550 ns with zero observed allocation
calls and checksum `0xb9c9b02511675303`; this is a threshold-free local
observation, not a performance promise.

CUDA 12.9.86 on an NVIDIA GeForce RTX 3060 Laptop GPU (compute 8.6) passed
the CPU-only QMC test and all five existing Random CUDA runtime/parity/
benchmark checks, with no skip. That real-device run is regression evidence
for the pre-existing Philox/`Uniform01` and storage-facet contracts only. The
Issue 14 APIs accept no execution context and are not marked GPU-verified;
they perform no transfer, synchronization, provider fallback, or hidden
allocation.

## Issue 13 Random engine and distribution evidence

Issue 13 adds nine implemented crosswalk rows for explicit seed acquisition,
four stateful engines, generic value composition, uniform integer/real, and
scalar Box-Muller normal generation. These additions are CPU-only and
storage-neutral. No new CUDA row is declared; real-device checks cover only
the pre-existing Philox/`Uniform01` CUDA contract.

On Linux/WSL2 6.18 with GNU C++ 11.4, CMake 4.1.2, and the installed
ASCCMake 0.1.0 package, the warnings-as-errors Debug/static complete build
compiled all 51 public headers normally and with exceptions disabled. After
correcting one stale Milestone-2 inventory oracle, the affected architecture,
compile, Random, documentation, public-surface, and Random package selection
passed 55/55. The final complete suite passed 210/210 in 142.95 seconds.

The corresponding Debug/shared build passed 31/31 affected tests, including
the source/install/relocation 51/39-header projections, build-tree and
installed/relocated Random consumers, and the selected ELF ABI observation.
An AddressSanitizer plus UndefinedBehaviorSanitizer build passed all six
Random runtime/statistical tests. Fixed statistical workloads are 65,536
uniform integer samples, 65,536 uniform-real samples, and 131,072 standard
normal samples with the thresholds justified by ADR 0020.
An independent Clang 19 warnings-as-errors Debug/static build also passed the
same six Random runtime/statistical tests.

The Release/static threshold-free CPU benchmark exercised all four engines,
uniform integer/real, and scalar normal generation with zero observed
successful-path allocations and all checksum/moment guards passing. Exact
local timings and observations are in the [performance record](performance.md).

CUDA 12.9.86 with driver 576.83 on an NVIDIA GeForce RTX 3060 Laptop GPU
(compute 8.6) passed the 12-test architecture selection and 5/5 existing
Random CUDA runtime/parity/benchmark checks. This is regression evidence only
for the pre-existing Philox/`Uniform01` and Random storage facets. The nine
Issue 13 rows remain CPU-only and are not marked GPU-verified.

## Issue 12 Random architecture evidence

Issue 12 adds no engine, distribution, sampler, helper, adapter, target,
dependency, or backend implementation. Its 33-row crosswalk contains 18
clean-room-required, three incomplete, one permission-route, and 11 rejected
rows; none is marked implemented or backend-verified by this milestone.

On 2026-08-01, a clean GNU 11.4 Debug/static warnings-as-errors build used
CMake 4.1.2 and the real ASCCMake 0.1.0 package. The complete suite passed
201/201 in 174.00 seconds. It includes manifest drift and invalid-fixture
checks, current Random
known-answer/endpoint, invalid-input, empty/layout/thread/allocation tests, 47
package-labeled checks, downstream consumers, documentation consistency, and
three correctness-guarded CPU benchmarks.

After the final diff review tightened the row, source-inventory, and complete
provenance-metadata identities, exact scalar/QMC mappings, strict schema, and
negative tamper coverage, the affected static architecture/documentation
selection passed 2/2 on the final files. The Random contract check also passed
directly with CMake 4.4.0.

The corresponding Debug/shared build passed the 12-test architecture label
and a separate 12-test selection covering current Random CPU behavior,
allocation, package/install/relocation, documentation, and the Random storage
benchmark. A CUDA 12.9.86 Release/static build for architecture 8.6 passed the
12-test architecture label and 5/5 existing Random CUDA runtime/parity/
benchmark checks on the NVIDIA GeForce RTX 3060 Laptop GPU with driver 576.83.
No new Random feature is approved for CUDA, and this real-backend regression
does not verify a planned Issue 13–15 GPU row.

No sanitizer rerun is claimed: the milestone changes no production C++ or
data, and the complete static plus shared/CUDA affected selections exercise the
changed generator, CMake registration, and documentation. Statistical sample
sizes and thresholds are frozen for child implementation, but no unimplemented
statistical row is reported as exercised.

## Issue 11 BLAS completion audit

Issue 11 adds no product capability. On 2026-08-01, the independent audit
resolved the frozen 229-row inventory to 182 verified rows and 47
standards-based not-applicable rows, with no planned, implemented, or blocked
row. The 49-family dense-to-sparse crosswalk resolves to 11 verified analogues
and 38 standards-based not-applicable mappings. The audit removed four
incorrect `gemmtr` API/header claims for excluded extension rows and completed
11 stale crosswalk states after verifying their Sparse analogues.

Fresh GCC 11.4 Debug/static and Debug/shared builds used the real ASCCMake
0.1.0 package and warnings as errors. Their complete suites passed 200/200 in
161.29 seconds and 202/202 in 147.25 seconds, respectively. These runs include
the generated-report drift check, negative manifest fixtures, conformance,
invalid-input and edge-case tests, package and relocation aggregates,
downstream consumers, documentation checks, and correctness-guarded
benchmarks. The AddressSanitizer plus UndefinedBehaviorSanitizer selection
passed 157/157 in 41.48 seconds with package-labeled downstream builds excluded
because sanitizer runtime requirements are intentionally not exported to
consumers.

CUDA 12.9.86 Release/static and Release/shared builds used compute
architecture 8.6. In each linkage mode, the 18-test BLAS audit selection passed
18/18 on the NVIDIA GeForce RTX 3060 Laptop GPU (driver 576.83, CUDA driver API
12.9). The selection exercised all three Dense BLAS levels, all applicable
Sparse BLAS compute families, CPU/GPU conformance, explicit completion, and
four correctness-guarded CPU/GPU benchmarks. No forced-no-device result was
used as verification.

The shared CUDA package/consumer selection passed 10/10 in 755.15 seconds. It
covered build-tree and installed/relocated consumers for Dense, Sparse, Dense
CUDA, and Sparse CUDA, followed by the complete build-tree component aggregate
(358.65 seconds) and installed/relocated component aggregate (309.60 seconds).
The audit found no retired `linalg` interface text in product headers, sources,
benchmarks, or build logic.

## Issue 10 Sparse BLAS evidence addendum

Issue 10 preserves the recorded environment and support boundaries. On
2026-08-01, the final corrected GCC 11.4 Debug/static CPU suite passed 199/199
in 155.81 seconds and the Debug/shared suite passed 201/201 in 144.93 seconds.
The GCC ASan+UBSan suite passed its complete registered 156/156 selection in
34.54 seconds. These runs include the standardized S/D/C/Z Sparse BLAS
conformance, invalid-enum and invalid-structure rejection, empty and signed
stride cases, zero-alpha no-read checks, allocation probes, architecture,
documentation, package, relocation, and downstream-consumer checks.

CUDA 12.9.86 static and shared builds exercised all 36 declared Sparse BLAS
rows on the NVIDIA GeForce RTX 3060 Laptop GPU (driver 576.83, CUDA driver API
12.9, compute capability 8.6). The final static Issue 10 selection passed 9/9
and the corresponding shared selection passed 9/9, including real-device
S/D/C/Z conformance, explicit completion, and the benchmark. The final shared
package/consumer selection passed 6/6: build-tree and installed/relocated
consumers for both Sparse and Sparse CUDA, plus the build-tree and
installed/relocated component aggregates. The shared package aggregates passed
in 304.23 and 339.81 seconds. Optimized CPU remains not applicable, and no
forced-zero-device result verifies a CUDA row.

The final correctness-guarded Issue 10 Release observations were:

| Probe row | Observation |
| --- | --- |
| Sparse CPU indexed dot | 70.6143 ns/iteration over 1,024 iterations; zero allocations; independent oracle |
| Sparse CPU CSR SpMM | 1,246.91 ns/iteration over 64 iterations; zero allocations; independent oracle |
| Standardized CUDA CSR SpMV float | 227.381 million nonzeros/s over 20 repetitions; zero workspace and operation allocations; independent oracle |
| Standardized CUDA CSR SpMV double | 215.335 million nonzeros/s over 20 repetitions; zero workspace and operation allocations; independent oracle |

These values are threshold-free observations, not speedup claims or support
guarantees.

## Issue 9 Level 3 evidence addendum

Issue 9 leaves the recorded Milestone 8 environment and support boundaries
unchanged. On 2026-08-01, the complete GCC 11.4 Debug/static CPU suite passed
198/198 and the complete Debug/shared suite passed 200/200. The exact Level 3
CPU test also passed under ASan+UBSan.

CUDA 12.9.86 static and shared builds exercised all 30 declared Level 3 rows
on the NVIDIA GeForce RTX 3060 Laptop GPU (driver 576.83, compute capability
8.6). The shared affected selection passed 24 tests with two intentional
forced-zero-device skips and zero failures; it included CPU and real-device
Level 3 conformance, explicit completion, benchmark, architecture,
documentation, build-tree/install/relocation consumer, and package aggregate
checks. The package aggregate passed in both the build tree and relocated
installation. Optimized CPU remains not applicable, and no CUDA row is
verified from a forced-no-device execution.

## Language, build, and package contract

| Item | Candidate contract | Evidence boundary |
| --- | --- | --- |
| C++ | C++20; extensions disabled on ASCCpp targets | Public headers are checked as first includes and with exceptions disabled where the compiler supports it |
| CMake | 3.25 or newer | A run on one CMake version does not prove all later versions |
| ASCCMake | exact released `ASCCMake` 0.1.0 when building ASCCpp | It is a producer dependency, not a dependency of the installed ASCCpp package |
| linkage | static and shared selected by `BUILD_SHARED_LIBS` | Each mode needs independent package and consumer evidence |
| build modes | Debug and Release | Evidence applies only to the mode actually built and tested |
| package | relocatable CMake config package | Build tree, copied build tree, installed prefix, relocated prefix, and paths containing spaces are separate checks |
| registry | CMake user package registry not required | Validation supplies an explicit package directory or prefix and disables the registry |

All imported product targets propagate `cxx_std_20`. `ASC::expression`,
`ASC::random_dense`, `ASC::random_sparse`, and `ASC::cpp` are interface
targets; the other provider-free targets and all CUDA facets are compiled
targets.

## Locally available Milestone 8 environment

| Layer | Available locally |
| --- | --- |
| operating system | Ubuntu 22.04.5 LTS under WSL2, Linux 6.18.33.2, x86-64 |
| candidate identity | uncommitted cumulative candidate on `feature/asc-cpp-m8-hardening-downstream-r2`, based on `33b261ea33616a6395c4ad3b20646093103344f7`; product/build-input SHA-256 `e7feae4784079c3edf331940b1ea8373f9c0748d4e20ad49ae0b2cbcae401f69` |
| CPU and memory | 11th Gen Intel Core i7-11800H, 1 socket, 8 cores, 16 hardware threads; 16,244,380 kB total memory |
| host compilers | GCC 11.4.0 and Clang 19.0.0 |
| CMake and generator | CMake 4.1.2 and GNU Make 4.3 |
| benchmark build | GCC 11.4.0, C++20, Release/shared, `-O3 -DNDEBUG`; GNU libstdc++.so.6.0.30 |
| CUDA | toolkit/compiler 12.9.86, driver 576.83 |
| GPU | NVIDIA GeForce RTX 3060 Laptop GPU, 6144 MiB, compute capability 8.6 |
| safety tools | ASan, UBSan, LSan, TSan, and NVIDIA Compute Sanitizer 2025.2.1 |

The corrected CPU evidence applies only to the explicitly named
compiler/configuration/linkage rows below. CUDA evidence applies only to the
named CUDA compiler, host compiler, build mode, linkage, and architecture
combination; it is not inferred for other CPU or CUDA combinations.
Corrected CUDA-disabled/provider-free GCC Release static/shared and Clang
Debug/static or Release static/shared rows remain `skipped`; earlier
pre-correction observations do not upgrade them to corrected evidence. This
does not apply to the separately listed CUDA-enabled GCC/NVCC Release/shared
architecture-86 row.
Windows/MSVC, macOS/AppleClang, other standard libraries and CPU
architectures, other CUDA toolkits/drivers/architectures, multi-GPU and
peer-access topologies, MIG, non-CUDA accelerators, Ninja, and hosted CI are
`skipped` until actually exercised.

ELF baselines are enforced only when the complete recorded environment
selector matches: operating system and architecture, compiler identity and
full version, standard library, configuration, linkage, inspection-tool
identities, and every applicable CUDA compiler, host compiler, and architecture
field. Other environments produce a non-enforcing observation or explicit
skip and cannot be reported as a baseline pass.

CMake exposes its detected CUDA host compiler ID and version beginning with
3.31. On CMake 3.31 or newer, the exact CUDA-host integration is registered
and only CMake's trusted detected fields reach the selector; a baseline may
enforce only when those fields and the rest of the complete selector match. On
supported CMake 3.25 through 3.30, exact CUDA-host identity enforcement is
unavailable: ASCCpp ignores same-named raw cache variables, registers the
direct unavailable-identity and nested cache-spoof regressions, and selects an
empty baseline. That is fail-closed, non-enforcing behavior, not an inferred
or caller-asserted exact-host match. When NVCC chooses its default host and
the outer host override is empty, the nested spoof driver omits that cache
argument instead of rejecting the valid configuration; the same empty
baseline remains required.

## Corrected local matrix

| Exact combination | Result |
| --- | --- |
| GCC 11.4 Debug/static, CUDA disabled | full suite passed 193/193, zero failed; 129.71 seconds |
| GCC 11.4 Debug/shared, CUDA disabled | full suite passed 195/195, zero failed; 132.96 seconds |
| Clang 19 Debug/shared, CUDA disabled | full suite passed 194/194, zero failed; 150.54 seconds |
| GCC 11.4 ASan+UBSan, CUDA disabled | selected sanitizer suite passed 139/139; 2.08 seconds |
| Clang 19 LSan, CUDA disabled | bounded sanitizer-safe subset passed 12/12; 0.13 seconds |
| Clang 19 TSan, CUDA disabled | bounded sanitizer-safe subset passed 12/12; 0.29 seconds |
| final CPU package/tooling selection | passed 25/25; 83.84 seconds |
| GCC 11.4 C++ + NVCC 12.9.86 with GNU 11 host, Release/shared, architecture 86 | 262 registered: 252 passed, 10 intentional forced-no-device tests skipped through CTest code 77, zero failed; 1483.00 seconds |
| dedicated hook-enabled static focused selection | passed 4/4; 1.24 seconds |
| Compute Sanitizer memcheck | passed 13/13 with zero reported errors and zero leaks |
| final CUDA package aggregate against the strengthened workspace guard | passed 2/2: build-tree package 305.07 seconds; install/relocate package 296.75 seconds; total 601.82 seconds |
| official CMake 3.25.3, CUDA 12.9.86 with GNU 11.4 host, Release/shared, architecture 86 | configured; built `asc_core_cuda`; selector selection passed 3/3 in 29.29 seconds; generated `elf_abi` baseline was empty |
| official CMake 3.30.9, CUDA 12.9.86 with GNU 11.4 host, Release/shared, architecture 86 | configured; built `asc_core_cuda`; selector selection passed 3/3 in 29.94 seconds; generated `elf_abi` baseline was empty |
| explicit outer CMake 3.30.9 raw-cache spoof with `GNU`/`11.4` host values | selector and unavailable-identity tests passed 2/2 in 0.01 seconds; variables were reported ignored and the generated `elf_abi` baseline was empty |
| official CMake 3.30.9 with NVCC default host, no explicit host override, Release/shared, architecture 86 | configured with `CMAKE_CUDA_HOST_COMPILER` empty; built `asc_core_cuda`; generated `elf_abi` baseline was empty; selector selection passed 3/3 in 30.53 seconds and independent portability rerun passed 3/3 in 32.37 seconds |
| CMake 4.1.2 trusted-field guard, selector unit, and exact unlike-host integration | passed 3/3 in 36.39 seconds within 262 registered tests; the unlike detected host remained non-enforcing |
| forced CUDA enumeration failure | exact process exit 2, passed; not registered as a CTest skip |
| successful forced zero-device enumeration | exact process exit 77, correctly skipped |
| Release/shared CPU and CUDA benchmark selection | passed 6/6 in 11.04 seconds; all observations were threshold-free and correctness-guarded |
| explicit real asc-xde repository audit | passed 1/1 in 2.89 seconds; expected commit and exact worktree status were unchanged |

The LSan and TSan results are intentionally bounded subsets. They do not imply
whole-suite sanitizer evidence where deliberate allocation interposition
conflicts with sanitizer allocator interceptors.

The package aggregate was rerun after all recursive fixture deletions,
including former direct `cmake -E rm -rf` registrations, were routed through
the guarded test-workspace helper. Its regression recursively scans every test
CMake file and rejects all supported recursive-deletion spellings outside that
helper. The 2/2 package result therefore covers build-tree,
copied-build-tree, installed, relocated, and path-with-spaces consumers after
that correction.

The ten no-device results use the dedicated forced-no-device test seam and
CTest skip code 77. They verify result classification without converting
missing hardware into a pass or failure. No `CUDA_VISIBLE_DEVICES` masking
result is claimed: with CUDA 12.9 in this environment, independently
initializing `libcudart` outside ASCCpp aborts under that masking mechanism
before ASCCpp can classify the device state.

Only a successful enumeration returning zero devices maps to code 77. A CUDA
enumeration/provider error prints the provider status and exits 2; the exact
forced-failure regression passed and is not classified as skipped.

The benchmark result means that all six named correctness-guarded probes
completed successfully. Their timing, throughput, allocation, and checksum
records are threshold-free observations from this one Release/shared
environment. They are not acceptance thresholds, cross-machine guarantees, or
speedup claims.

The final one-run observations were:

| Probe row | Observation |
| --- | --- |
| Dense CPU double 32x32 | evaluate 58,445.4 ns/iteration; GEMM 18,160.8 ns/iteration |
| Sparse CPU CSR 128x256, 512 nonzeros | evaluation 144,669 ns/iteration; SpMV 6,350.32 ns/iteration |
| Random storage CPU | Dense layout-left 24,205,012 ns and layout-right 25,822,548 ns over 100 repetitions; Sparse 298,307,733 ns over 10 repetitions |
| Dense CUDA | H2D 6.999 GB/s; D2H 7.381 GB/s; D2D 79.689 GB/s; terminal evaluation 6.053 GB/s; AXPY 30.607 GFLOP/s; GEMV 25.196 GFLOP/s; GEMM 2.469 TFLOP/s |
| Sparse CUDA CSR SpMV | float 119.649 million nonzeros/s; double 112.434 million nonzeros/s |
| Random CUDA | raw Philox 15.377 billion items/s; Dense `Uniform01` 11.639 billion items/s; Sparse `Uniform01` 26,808.3 items/s |

Dense and Sparse CPU rows, both CPU Random layouts and Sparse generation, and
all Dense CUDA rows use operation-specific independent oracles and print
`oracle=independent`. Sparse CUDA uses independent host float/double CSR
reference rows. Random CUDA reconstructs raw words, Dense values, and Sparse
priority/ordinal selection and values from independent Philox oracles.
Correctness checks run after the timed repetitions. CUDA timing waits for
completion of every measured repetition; setup and verification are outside
the timed interval unless the row explicitly includes allocation. Dense CPU
uses one untimed operation before each 64-iteration evaluation and
four-iteration GEMM measurement; Sparse CPU likewise uses one untimed
operation before its 64-iteration evaluation and 256-iteration SpMV
measurement. CPU Random has zero warmups: all 100 Dense and 10 Sparse
generations are timed. Dense and Random CUDA use three warmups and 12 measured
repetitions; Sparse CUDA uses three warmups and 20 measured repetitions. Probe
output records workloads, measured repetition counts, allocation scope,
checksums/oracles, and exact timing units; this matrix records the corresponding
warmup boundary.

The observation record includes the candidate digest and machine/toolchain
identity above. It is a single WSL2 run with no thermal, frequency, scheduling,
or contention normalization, so those factors remain noise boundaries rather
than silently inferred controls.

The configure-level NVCC/Clang-host selector is `configure-tested` through the
post-fix CMake 4.1.2 3/3 selection.
CMake 3.25.3 and 3.30.9 are `configure-tested` for the named CUDA/GNU-host
combination, and `ASC::core_cuda` is `compile-tested`; exact-host identity
enforcement on those CMake versions is `skipped` because the detected fields
do not exist, while their fail-closed unavailable-identity and cache-spoof
regressions passed. Runtime and parity evidence was not collected for those
two version-specific trials. The CMake 3.30.9 default-NVCC-host trial is also
`configure-tested` and its `ASC::core_cuda` target is `compile-tested`; exact
host identity, runtime, and parity are `skipped` for that trial. Compiling or
running provider facets with the NVCC/Clang-host pairing, a true Clang CUDA
compiler, Windows/MSVC CUDA, and other host/toolkit pairings remain `skipped`.

## Provider-free components

| Component | Exact direct ASC edge | Implemented capability |
| --- | --- | --- |
| `ASC::core` | none | status/result, checked metadata, configuration vocabulary, byte I/O, host memory, serial execution |
| `ASC::utilities` | `ASC::core` | transactional command-line configuration and checked steady-clock timing |
| `ASC::expression` | `ASC::core` | storage-neutral readable, placement, writable, alias, and sparsity protocols |
| `ASC::dense` | `ASC::core`, `ASC::expression` | host storage/views, pointwise evaluation, reductions, complete reference real/complex BLAS Levels 1, 2, and 3 |
| `ASC::sparse` | `ASC::core`, `ASC::expression` | canonical coordinate/CSR/CSC storage, conversion, evaluation, and all 36 applicable S/D/C/Z Sparse BLAS compute rows |
| `ASC::random` | `ASC::core` | Philox4x32-10 words and exact float/double `Uniform01` transforms |
| `ASC::random_dense` | `ASC::random`, `ASC::dense` | logical-order host Dense `Uniform01` fill |
| `ASC::random_sparse` | `ASC::random`, `ASC::sparse` | host exact-count canonical coordinate generation |
| `ASC::cpp` | all provider-free targets | convenience aggregate; no provider behavior |

The CUDA option defaults to `OFF`. A provider-free configure or package lookup
does not enable the CUDA language, discover CUDAToolkit, import a `CUDA::`
target, or select a provider fallback.

If a CUDA facet is requested only through `OPTIONAL_COMPONENTS` and
CUDAToolkit is unavailable, that optional facet is not found while required
provider-free components remain usable. Making the same CUDA facet required
fails the package lookup.

## Optional CUDA components

CUDA facets exist only in a package built with `ASC_CPP_ENABLE_CUDA=ON`.
Building them requires a C++20-capable CUDA compiler, CUDA Toolkit 12 or newer,
and a caller-selected architecture.

| Component | Exact direct ASC edge | Private provider edge | Bounded capability |
| --- | --- | --- | --- |
| `ASC::core_cuda` | `ASC::core` | `CUDA::cudart` | device inventory, pinned/device/managed resources, stream-backed contexts, copies, events |
| `ASC::dense_cuda` | `ASC::dense`, `ASC::core_cuda` | `CUDA::cublas` | bounded float/double pointwise evaluation and complete real/complex BLAS Levels 1, 2, and 3 |
| `ASC::sparse_cuda` | `ASC::sparse`, `ASC::core_cuda` | `CUDA::cusparse` | trusted indexed/CSR/triangular clones, all 36 applicable asynchronous S/D/C/Z Sparse BLAS compute rows, retained CSR SpMV, and bounded trusted sparse evaluation |
| `ASC::random_cuda` | `ASC::random`, `ASC::core_cuda` | none | raw Philox words into a capacity-carrying `MutableMemoryView` |
| `ASC::random_dense_cuda` | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | none | logical-order Dense `Uniform01` |
| `ASC::random_sparse_cuda` | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | none | exact-count canonical coordinate/value `Uniform01` |

The three Random CUDA facets use CUDA Runtime through `ASC::core_cuda`; they do
not add a direct cuRAND edge. There is no CUDA Driver, cuRAND, cuSOLVER,
Thrust/CUB, HIP, SYCL, or implicit CPU fallback dependency.

## GPU evidence vocabulary

The project uses exactly these independent labels:

| Label | Meaning |
| --- | --- |
| `configure-tested` | CMake enabled CUDA and found the required compiler, toolkit targets, and declared architecture |
| `compile-tested` | the named provider target and applicable contracts/consumers compiled and linked |
| `runtime-tested` | the named operation executed successfully on identified real hardware |
| `parity-tested` | real-device output agreed with an approved independent numerical, structural, or bit oracle |
| `skipped` | an environment, topology, operation, or oracle was unavailable or outside the approved run |

When CUDA targets compile but no runtime device is available, device-dependent
Core and Dense runtime/performance tests use CTest `SKIP_RETURN_CODE 77`.
Absence of hardware is `skipped`; it is neither a product pass nor a product
failure. Configure and compile evidence remain separately reportable.

The corrected GCC/NVCC Release/shared architecture-86 suite ran on the
identified RTX 3060 Laptop GPU with driver 576.83:

| Facet | Corrected-candidate evidence |
| --- | --- |
| `core_cuda` | `configure-tested`; `compile-tested`; `runtime-tested` |
| `dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |

Trusted device CSC evaluation remains `skipped` because no approved producer
creates trusted device CSC storage. Multi-GPU, cross-toolkit/cross-driver,
non-Linux, other compute-capability, and hosted GPU CI evidence also remain
`skipped`.

## Limits of the matrix

The matrix does not claim:

- ABI compatibility across `0.x` minors, compilers, standard libraries,
  linkage modes, build modes, sanitizers, CUDA toolkits, platforms, or
  architectures;
- identical floating results across unlike toolchains, providers, or hardware;
- a provider operation, scalar, rank, layout, or format outside its documented
  subset;
- compatibility with deleted `asc/array*`, `asc/linalg*`, or `asc/cpp.h`;
- general broadcasting, runtime-rank owners, hidden materialization, automatic
  provider selection, or a provider registry; or
- a release, `1.0`, or long-term support commitment.

See [API compatibility](api-compatibility.md), [package
capabilities](package-capabilities.md), and [performance](performance.md) for
the corresponding boundaries.
