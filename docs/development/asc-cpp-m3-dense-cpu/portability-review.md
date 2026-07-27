# Milestone 3 Portability, GPU, and Performance Review

Status: Complete; no unresolved release blocker

Date: 2026-07-26

Branch: `feature/asc-cpp-m3-dense-cpu`

Role: independent portability/GPU/performance reviewer

## Authority, scope, and independence

This review follows the complete all-in-one runbook, frozen Milestone 3
contract and ownership ledger, accepted ADRs 0001--0004, 0007--0011, 0013,
0017, and 0018, and the final dense implementation, tests, package
integration, module guide, and specialist reviews.

The current official Google C++ Style Guide was inspected at
<https://google.github.io/styleguide/cppguide.html> on 2026-07-26. Its current
C++20, self-contained-header, direct-include, header-guard, related-header,
namespace, ownership, and portability guidance was applied together with the
repository `.clang-format`, `.clang-tidy`, and accepted ADRs.

No applicable `AGENTS.md` or `generator.md` survives in the working tree.
Their deletion is retained predecessor work and was not changed by this
review.

This role was read-only except for this report. It did not edit production,
tests, CMake/package files, architecture manifests, module documentation, or
another specialist's report. It did not inspect MdeCpp implementation or
tests, restore deleted asc-cpp material, add a dependency, or perform a Git or
remote mutation.

## Conclusion

The Milestone 3 dense CPU implementation is acceptable at Publication
Checkpoint B with **no unresolved release-blocking portability, GPU-isolation,
undefined-behavior, or performance finding**.

The implemented evidence supports:

- strict C++20 compilation with GCC 11 and Clang 19;
- static and shared Linux builds;
- exceptions-enabled and exceptions-disabled public headers and production
  source;
- ASan and UBSan execution of the focused dense suite;
- provider-free linkage and exact `core`/`expression` dependencies;
- successful-path computation without storage allocation, workspace, packing,
  transfer, synchronization, provider dispatch, or fallback;
- deterministic serial reference behavior;
- a functional, non-gating allocation benchmark.

MSVC and AppleClang remain hosted-CI evidence, not local runtime evidence. GPU
evidence is exactly **skipped**.

## Findings and disposition

| ID | Severity | Evidence | Disposition |
| --- | --- | --- | --- |
| PORT-M3-01 | residual evidence boundary | Successful evaluation, reductions, and benchmarked linalg perform no observed heap allocation. Validation failures construct the core `Status` diagnostic, whose `std::string` representation may allocate. | No code blocker. Publication language must say no computational storage/workspace allocation on successful paths; it must not claim that diagnostic creation is allocation-free on every error path. Destination-before-mutation guarantees remain intact. |
| PORT-M3-02 | residual benchmark limitation | The project-owned harness reports compiler, Debug/Release-like configuration, serial provider, shape, iterations, operations, allocation count, elapsed time, and checksum. It does not warm up separately, print layout/compiler flags/CPU, collect a timing distribution, or compare a baseline. | Accept as the frozen M3 allocation/timing observation only. Do not make a speed, regression, provider-parity, or cross-compiler comparison claim. Add richer metadata and sampling before establishing a later performance baseline. |
| PORT-M3-03 | hosted-platform evidence gap | No MSVC, Windows loader, AppleClang, macOS arm64, or multi-config runtime was available in this WSL2 session. The code and export definitions were reviewed, and CI contains VS2022 shared Debug/Release and macOS 15 AppleClang jobs. | No local blocker. Those jobs must remain required hosted validation before publication acceptance; this report does not pre-claim their result. |
| PORT-M3-04 | documented external-view precondition | `DenseView::Create` validates metadata, null/nonempty state, byte-count conversion, `ptrdiff_t` span, and `uintptr_t` end-address overflow. C++ cannot prove an external pointer's provenance, allocation length, alignment, actual memory placement, or remaining lifetime. Integer-address overlap is conservative and assumes the ordinary hosted `uintptr_t` model. | No code blocker. The module guide already states the caller preconditions. Capability-pointer or other exotic targets are outside the tested hosted-platform matrix. |
| PORT-M3-05 | conservative performance behavior | Arbitrary-stride uniqueness and byte-span overlap deliberately reject some mathematically safe layouts/aliases; logical traversal is scalar serial work and layout-agnostic rather than provider-optimized. | Approved M3 behavior, not a defect. Preserve it until an approved workspace/materialization or provider milestone supplies observable alternatives. |

Every finding was sent to the lead before this report was finalized. None
requires production scope expansion or a later-milestone implementation.

## C++20, style, and header audit

The seven dense public headers:

