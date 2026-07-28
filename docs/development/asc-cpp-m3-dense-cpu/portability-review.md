# Milestone 3 Dense CPU portability, GPU, and performance review

Status: complete; approved for Publication Checkpoint B

Date: 2026-07-28

## Scope and independence

This review covers only the frozen Milestone 3 Dense CPU contract and the
integrated Milestone 3 candidate. It used the ownership ledger, ADRs 0004 and
0007--0011, ADR 0013, the dependency and capability manifests, the backend
matrix, the current production and verification sources, and the independent
production, verification, and documentation reviews.

The review did not inspect MdeCpp, deleted asc-cpp code, or a later milestone
implementation. Its only write is this report.

No unresolved portability, GPU, or performance release blocker remains.

## Findings and resolutions

### P1: expression source accessibility was not preflighted

Initial severity: release blocking.

`Evaluate` initially validated the serial context and destination memory but
did not validate the memory space of a direct or nested `DenseView` source.
A device-marked source could therefore reach the Dense adapter's `Read`
operation, whose checked access terminates on an inaccessible pointer, after
evaluation had entered its mutation phase.

The lead added the non-breaking optional
`ValidateExpressionAccess(context, expression)` customization to Expression.
Built-in unary and binary nodes propagate it recursively. The Dense adapter
returns `kUnsupported` for non-serial execution and `kMemoryAccess` for
non-host or inaccessible storage. `Evaluate` invokes the hook before shape,
alias, or mutation work. Independent direct and nested device-source tests now
return `kMemoryAccess` with the host destination byte-for-byte unchanged.
Closed.

### P2: the allocation probe was not MSVC-portable

Initial severity: release blocking for the required Windows job.

The verification allocation probe initially used `std::aligned_alloc`
unconditionally. MSVC's CRT requires `_aligned_malloc` and `_aligned_free`.
The verifier added the `_MSC_VER` branch, matching aligned delete overloads,
zero-size handling, and checked rounding on the non-MSVC path. It subsequently
added ordinary and aligned scalar/array nothrow overloads so Core's nothrow
allocation family pairs with the probe's matching delete family.

GCC and Clang strict builds, ASan+UBSan, and formatting pass after the change.
The Windows branch is source-reviewed; native MSVC execution remains a hosted
CI gate. Closed for local Publication Checkpoint B.

### P3: standalone LeakSanitizer conflicts with allocation instrumentation

Severity: expected tool/test incompatibility, not a product defect.

The three Dense runtime tests and the benchmark deliberately provide strong
global `new` and `delete` replacements to count allocations. Clang's
standalone LeakSanitizer provides the same strong interceptor symbols, so
those targets cannot link together. This is the same incompatibility already
recorded for earlier allocation-counting tests.

The all-Dense LSan attempt is classified as incompatible. Five Dense-linked
tests without the allocation probe passed under standalone LSan. The complete
allocation-sensitive Dense runtime suite passed under ASan+UBSan, whose
interposition model is compatible here. No production change is warranted.

## C++20, arithmetic, pointer, and lifetime review

- Dense rank, extent, index, and stride metadata use the approved Core types.
  Layout construction rejects negative metadata and uses checked multiply,
  add, and conversion operations for logical sizes, strides, offsets, spans,
  and byte counts.
- Rank zero publishes logical/span size one. Zero-extent mappings publish
  logical/span size zero. The strided uniqueness proof is conservative and
  cannot publish a repeated-address mutable mapping.
- The unchecked inner linear-algebra offset loop is reached only with
  non-negative in-bounds coordinates and a mapping whose maximum offset was
  checked at construction. Its signed products and sums are therefore bounded
  by the validated maximum offset.
- View creation checks `required_span_size * sizeof(Element)`. Linear-algebra
  overlap and expression alias calculations reuse that invariant and
  conservatively report overlap if adding a byte span would wrap
  `uintptr_t`.
- Pointer-to-`uintptr_t` address ordering is implementation-defined rather
  than fully abstract-machine-portable. It is supported by the required
  GCC/Clang/MSVC platforms; overflow becomes a conservative alias result.
  Capability or segmented-pointer platforms are not claimed.
- Raw-pointer views cannot verify allocation capacity, alignment, lifetime,
  concurrent mutation, or dangling use after owner destruction/move/resize.
  Those remain documented caller obligations. Owners use Core `Buffer`,
  retain an explicit non-owning resource pointer, and release exactly once.
- Owner construction, clone, and discard-resize use result/status paths.
  Failed replacement construction leaves the original owner intact.
  Successful resize invalidates prior views as documented.
- The implementation uses C++20 standard facilities available in GCC 11,
  Clang 19, current MSVC, and AppleClang. It introduces no compiler extension,
  third-party header, provider SDK, or platform API in production.

## Static/shared, DLL visibility, RPATH, and packaging

