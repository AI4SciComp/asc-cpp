# Milestone 1 dependency, capability, license, and provenance audit

Status: Publication Checkpoint B candidate

Evidence date: 2026-07-27

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
Standard CMake implements the approved conditional component export because
released ASCCMake has no multi-component conditional-export API.

The live package makes only `core` available. `utilities`, `expression`,
`dense`, `sparse`, `random`, their storage facets, and `cpp` remain known but
unavailable. Required unavailable or unknown components fail. An optional
unavailable component reports false without invalidating a required `core`
request. A no-component lookup requests the unavailable required `cpp`
aggregate and fails.

## Source and include closure

The frozen inventory contains exactly 11 public headers and six compiled
`.cc` sources. Configure-time and CTest audits compare the complete observed
inventory and target topology with the contract, inspect direct link
properties, scan production includes, and reject:

- a `utilities`, `expression`, `dense`, `sparse`, `random`, historical
  `array`/`linalg`, or provider dependency;
- a CUDA, cuBLAS, cuSOLVER, cuSPARSE, cuRAND, HIP/ROCm, SYCL, or MKL SDK
  include;
- `#pragma once`, public exception syntax, and forbidden internal namespace
  spellings;
- an unapproved production target or file; and
- a direct or exported `asc_core` link edge.

Every production include is either a C++20 standard-library header or another
`asc/core` header. Each public header is compiled as the first ASC include.
GNU and GNU-style Clang frontends repeat the public-header checks with
exceptions disabled. The complete six-source library, installation, and an
isolated consumer also compile with GCC exceptions disabled.

## Capability and backend closure

Implemented capabilities are limited to:

- checked logical metadata and mixed extents;
- status, result, and release-active contracts;
- storage-independent configuration values and schema validation;
- portable byte/text I/O;
- host memory resources and move-only raw buffers; and
- serial CPU context, overlap-safe host copy, and completed events.

The capability manifest marks those six M1 entries `runtime-tested` and every
later capability `proposed`; the architecture test enforces that split.

No optimized CPU provider or GPU/provider target exists. Milestone 1 GPU
evidence is **skipped**. Toolkit or hardware inventory is not relabeled as
ASCCpp configure-tested, compile-tested, runtime-tested, or parity-tested
evidence.

## License and provenance

ASCCpp remains Apache-2.0 and installs its root `LICENSE`. ASCCMake 0.1.0 is
Apache-2.0. MdeCpp remains a separate GPLv3 comparison repository at verified
commit `f6294e9079262682ce63ae7ff2d8a643e658bf5d`; it supplied behavioral and
test-category evidence only.

Case-insensitive scans of Milestone 1 production and verification files found
no MdeCpp author/repository marker, GPL notice, numerical corpus, or provider
SDK include. Production and verification separately attest that no MdeCpp
source, test, literal corpus, generated data, or mechanical translation was
copied.

## Validation evidence

- minimum CMake 3.25.0, GCC 11.4, Release static with warnings-as-errors and
  paths containing spaces: 44/44 tests passed;
- current CMake 4.1.2, GCC 11.4, Debug static and Release shared with
  warnings-as-errors: 44/44 tests passed in each independent matrix;
- current CMake 4.1.2, GCC 11.4, Debug static with ASan/UBSan: the focused
  instrumented architecture/compile/core matrix passed 38/38, separately from
  non-instrumented package consumers;
- GCC 11.4 and Clang 19.0: all 11 headers and six sources compiled under
  C++20, warnings-as-errors, and exceptions disabled;
- GCC 11.4 Release whole-library exceptions-disabled build, install, and
  isolated installed consumer: passed; and
- static/shared build-tree, installed, relocated, subproject, optional
  component, and path-with-spaces consumers passed without a package-registry
  write.

The exact final commands, hosted-platform skips, and separate portability
review are reported at Publication Checkpoint B. Hosted MSVC and AppleClang
results remain pending and are not claimed as local evidence.