```text
asc/dense.h
asc/dense/array.h
asc/dense/evaluate.h
asc/dense/export.h
asc/dense/layout.h
asc/dense/linalg.h
asc/dense/view.h
```

use full-path include guards, direct includes, `.h` extensions, flat
`namespace asc`, and internal namespaces containing `internal`. No C++23
feature, module, compiler extension, `using namespace`, provider include, or
third-party forward declaration was found.

All header templates are ODR-safe. The representative multi-translation-unit
test passes. `src/dense/reference_linalg.cc` is a real compiled `.cc` source
and includes its owning public linalg header first. Repository formatting and
strict warning checks pass.

Public headers and compiled source passed both GCC 11 and Clang 19 with:

```text
-std=c++20 -pedantic-errors -Wall -Wextra
-Wconversion -Wsign-conversion -Werror
```

The public headers and compiled source also pass with `-fno-exceptions`.
This proves compilation without language exception support; it does not claim
that every possible standard-library allocation failure can be recovered in
an exception-disabled process.

## Arithmetic, pointer, lifetime, and concurrency audit

### Layout arithmetic

- Signed extents and strides are validated before unsigned conversion.
- Logical sizes, affine terms, coordinate sums, required spans, and byte
  counts use checked operations.
- Rank zero publishes logical/span size one.
- A zero extent publishes logical/span size zero and vacuous
  uniqueness/exhaustiveness.
- Named-layout propagation stops across a zero boundary rather than evaluating
  an irrelevant overflowing product.
- Nonempty overflow remains a structured error.
- The arbitrary-stride uniqueness proof is sufficient and intentionally
  conservative.

No unchecked signed overflow was found in validated iteration or access paths.
Iteration counters reach the final valid signed index and reset or terminate
before another increment.

### Views and address arithmetic

- Nonempty null views reject.
- Mutable views require a proven-unique mapping.
- Host dereference rejects pinned, managed, and device spaces in M3.
- Empty subviews preserve the parent pointer and avoid unnecessary pointer
  arithmetic.
- Nonempty subview offsets are checked and converted before pointer addition.
- View construction checks element spans against `ptrdiff_t`, byte
  multiplication against `size_t`, and integer end-address overflow.
- Physical byte-span overlap catches independently created overlapping dense
  views and is conservative for padding.

Unchecked expression reads and linalg helpers are reached only after the
built-in dense path validates shape and host accessibility. External
expression adapters remain responsible for truthful placement, indexing, and
alias metadata as required by the storage-neutral expression protocol.

### Ownership and concurrency

`DenseArray` is move-only, binds only to unqualified core `Extents`, and owns
only trivial arithmetic elements. Value initialization starts object lifetime;
omitting explicit element destruction is valid because supported elements are
trivially destructible. Buffer ownership remains RAII and resource lifetime is
explicit.

Clone performs one destination allocation and explicit serial host copy.
Discard-resize constructs the replacement before move assignment, so failure
does not invalidate the original owner or views. Successful resize and owner
destruction retain their documented invalidation/lifetime obligations.

Production dense code contains no global mutable state or hidden cache.
Separate non-overlapping instances can execute independently. The caller must
synchronize overlapping mutation. M3 execution is synchronous and does not
retain a context, view, workspace, or event after return. ThreadSanitizer was
not run because M3 introduces no parallel execution contract; this is not
evidence for unsynchronized same-storage mutation.

## Static, shared, ABI, and loader audit

The CMake target is a real compiled library:

```text
asc_dense / ASC::dense
```

Its exact public links are:

```text
ASC::core;ASC::expression
```

The static export publishes `ASC_DENSE_STATIC_DEFINE`; the shared build
publishes no static define and privately defines
`ASC_DENSE_BUILDING_LIBRARY`. Windows declarations use
`__declspec(dllexport/dllimport)`, while GCC/Clang use default visibility
against a hidden target baseline.

Linux shared-symbol inspection found all compiled reference entry points
exported. `libasc_dense.so` had only `libasc_core` plus ordinary C++/C runtime
dependencies; no utility, sparse, random, BLAS/LAPACK, GPU, or other provider
dependency appeared. The build-tree `RUNPATH` was expectedly build-local;
installed/relocated consumers passed through the CTest fixtures.

The static and shared generated target exports both publish `cxx_std_20` and
the exact core/expression closure. The static export additionally publishes
the required static visibility definition.

Windows DLL behavior and macOS install-name behavior were reviewed but not
executed locally. The CI jobs are the authoritative hosted validation for
those loaders and ABIs.

## Provider and GPU isolation

Searches across dense production, target integration, tests, consumer, and
benchmark inputs found no CUDA, HIP, SYCL, cuBLAS, cuSOLVER, cuSPARSE, cuRAND,
Eigen, BLAS/LAPACK, oneMKL, OpenMP, or TBB integration.

