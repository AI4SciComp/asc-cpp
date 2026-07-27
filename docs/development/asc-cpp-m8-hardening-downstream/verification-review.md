# Milestone 8 Independent Verification Review

Status: Final integrated verification pass; accepted for Publication Checkpoint B

Date: 2026-07-27

## Focused results

The independent verification fixtures were exercised against a fresh GCC
11.4 Release static CPU build and installation of the unreleased 0.9.0
candidate under `/tmp/asc-cpp-m8-verification.unw7mD`.

| Evidence | Result |
| --- | --- |
| fresh CPU configure/build/install | pass |
| exact installed provider-free header manifest | pass, 37/37 |
| installed aggregate API compile/link/run | pass |
| version `0.9.0 EXACT` | pass |
| version `0.9` compatible request | pass |
| version `0.9.1` newer candidate request | rejected as expected |
| version `0.8` incompatible minor | rejected as expected |
| version `1.0` incompatible major | rejected as expected |
| build-tree asc-xde trial | pass |
| installed path-with-spaces asc-xde trial | pass |
| real asc-xde repository before/after | unchanged, clean approved commit |
| GCC repeated compile/object probe | pass, 1.15452/1.16441 s, 1864 bytes |
| Clang 19 repeated compile/object probe | pass, 1.44920/1.49949/1.82572 s, 1144 bytes |
| repository formatting/diff whitespace for owned files | pass |

The installed runtime observations from GCC 11.4 were:

```text
asc::Status                         size 80, alignment 8
asc::ExecutionContext               size 32, alignment 8
asc::DenseView<double,2>            size 104, alignment 8
asc::CoordinateView<double,2>       size 88, alignment 8
asc::CsrView<double>                size 96, alignment 8
asc::Philox4x32Counter              size 16, alignment 4
```

These are local pre-1.0 ABI observations, not compatibility promises.

## Final integrated follow-up

The final follow-up inspected the lead-owned hardening/downstream
registrations, the generated package closure algorithm, the repeated-component
consumer, and the portability review's independent CPU/CUDA reruns.

`ASCCppConfig.cmake` has no one-shot include guard. Each lookup computes the
transitive closure of only the requested available ASC components from the
generated dependency metadata, discovers CUDAToolkit only when that closure
contains a CUDA component, includes only the needed ASC export files, and
cleans its temporary variables. This supports cumulative component requests in
one downstream CMake process without importing unrelated ASC targets.

The repeated-component fixture first requests `core`, then `dense` or
`dense_cuda`, and finally `utilities`. It checks the exact ASC target closure
and exact imported link interfaces after each applicable lookup. For static
CUDA packages it also checks the precise `LINK_ONLY` Runtime and cuBLAS edges.
It correctly does not reject unrelated CUDA targets merely because
CMake's `FindCUDAToolkit` defines them: target existence is not an ASCCpp link
edge.

A fresh independent CPU rerun used:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m8-verification-final-cpu.vxGd2P/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m8-verification-final-cpu.vxGd2P/build \
  --target asc_core asc_utilities asc_dense --parallel 4
ctest \
  --test-dir /tmp/asc-cpp-m8-verification-final-cpu.vxGd2P/build \
  --output-on-failure --no-tests=error \
  -R '^asc_cpp\.package\.repeated_component_lookup(_build)?$'
```

Result: 2/2 pass in 2.78 seconds. The nested consumer configured, compiled,
and linked.

The portability reviewer independently recorded the corrected CUDA
repeated-lookup configure/build result as 2/2 pass in 21.37 seconds. That
review also records the integrated CPU hardening/downstream slice at 26/26 and
the CUDA hardening slice at 22/22, including 49 exact installed CUDA-enabled
headers and the installed API consumer with all six CUDA facets.

## Findings

### VER-M8-001 — full integration is lead-owned

Disposition: resolved.

The lead registered the metadata/version, source/install surface,
installed-header/API, compile/object, and downstream trials with the required
fixtures and labels. Reviewed independent evidence covers build-tree, copied
build-tree, installed, relocated, path-with-spaces, CPU, CUDA, GCC, Clang,
static, and shared cases.

### VER-M8-002 — timing is noisy

Disposition: accepted and bounded.

The Clang compile observations varied by about 26 percent across three local
runs. No absolute or cross-compiler threshold is justified. The implemented
gate checks correctness and within-command object-size repeatability only.

### VER-M8-003 — CUDA installed-header/API integrated follow-up

Disposition: resolved.

The independent portability rerun exercised the CUDA-aware header manifest and
installed API paths: the CUDA hardening slice passed 22/22, the installed
manifest contained exactly 49 headers, and the public consumer configured,
compiled, linked, and ran with all six CUDA facets. For these package/API
fixtures this is `configure-tested` and `compile-tested` GPU evidence. The
consumer runtime itself does not execute a provider operation and therefore
does not independently upgrade this fixture to `runtime-tested` or
`parity-tested`; those classifications are supplied separately by the
reviewed real-device numerical and bit-oracle tests.

### VER-M8-004 — repeated CUDA lookup initially overconstrained toolkit targets

Disposition: resolved.

The initial repeated CUDA consumer treated the existence of an unrelated
`CUDA::cusparse` target as a dependency leak after requesting `dense_cuda`.
`FindCUDAToolkit` is permitted to define toolkit targets beyond those present
in ASCCpp's imported link interfaces, so that was not a valid dependency
oracle.

The corrected fixture checks exact ASC closures and exact static/shared link
interfaces while requiring only Runtime and cuBLAS for `dense_cuda`.
Independent evidence is CPU 2/2 from this verifier and CUDA 2/2 from the
portability reviewer. The superseded diagnostic remains documented in the
portability review.

## Review conclusion

The independently designed package metadata, installed API/header,
compile/object, and asc-xde trial evidence passes after lead integration. The
final repeated-lookup correction is valid and independently rechecked for CPU
and CUDA. No product C++ defect, unapproved dependency, later-milestone
capability, or open verification blocker remains. Independent verification
accepts the integrated Milestone 8 candidate for Publication Checkpoint B.