- `asc_dense` is a genuine compiled library with exported alias `ASC::dense`
  and direct public links exactly `ASC::core;ASC::expression`.
- Static builds propagate `ASC_DENSE_STATIC_DEFINE`. Shared builds define
  `ASC_DENSE_BUILDING_LIBRARY` privately. `ASC_DENSE_EXPORT` selects
  `__declspec(dllexport/dllimport)` on Windows and default ELF/Mach-O
  visibility for GCC/Clang.
- Dynamic-symbol inspection of the Clang shared library found only the 20
  approved float/double `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and
  `Gemm` overloads.
- The installed ELF Dense library records `NEEDED libasc_core.so` and
  `RUNPATH [$ORIGIN]`. The Apple branch uses `@loader_path`. Windows runtime
  artifacts install to the package runtime directory and the isolated
  consumer fixture stages DLLs beside its executable.
- GCC static and Clang shared build-tree, installed, relocated,
  path-with-spaces, component-selection, registry-isolation, subproject, and
  Dense-only consumer cases passed in the independent 42-test Milestone 3
  matrices.
- CMake 3.25.0 configured and built the Release/static `asc_dense` target
  successfully. Current CMake 4.1.2 configured, built, installed, and
  relocated both local matrices.

## Exceptions and sanitizer evidence

All seven public headers compile standalone with exceptions disabled under
GCC and Clang. Dense's status/result paths contain no `throw`, `try`, or
`catch`; the checked contract path remains a deliberate fatal path for
violated internal invariants.

The focused Clang 19 ASan+UBSan matrix built and passed the three independent
Dense runtime tests:

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-portability-clang-asan-ubsan.tXgjRs \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build \
  /tmp/asc-cpp-m3-portability-clang-asan-ubsan.tXgjRs --parallel 2 \
  --target asc_dense_layout_view_test asc_dense_array_evaluate_test \
    asc_dense_linalg_test
ctest --test-dir \
  /tmp/asc-cpp-m3-portability-clang-asan-ubsan.tXgjRs \
  --output-on-failure \
  -R '^asc_cpp\.dense\.(layout_view_test|array_evaluate_test|linalg_test)$'
```

Result: **pass, 3/3**, with zero sanitizer diagnostics. The verifier's final
broader focused matrix additionally passed the multi-TU and benchmark tests,
**5/5**.

Standalone LSan first demonstrated the expected strong-symbol collision when
linking `allocation_probe.cc`. The compatible subset was then run explicitly:

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-portability-clang-lsan.otTN6S \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_LEAK_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m3-portability-clang-lsan.otTN6S \
  --parallel 2 --target asc_dense_m3_header_dense \
    asc_dense_m3_header_array asc_dense_m3_header_evaluate \
    asc_dense_m3_header_linalg asc_dense_multi_tu
ctest --test-dir /tmp/asc-cpp-m3-portability-clang-lsan.otTN6S \
  --output-on-failure \
  -R '^asc_cpp\.(compile\.m3_header_(dense|array|evaluate|linalg)|dense\.multi_tu)$'
```

Result: **pass, 5/5**, with zero LSan diagnostics. Allocation-probe targets are
**skipped as incompatible** under standalone LSan.

The same compatible subset was configured, compiled, linked, and run under
standalone TSan:

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-portability-clang-tsan.EZiKOp \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_THREAD_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m3-portability-clang-tsan.EZiKOp \
  --parallel 2 --target asc_dense_m3_header_dense \
    asc_dense_m3_header_array asc_dense_m3_header_evaluate \
    asc_dense_m3_header_linalg asc_dense_multi_tu
ctest --test-dir /tmp/asc-cpp-m3-portability-clang-tsan.EZiKOp \
  --output-on-failure \
  -R '^asc_cpp\.(compile\.m3_header_(dense|array|evaluate|linalg)|dense\.multi_tu)$'
```

Result: **pass, 5/5**, with zero TSan diagnostics.

## CPU provider and performance evidence

The only Milestone 3 execution path is provider-free serial host C++20. It is
runtime-tested for layout/view/owner behavior, expression evaluation,
reductions, every approved linear-algebra operation, transpose combinations,
non-finite and extreme values, alias and memory failures, and allocation
freedom. Dense production and its target contain no CUDA, HIP, SYCL, OpenMP,
TBB, Eigen, BLAS/LAPACK, oneMKL, provider, SDK, or third-party reference.

The final Clang 19 Release/shared threshold-free benchmark reported:

```text
compiler=clang configuration=release-like operation=evaluate scalar=double shape=32x32 iterations=64 total_ns=4867777 per_iteration_ns=76059 checksum=1283 allocations=0
compiler=clang configuration=release-like operation=gemm scalar=double shape=32x32 iterations=4 total_ns=66532 per_iteration_ns=16633 checksum=20790.7 allocations=0
```

The final GCC 11.4 Debug/static observation reported:

```text
compiler=gcc configuration=debug-like operation=evaluate scalar=double shape=32x32 iterations=64 total_ns=155124939 per_iteration_ns=2.42383e+06 checksum=1283 allocations=0
compiler=gcc configuration=debug-like operation=gemm scalar=double shape=32x32 iterations=4 total_ns=13375893 per_iteration_ns=3.34397e+06 checksum=20790.7 allocations=0
```

These timings are observations from an uncontrolled local host, not speed
thresholds or optimized-provider claims. Warm-up completed before timing,
operands were preallocated, checksums were nonzero, and both timed loops
recorded zero allocation.

GPU evidence: **skipped**.

Milestone 3 has no approved GPU target, source, dependency, provider,
configuration, compile, runtime, or CPU/GPU parity work.

## Exact independent build and package results

```sh
cmake -S . -B /tmp/asc-cpp-m3-portability-gcc-make.n7S9bz \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m3-portability-gcc-make.n7S9bz --parallel 2
ctest --test-dir /tmp/asc-cpp-m3-portability-gcc-make.n7S9bz \
  --output-on-failure -L milestone-3
```

Result: configure/build **pass** with GCC 11.4 Debug/static and strict
warnings; Milestone 3 tests **pass, 42/42**. The final five Dense targets were
rebuilt after all corrections and passed **5/5**.

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-portability-clang-shared.AzdFSx \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m3-portability-clang-shared.AzdFSx \
  --parallel 2
ctest --test-dir \
  /tmp/asc-cpp-m3-portability-clang-shared.AzdFSx \
  --output-on-failure -L milestone-3
```

Result: configure/build **pass** with Clang 19 Release/shared and strict
warnings; Milestone 3 tests **pass, 42/42**. The final five Dense targets were
rebuilt after all corrections and passed **5/5**.

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake -S . \
  -B /tmp/asc-cpp-m3-portability-cmake325.FOwQ6T \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake --build \
  /tmp/asc-cpp-m3-portability-cmake325.FOwQ6T \
  --parallel 2 --target asc_dense
```

Result: **pass**, CMake 3.25.0 with GCC 11.4 Release/static.

```sh
cmake --install \
  /tmp/asc-cpp-m3-portability-clang-shared.AzdFSx \
  --prefix "/tmp/asc cpp m3 portability installed"
readelf -d \
  "/tmp/asc cpp m3 portability installed/lib/libasc_dense.so" |
  grep -E 'NEEDED|RPATH|RUNPATH'
nm -D --defined-only --demangle \
  /tmp/asc-cpp-m3-portability-clang-shared.AzdFSx/src/dense/libasc_dense.so
```

Result: install **pass** at a path containing spaces; the Dense library has
`NEEDED libasc_core.so`, `RUNPATH [$ORIGIN]`, and exactly the approved 20
public overload symbols.

```sh
clang-format-19 --dry-run --Werror \
  include/asc/expression/expression.h include/asc/dense.h \
  include/asc/dense/*.h src/dense/linalg.cc tests/dense/*.cc \
  tests/dense/*.h benchmarks/dense/dense_benchmark.cc \
  tests/consumer/dense/main.cc tests/compile/m3_*.cc \
  tests/compile/m3_*.h
git diff --check -- include/asc/expression/expression.h \
  include/asc/dense.h include/asc/dense src/dense tests/dense \
  tests/compile tests/consumer/dense benchmarks/dense \
  docs/modules/dense.md docs/modules/expression.md \
  docs/development/asc-cpp-m3-dense-cpu
cmake -DSOURCE_DIR:PATH=/home/yicai/AI4SciComp/asc-cpp \
  -P tests/compile/m3_dependency_check.cmake
```

Result: formatting, whitespace, and exact Dense dependency/inventory checks
**pass**.

The first local Ninja configure attempt failed before product configuration
because Ninja is not installed. Repeating the same review with Unix Makefiles
passed; this is not a product failure.

## Residual risks and disposition

- Native MSVC and AppleClang execution is unavailable locally. Their source
  paths are reviewed, and the required hosted Windows shared Debug/Release
  and macOS AppleClang jobs remain publication gates.
- `uintptr_t` interval aliasing is appropriate for the approved hosted
  platforms but is not a claim for capability or segmented-pointer systems.
- Externally constructed raw-pointer views depend on caller-supplied capacity,
  alignment, lifetime, and synchronization correctness.
- Standalone LSan cannot link tests that deliberately replace global
  allocation functions; the compatible LSan subset and full ASan+UBSan Dense
  suite pass.
- The serial loops are deterministic reference implementations, not
  vectorized, parallel, correctly-rounded, overflow-free, or performance
  stable promises. Benchmark timing has no pass/fail speed threshold.
- No GPU support is present or implied; GPU evidence is **skipped**.

Independent disposition: approve Milestone 3 for Publication Checkpoint B,
subject to the lead's final complete clean validation matrix and hosted CI at
publication time.
