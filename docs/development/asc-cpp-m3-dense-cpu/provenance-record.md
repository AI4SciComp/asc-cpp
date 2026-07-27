# Milestone 3 Provenance Record

Status: Frozen clean-room boundary

Date: 2026-07-26

## Project ownership

Milestone 3 production source, tests, examples, benchmarks, and documentation
are newly project-owned work derived from the owner-approved architecture
contracts and standard mathematical definitions.

No source, test, table, vector, prose, or mechanical translation was copied
from:

- MdeCpp;
- the user-deleted asc-cpp implementation;
- an external BLAS/LAPACK implementation;
- an optimized provider SDK;
- another dense-array library.

The production and verification agents were explicitly prohibited from
inspecting MdeCpp or deleted/upstream implementation and test material.
MdeCpp remains historical comparison/provenance evidence only.

## Bound repositories

```text
asc-cpp
remote: git@github.com:AI4SciComp/asc-cpp.git
baseline: 33b261ea33616a6395c4ad3b20646093103344f7

asc-cmake
remote: git@github.com:AI4SciComp/asc-cmake.git
commit: 8a7dcbad3a97267cce59810aff24de800a3497a7
annotated tag object: 620b2e912ac5bac7561e09529a65cce965ebc920
tag: v0.1.0

MdeCpp comparison checkout
path: /home/yicai/repo/MdeRepo/MdeCpp
remote: git@github.com:escapetiger/MdeCpp.git
commit: f6294e9079262682ce63ae7ff2d8a643e658bf5d
retained unrelated modification: Makefile
```

## Mathematical sources

The M3 reference algorithms use elementary loop definitions:

- copy and scale;
- `y = alpha * x + y`;
- deterministic inner products;
- scaled sum-of-squares Euclidean norm;
- matrix-vector and matrix-matrix multiplication.

No literal numerical corpus is required. Verification derives independent
small cases analytically and uses higher-precision or separately structured
oracles where appropriate.

## Dependencies and providers

No third-party dependency or provider adapter is introduced. Locally detected
BLAS/LAPACK and CUDA libraries are inventory only and are not included, linked,
configured, invoked, or claimed as Milestone 3 evidence.

## License

The existing asc-cpp Apache-2.0 `LICENSE` is unchanged:

```text
SHA-256:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4
```

It matches the exact asc-cmake release repository license. MdeCpp's separate
license hash is:

```text
230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809
```

That distinct provenance is one reason MdeCpp implementation/test material is
excluded from this milestone.
