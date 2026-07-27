# asc-cpp restart repository audit

Status: Stage A evidence, awaiting Architecture Checkpoint A approval

Audit date: 2026-07-26 (Asia/Shanghai)

Runbook SHA-256:
`a08b70d6df1fa639aca481a63072caeb42b46e1e7a2c767d3d737bb26f241647`

## Evidence boundary

This audit describes the current worktrees. Deleted asc-cpp files inspected
with `git show HEAD:<path>` are historical evidence only. They were not
restored. No credential-bearing URL or secret was recorded.

## Repository identities

| Repository | Root | Remote | Branch / HEAD | Worktrees | Tags | Dirty state |
| --- | --- | --- | --- | --- | --- | --- |
| asc-cpp | `/home/yicai/AI4SciComp/asc-cpp` | `git@github.com:AI4SciComp/asc-cpp.git` | `main`, `33b261ea33616a6395c4ad3b20646093103344f7`, equal to `origin/main` | one | none | 208 intentional tracked deletions plus this untracked Stage A package |
| asc-cmake | `/home/yicai/AI4SciComp/asc-cmake` | `git@github.com:AI4SciComp/asc-cmake.git` | `main`, `8a7dcbad3a97267cce59810aff24de800a3497a7`, equal to `origin/main` | one | annotated `v0.1.0`, tag object `620b2e912ac5bac7561e09529a65cce965ebc920` | clean |
| MdeCpp | `/home/yicai/repo/MdeRepo/MdeCpp` | `git@github.com:escapetiger/MdeCpp.git` | `main`, `f6294e9079262682ce63ae7ff2d8a643e658bf5d`, equal to `origin/main` | one | none | unrelated modified `Makefile` |

asc-cpp and asc-cmake are private GitHub repositories. MdeCpp is public.
GitHub reported no open issue or pull request in any of the three repositories.
MdeCpp also has `develop` at
`1a3e81962b424ababf1cbaca9acfdf4f7e59c67c`.

## Applicable instructions

- The owner instruction and the all-in-one runbook are authoritative.
- asc-cpp's tracked `AGENTS.md` and `generator.md` are intentionally deleted.
  They are not live instructions and were not restored.
- asc-cmake's `generator.md` was read completely. It labels itself a
  historical implementation prompt and is superseded by released code,
  documentation, ADRs, and `v0.1.0`.
- asc-cmake `CONTRIBUTING.md` was read completely.
- MdeCpp's tracked `CLAUDE.md` was read completely. Its rules apply to the
  MdeCpp inspection. The modified MdeCpp `Makefile` was preserved.

## asc-cpp retained state

The baseline commit tracks 236 files. The current worktree deletes 208:

- root build, preset, contributor, lint, changelog, generator, and notice
  files;
- all 83 public headers;
- all 32 source/build files below `src`;
- all 78 tests;
- examples, generated configuration, and package templates.

The retained tracked files are `.gitignore`, `LICENSE`, `README.md`,
`.github/workflows/ci.yml`, and 24 documents. There is no live
`CMakeLists.txt`, `include`, `src`, `tests`, package config, example, or
production API. The repository therefore cannot currently configure, build,
test, install, or provide a package.

The retained documents describe a deleted five-component implementation
(`core`, `utilities`, `array`, `linalg`, `random`). Claims that features are
implemented or that tests pass are historical. They conflict with the current
six-module contract where they:

- combine expression, dense, and sparse under `array`;
- place dense and sparse algebra under `linalg`;
- make random depend on array storage;
- use `asc::detail`;
- expose optional provider requirements too broadly.

They remain useful for behavior cataloguing and migration history. Milestone 0
must mark, archive, replace, or remove their stale operational claims.

## Retained work worth re-specifying

Historical HEAD contains useful ideas, not current code:

- signed 64-bit index/extent/stride/NNZ vocabulary and checked arithmetic;
- status/result transport and release-active public preconditions;
- move-only, single-space RAII buffers;
- explicit execution contexts and events;
- const-correct non-owning dense views and mixed extents;
- serial reference BLAS kernels and allocation/alias tests;
- explicit Philox key/counter state and deterministic logical traversal;
- transactional configuration and command-line behavior;
- monotonic timing behavior;
- self-contained header, ODR, dependency, relocation, and isolated-consumer
  tests.

