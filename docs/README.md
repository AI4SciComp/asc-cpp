# ASCCpp documentation

Start with:

- [Getting started](getting-started.md)
- [Installation and immutable dependencies](installation.md)
- [API main page](api/mainpage.md) and [API conventions](api/conventions.md)
- [Support matrix](support-matrix.md)
- [Package capabilities](package-capabilities.md)
- [Release process](release-process.md)

## Architecture and contracts

- [Six-module architecture](architecture/overview.md)
- [Dependency policy](architecture/dependency-policy.md)
- [Accepted decisions](architecture/decisions/)
- [Dependency, capability, BLAS, and Random contracts](contracts/)
- [API compatibility](api-compatibility.md)
- [Extension guide](extension-guide.md)

## Modules

- [Core](modules/core.md)
- [Utilities](modules/utilities.md)
- [Expression](modules/expression.md)
- [Dense and Dense BLAS](modules/dense.md)
- [Sparse and Sparse BLAS](modules/sparse.md)
- [Random](modules/random.md)

## Integration and scientific evidence

- [Installed/downstream integration](downstream-integration.md)
- [BLAS coverage](blas-coverage.md)
- [Random crosswalk](random-crosswalk.md)
- [Performance methodology](performance.md)
- [`linalg` to `blas` migration](migration/linalg-to-blas.md)
- [Primary scientific references](references.md)
- [MdeCpp disposition/provenance](provenance/)

Temporary development checkpoints and review traces are retained by the
annotated snapshot tag `snapshot/v0.9.0-candidate-20260802`, not by the release
tree.