The local host inventory happens to include:

```text
CUDA toolkit: 12.9
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
driver: 576.83
```

That inventory is not Milestone 3 evidence. No GPU component or source exists,
no GPU option/provider discovery was requested, no provider translation unit
was compiled, and no device operation or CPU/GPU parity comparison was run.

GPU evidence: **skipped**.

No `configure-tested`, `compile-tested`, `runtime-tested`, or `parity-tested`
GPU claim is made.

## Performance and hidden-work audit

The reference algorithms dispatch directly once and use scalar serial loops:

- mapping/access/subview work is rank-dependent and allocation-free;
- owner create/resize performs its one documented buffer allocation;
- clone performs one destination allocation and one explicit host copy;
- evaluation and reductions use no computational allocation or workspace;
- Copy, Scal, Axpy, Dot, Nrm2, Gemv, and Gemm allocate no computational
  storage or workspace;
- no operation packs, transfers, densifies, synchronizes, changes precision,
  selects a provider, or falls back;
- `beta == 0` branches before reading Gemv/Gemm output;
- `Nrm2` uses scaled sum-of-squares;
- traversal and reduction order are deterministic.

The project-owned allocation counter covers ordinary, array, aligned, sized,
and nothrow global allocation forms on the tested platform. It is an
observation of process-level C++ heap allocations during the measured scope,
not proof about every allocator or system call on every platform.

Independent Debug-like observations on the WSL2 host were:

```text
GCC 11.4.0, static:
shape=32x32 iterations=100 operations=evaluate-add,gemm
allocations_in_operations=0 elapsed_us=245248 checksum=4.75

Clang 19.0.0, shared:
shape=32x32 iterations=100 operations=evaluate-add,gemm
allocations_in_operations=0 elapsed_us=371768 checksum=4.75
```

The machine was WSL2 x86-64 on an Intel Core i7-11800H with 16 logical CPUs.
The benchmark itself is serial. These single Debug timings include normal
host noise, differ by compiler/linkage, have no warm-up or distribution, and
must not be compared as a performance conclusion.

The independent verifier separately reported a Release-like allocation
observation of zero allocations and 20,585 microseconds. It likewise has no
threshold or speed claim.

## Independent commands and results

Fresh review root:

```text
/tmp/asc-cpp-m3-portability.eVDcBj
```

Tool inventory:

```text
CMake 4.1.2
CMake 3.25.0 minimum-version probe
GCC 11.4.0
Clang 19.0.0
clang-format 19.0.0
Linux WSL2 x86-64
```

### GCC 11 static Debug

```bash
cmake -S . \
  -B /tmp/asc-cpp-m3-portability.eVDcBj/gcc-static \
  -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build /tmp/asc-cpp-m3-portability.eVDcBj/gcc-static --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-portability.eVDcBj/gcc-static \
  --output-on-failure -L dense
```

Result: configure pass, build pass, **27/27 passed**. The result includes 14
header/exceptions configurations, runtime/numerical tests, positive and
negative compile contracts, multi-TU, benchmark, subproject, build-tree
consumer, and installed relocation consumer.

### Clang 19 shared Debug

```bash
cmake -S . \
  -B /tmp/asc-cpp-m3-portability.eVDcBj/clang-shared \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build /tmp/asc-cpp-m3-portability.eVDcBj/clang-shared --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-portability.eVDcBj/clang-shared \
  --output-on-failure -L dense
```

Result: configure pass, shared build pass, **27/27 passed**, including the
three package/consumer fixtures.

### Clang 19 ASan and UBSan

```bash
cmake -S . \
  -B /tmp/asc-cpp-m3-portability.eVDcBj/clang-asan-ubsan \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build /tmp/asc-cpp-m3-portability.eVDcBj/clang-asan-ubsan \
  --parallel 4 --target \
  asc_dense_layout_view_test \
  asc_dense_array_test \
  asc_dense_evaluate_test \
  asc_dense_linalg_test \
  asc_m3_dense_contract \
  asc_m3_dense_multi_tu \
  asc_dense_allocation_free_benchmark
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir \
  /tmp/asc-cpp-m3-portability.eVDcBj/clang-asan-ubsan \
  --output-on-failure \
  -R '^asc_cpp\.dense\.(layout_view|array|evaluate|linalg|compile_contract|multi_tu|allocation_free_benchmark)$'
```

Result: sanitizer configure/build pass, **7/7 passed**, no ASan, leak, or
UBSan diagnostic.

The lead independently completed the larger eligible Clang 19 ASan+UBSan
matrix with:

```text
94/94 passed
```