No compatibility commitment to the deleted five-component API is inferred.

## Current defects and blockers

| Severity | Finding | Consequence |
| --- | --- | --- |
| P0 | No live build, source, or tests | No current asc-cpp capability can be claimed or validated |
| P0 | Retained docs and CI describe deleted files and five components | Operational documentation is false and architecture is superseded |
| P0 | MdeCpp is GPLv3 under current evidence; asc-cpp is Apache-2.0 | MdeCpp production code and tests cannot be copied into Apache-only asc-cpp |
| P0 | Deleted `THIRD_PARTY_NOTICES` and unresolved Sobol lineage | No historical random table/source import is authorized |
| P0 | Expression neutrality, mixed storage, packaging, memory/execution, and error contracts require ADRs | Production work must wait for Checkpoint A approval |
| P1 | Historical CI cannot authenticate its private asc-cmake clone | Every build job failed before asc-cpp configure |
| P1 | Historical cpplint job reported 187 errors | Deleted implementation did not meet its own lint gate |
| P1 | Current host lacks Ninja, Clang, clang-format, clang-tidy, cppcheck, and Doxygen in `PATH` | Full local portability/static-analysis validation is unavailable |
| P1 | MdeCpp has an unrelated dirty `Makefile`, ignored MdeMat inputs, and a broken absolute symlink | Those artifacts cannot be reproducible provenance |
| P2 | Current CI is Linux-only and uses mutable `actions/checkout@v4` | It does not meet release portability or supply-chain policy |

The last asc-cpp GitHub Actions run,
[30061662950](https://github.com/AI4SciComp/asc-cpp/actions/runs/30061662950),
ran against historical HEAD. Its build jobs failed while cloning private
asc-cmake over unauthenticated HTTPS. The separate cpplint job reported 187
errors. Neither result tests the current gutted worktree.

## License and provenance facts

- asc-cpp retains Apache License 2.0. Its `LICENSE` SHA-256 is
  `c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4`.
- asc-cmake is Apache-2.0 and distributes its license.
- MdeCpp's tracked `LICENSE` is GPL version 3, SHA-256
  `230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809`.
  The conservative metadata is `GPL-3.0-only` until its owner clarifies.
- MdeCpp tests are covered by the same repository evidence and are behavioral
  prompts only.
- MdeCpp Sobol source and tables, ignored MdeMat material, and generated
  Lebedev data are blocked as described in `provenance-review.md`.

## Toolchain and provider inventory

Host:

- WSL2 Linux x86_64, 16 logical CPUs;
- Intel Core i7-11800H, eight cores/two threads per core;
- Git 2.34.1, CMake/CTest 4.1.2, GNU Make 4.3;
- GCC/G++ 11.4.0;
- Python 3.12.4 and cpplint 1.6.1.

Detected provider material:

- GCC OpenMP 4.5 (`_OPENMP=201511`);
- Eigen 3.4.0;
- CMake finds BLAS/LAPACK through oneAPI MKL 2024.2;
- oneAPI DPC++/C++ compiler 2024.2, MKL 2024.2, and TBB;
- CUDA compiler/toolkit 12.9.86;
- CUDA runtime, cuBLAS, cuSOLVER, cuSPARSE, and cuRAND libraries;
- NVIDIA GeForce RTX 3060 Laptop GPU, 6 GiB, driver 576.83;
- oneAPI SYCL reports an OpenCL CPU device;
- no HIP compiler or ROCm runtime.

A temporary external CMake probe configured CXX and CUDA languages and found
OpenMP, Eigen3, BLAS, LAPACK, CUDAToolkit, MKL, and TBB. This proves discovery
and a CUDA compiler ABI smoke, not an asc-cpp provider or numerical runtime.

## Preflight conclusion

Repository identities are correct and unrelated work is preservable. Stage A
may proceed. Production implementation remains blocked until the owner accepts
the architecture package. The 208 deletions and MdeCpp `Makefile` modification
must remain untouched.
