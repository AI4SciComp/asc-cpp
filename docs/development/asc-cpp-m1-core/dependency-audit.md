# Milestone 1 dependency, capability, license, and provenance audit

Status: Publication Checkpoint B candidate

Evidence date: 2026-07-26

## Target and package closure

```text
asc_core
└── direct ASC dependencies: none
    external link dependencies: none

build-tree alias: ASC::core
installed target: ASC::core
available package component: core
```

`asc_core` is the only production target. Its `LINK_LIBRARIES` and
`INTERFACE_LINK_LIBRARIES` properties are empty. The installed target exports
no transitive link dependency. `ASCCMake 0.1.0` is an exact configure-time
CMake-language dependency, not a linked or installed consumer dependency.
Standard CMake implements the approved conditional component exports because
released ASCCMake has no multi-component conditional-export API.

The live package makes only `core` available. `utilities`, `expression`,
`dense`, `sparse`, `random`, their storage facets, `cpp`, and every provider
facet remain known but unavailable. Required unavailable or unknown
components fail; an optional unavailable component reports false without
invalidating a required `core` request.

## Source and include closure

The frozen inventory contains exactly 11 public headers and six compiled
`.cc` sources. A configure-time and CTest audit compares the complete observed
inventory with that contract, scans every production include, and rejects:

- any `utilities`, `expression`, `dense`, `sparse`, `random`, historical
  `array`/`linalg`, or provider dependency;
- CUDA, cuBLAS, cuSOLVER, cuSPARSE, cuRAND, HIP/ROCm, SYCL, or MKL headers;
- `#pragma once`, public exception syntax, and forbidden internal namespace
  spellings;
- any direct or exported `asc_core` link edge.

Every production include is either a C++20 standard-library header or another
`asc/core` header. Each public header is compiled as the first and only ASC
include. GNU and GNU-style Clang/AppleClang frontends repeat the checks with
exceptions disabled; MSVC and clang-cl do not claim an unsupported
exception-disabled standard-library mode.

## Capability and backend closure

Implemented and runtime-tested capabilities are limited to:

- checked logical metadata and mixed extents;
- status, result, and release-active contracts;
- storage-independent configuration values and schema validation;
- portable byte/text I/O;
- host memory resources and move-only raw buffers;
- serial CPU context, overlap-safe host copy, and completed events.

No optimized CPU provider or GPU/provider target exists. Milestone 1 GPU
evidence is **skipped**. Stage A toolkit and hardware inventory is not relabeled
as ASCCpp configure, compile, runtime, or parity evidence.

## License and provenance

ASCCpp remains Apache-2.0 and installs its root `LICENSE`. ASCCMake 0.1.0 is
Apache-2.0. MdeCpp remains a separate GPLv3 comparison repository at verified
commit `f6294e9079262682ce63ae7ff2d8a643e658bf5d`; it supplied behavioral and
test-category evidence only.

Case-insensitive scans of Milestone 1 production and test files found no
MdeCpp author/repository markers, GPL notice, Sobol/Lebedev corpus, or provider
SDK reference. The production and verification roles separately attest that
no MdeCpp source, test, literal corpus, generated data, or mechanical
translation was copied.

## Validation evidence

- minimum CMake 3.25.0, GCC 11.4, Release static: 44/44 tests passed;
- current CMake 4.1.2, GCC 11.4, Debug shared: 44/44 tests passed;
- current CMake 4.1.2, Clang 19.0, Debug static with ASan/UBSan: instrumented
  in-tree matrix passed 38/38, separately from non-instrumented package
  consumers;
- independent portability run, Clang 19.0, Debug shared: 44/44 tests passed;
- GCC 11.4 Release whole-library `-fno-exceptions`: configure, compile,
  install, and path-with-spaces passed;
- static/shared build-tree, installed, relocated, subproject, and
  path-with-spaces consumers passed with no package-registry write.

Exact command results and any platform skips are reported at Publication
Checkpoint B. Hosted MSVC and AppleClang results remain pending and are not
claimed as local evidence.
