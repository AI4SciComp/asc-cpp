# Milestone 5 Read-Only Preflight

Status: Complete

Date: 2026-07-26

## asc-cpp

```text
path: /home/yicai/AI4SciComp/asc-cpp
remote: git@github.com:AI4SciComp/asc-cpp.git
starting branch: feature/asc-cpp-m4-sparse-cpu
milestone branch: feature/asc-cpp-m5-random-storage-generation
HEAD: 33b261ea33616a6395c4ad3b20646093103344f7
worktrees: one
tags: none
```

The intentionally uncommitted M0--M4 restart, tracked legacy deletions, and
untracked replacement tree are retained. No applicable `AGENTS.md` or
`generator.md` exists in the retained restart tree. No overlapping dirty work
is discarded.

The Milestone 4 full static/shared/minimum-CMake/sanitizer/package/consumer
evidence is preserved in its Publication Checkpoint B. Milestone 5 starts from
that exact cumulative filesystem state without claiming a clean predecessor
commit.

## asc-cmake

```text
path: /home/yicai/AI4SciComp/asc-cmake
remote: git@github.com:AI4SciComp/asc-cmake.git
branch: main
worktree: clean
commit: 8a7dcbad3a97267cce59810aff24de800a3497a7
tag: v0.1.0
annotated tag object: 620b2e912ac5bac7561e09529a65cce965ebc920
tag target: 8a7dcbad3a97267cce59810aff24de800a3497a7
```

The exact release contract in `asc-cmake-consumption.md` was reread. M5 uses
only `asc_target_enable_cxx20`, `asc_target_enable_warnings`,
`asc_target_enable_sanitizers`, and `asc_register_test`. Standard CMake owns
the conditional multi-component exports. No helper is invented.

## MdeCpp

```text
path: /home/yicai/repo/MdeRepo/MdeCpp
remote: git@github.com:escapetiger/MdeCpp.git
branch: main
commit: f6294e9079262682ce63ae7ff2d8a643e658bf5d
retained unrelated work: modified Makefile
```

MdeCpp remains GPL-covered behavior/provenance evidence only. Its sampler,
random, array, sparse, and test sources are not implementation or literal
vector inputs. The modified `Makefile` is not touched.

## Toolchains and providers

```text
CMake: 4.1.2
minimum test CMake: 3.25.0
GCC: 11.4.0
Clang: 19.0.0
clang-format: 19.0.0
clang-tidy: unavailable
Ninja: unavailable
CUDA toolkit: 12.9
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
driver: 576.83
GPU memory: 6144 MiB
```

BLAS/LAPACK, CUDA, cuBLAS, cuSOLVER, cuSPARSE, and cuRAND are inventory only.
No provider target, option, discovery, header, library, compile, or runtime
operation is in M5. GPU evidence is exactly `skipped`.

## License hashes

```text
asc-cpp Apache-2.0:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4

asc-cmake Apache-2.0:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4

MdeCpp distinct license:
230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809
```

## Contract sources read

- complete runbook version 2.0;
- current official Google C++ Style Guide;
- Stage A blueprint, dependency/capability manifests, backend matrix, testing
  strategy, roadmap, and relevant ADRs;
- actual random, dense, sparse, expression, core, package, and ASCCMake APIs;
- random/dense/sparse/expression module contracts;
- Milestone 4 Publication Checkpoint B;
- MdeCpp disposition and provenance decisions relevant to random storage.

No material architecture decision remains unresolved for the bounded M5
contract.
