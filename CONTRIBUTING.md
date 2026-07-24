# Contributing

Read `AGENTS.md` before making changes. Preserve the MdeCpp-derived layering and
public vocabulary documented in [docs/architecture.md](docs/architecture.md).
Do not replace established `MShape`, layout, `MObject`, `DenseMArray`,
`SparseMArray`, memory, or sampler mechanisms with a parallel container model.

Public code uses C++20 without compiler extensions in normal C++ builds,
namespace `asc`, self-contained headers, include guards, and Doxygen comments.
Keep template definitions installed with their declarations. New optional
dependencies must be target-local, exported by the installed package, and off
by default.

Before publishing a change, run:

```bash
cmake --preset strict
cmake --build --preset strict --parallel
ctest --preset strict
cpplint --recursive include/asc src tests examples
```

Changes to layouts, memory ownership, sparse compression, or package exports
also require the relevant backend build and the package-relocation test. Record
source-level migration provenance and third-party notices when importing more
material.
