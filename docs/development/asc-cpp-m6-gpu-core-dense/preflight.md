# Milestone 6 Read-Only Preflight

Status: Complete before branch creation and production work

Date: 2026-07-26

## Repository identity and retained state

```text
asc-cpp:
  path: /home/yicai/AI4SciComp/asc-cpp
  remote: git@github.com:AI4SciComp/asc-cpp.git
  predecessor branch: feature/asc-cpp-m5-random-storage-generation
  baseline/HEAD: 33b261ea33616a6395c4ad3b20646093103344f7
  worktrees: one
  tags: none
  state: cumulative uncommitted restart through M5 plus owner deletions

approved M6 branch:
  feature/asc-cpp-m6-gpu-core-dense
```

The cumulative dirty state is intentional and non-overlapping with the
bounded M6 ownership ledger. It is preserved in place. No applicable
`AGENTS.md` remains in the retained restart tree.

```text
asc-cmake:
  path: /home/yicai/AI4SciComp/asc-cmake
  remote: git@github.com:AI4SciComp/asc-cmake.git
  branch: main
  state: clean
  commit: 8a7dcbad3a97267cce59810aff24de800a3497a7
  tag: v0.1.0
  annotated tag object: 620b2e912ac5bac7561e09529a65cce965ebc920

MdeCpp:
  path: /home/yicai/repo/MdeRepo/MdeCpp
  remote: git@github.com:escapetiger/MdeCpp.git
  branch: main
  commit: f6294e9079262682ce63ae7ff2d8a643e658bf5d
  unrelated retained change: Makefile
```

## Toolchain

```text
host: Linux x86_64 under WSL2
CMake: 4.1.2
minimum-test CMake: 3.25.0
GCC: 11.4.0
Clang: 19.0.0
clang-format: 19.0.0
Ninja: unavailable
clang-tidy: unavailable
```

## CUDA inventory

```text
CUDA compiler/toolkit: 12.9.86
driver: 576.83
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
memory: 6144 MiB
compute capability: 8.6
runtime library: visible
cuBLAS library: visible
cuSOLVER library: visible but excluded from M6
Compute Sanitizer: 2025.2.1.0
```

Inventory alone is not M6 provider evidence. Configure, provider compile,
real-hardware runtime, and parity must be produced by the exact M6 targets and
tests.

## Current predecessor capability

Milestone 5 has a locally accepted Publication Checkpoint B candidate:

- all six provider-free modules, both random facets, and `ASC::cpp` exist;
- static/shared CMake 3.25/4.1, GCC/Clang, sanitizer, package, relocation,
  subproject, and isolated-consumer matrices pass;
- CUDA is not discovered or exported;
- GPU evidence is exactly skipped; and
- no unresolved M5 release blocker remains.

The current base core already has provider-neutral memory spaces, devices,
execution vocabulary, host resources/buffers, serial contexts/events, and
`CopyBytes`, but CUDA creation is unavailable. Dense has one-space views,
move-only host owners, serial evaluation, and serial float/double algebra.

## Exact ASCCMake use

Only these released functions remain approved:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

ASCCMake has no CUDA helper, provider discovery helper, or conditional
multi-component export helper. Standard target-oriented CMake owns CUDA
language enablement, `FindCUDAToolkit`, target definitions, and conditional
component exports. No ASCCMake API will be invented.

## License and provenance

```text
asc-cpp LICENSE SHA-256:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4

asc-cmake LICENSE SHA-256:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4

MdeCpp LICENSE SHA-256:
230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809
```

M6 is clean-room from the accepted ADRs and official CMake/NVIDIA
documentation. MdeCpp and deleted asc-cpp CUDA source/tests are not inspected.