using:

```text
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:detect_leaks=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
```

That larger count is lead integration evidence; the 7/7 result above is this
reviewer's independently executed focused result.

### Minimum CMake and strict exception-disabled source

```bash
/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/cmake -S . \
  -B /tmp/asc-cpp-m3-portability.eVDcBj/cmake325-release \
  -DCMAKE_CXX_COMPILER=g++-11 \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
/tmp/asc-cpp-m0-cmake325.y9vVdy/venv/bin/cmake \
  --build /tmp/asc-cpp-m3-portability.eVDcBj/cmake325-release \
  --parallel 4 --target asc_dense
```

Result: CMake 3.25.0 configure and Release `asc_dense` build pass.

For each of GCC 11 and Clang 19:

```bash
<compiler> -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -fno-exceptions \
  -DASC_DENSE_BUILDING_LIBRARY -Iinclude \
  -c src/dense/reference_linalg.cc \
  -o /tmp/asc-cpp-m3-portability.eVDcBj/reference.o
```

Result: both exception-disabled strict source compilations pass.

### Formatting, dependency, and symbol probes

```bash
clang-format --dry-run --Werror -style=file \
  include/asc/dense.h include/asc/dense/*.h \
  src/dense/reference_linalg.cc \
  tests/dense/*.cc tests/dense/*.h \
  tests/compile/m3_dense*.cc tests/compile/m3_dense*.h \
  tests/consumer/dense/main.cc \
  benchmarks/dense/allocation_free_benchmark.cc
git diff --check -- \
  include/asc/dense.h include/asc/dense src/dense \
  tests/dense tests/compile/m3_dense* tests/consumer/dense \
  benchmarks/dense
nm -u \
  /tmp/asc-cpp-m3-portability.eVDcBj/gcc-static/src/dense/libasc_dense.a
nm -D --defined-only \
  /tmp/asc-cpp-m3-portability.eVDcBj/clang-shared/src/dense/libasc_dense.so
ldd \
  /tmp/asc-cpp-m3-portability.eVDcBj/clang-shared/src/dense/libasc_dense.so
```

Result: format pass; whitespace pass; no forbidden sibling/provider symbol or
dynamic dependency; expected dense entry points exported.

`clang-tidy` was not available in this local host. The configured CI Clang job
retains the production-source tidy invocation, so no local static-analysis
pass is claimed here.

## Evidence matrix

| Concern | Local evidence | Result |
| --- | --- | --- |
| GCC C++20 strictness | GCC 11 static Debug build | passed |
| Clang C++20 strictness | Clang 19 shared Debug build | passed |
| minimum CMake | CMake 3.25.0 GCC Release target build | passed |
| headers with exceptions | GCC and Clang header tests | passed |
| headers without exceptions | GCC and Clang `-fno-exceptions` tests | passed |
| compiled source without exceptions | direct GCC and Clang strict compile | passed |
| static package/consumer | GCC dense CTest fixtures | passed |
| shared package/consumer | Clang dense CTest fixtures | passed |
| ASan/UBSan | focused Clang dense matrix | 7/7 passed |
| broader ASan/UBSan | lead eligible non-package matrix | 94/94 passed |
| Windows/MSVC shared multi-config | code/CI review only | pending hosted CI |
| macOS/AppleClang arm64 | code/CI review only | pending hosted CI |
| ThreadSanitizer | no parallel M3 contract | not run |
| optimized CPU provider | not implemented in M3 | no claim |
| GPU | outside M3 implementation | **skipped** |
| performance | allocation/checksum/timing observation | no speed claim |

## Remaining risks

- Non-owning views can dangle if callers violate owner/resource lifetime.
- External expression adapters can misreport placement or aliasing.
- Conservative span aliasing can reject safe padded/interleaved cases.
- Scalar reference kernels are correctness baselines, not optimized provider
  performance.
- Hosted Windows/MSVC and macOS/AppleClang results must come from CI.
- Debug WSL2 benchmark timings are not baselines.
- Error diagnostic transport may allocate even though successful numerical
  kernels do not allocate computational storage.
- Capability-pointer and other non-ordinary `uintptr_t` architectures are
  outside the supported/tested matrix.

None of these risks justifies a Milestone 3 scope expansion. They are either
documented caller obligations, conservative approved behavior, hosted
validation gates, or later provider/GPU work.

## Final classification

CPU serial reference behavior: runtime-tested locally with GCC and Clang.

Optimized CPU provider behavior: not implemented and not claimed.

GPU evidence: **skipped**.

Independent portability/GPU/performance disposition: accept Milestone 3 at
Publication Checkpoint B, subject to the lead's complete integration matrix
and required hosted CI before publication.
