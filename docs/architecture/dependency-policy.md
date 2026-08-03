# ASCCpp dependency policy

Status: enforced architecture policy
Architecture: exactly six modules (`core`, `utilities`, `expression`, `dense`,
`sparse`, and `random`)

## Approved provider-free graph

| Target | Installed target | Direct ASC dependencies |
| --- | --- | --- |
| `asc_core` | `ASC::core` | none |
| `asc_utilities` | `ASC::utilities` | `ASC::core` |
| `asc_expression` | `ASC::expression` | `ASC::core` |
| `asc_dense` | `ASC::dense` | `ASC::core`, `ASC::expression` |
| `asc_sparse` | `ASC::sparse` | `ASC::core`, `ASC::expression` |
| `asc_random` | `ASC::random` | `ASC::core` |

`random_dense` and `random_sparse` are facets, not modules.
`ASC::random_dense` depends directly on `ASC::random` and `ASC::dense`;
`ASC::random_sparse` depends directly on `ASC::random` and `ASC::sparse`.
The `ASC::cpp` aggregate depends on every provider-free module and Random
storage facet. It does not contain a provider facet.

## Hard ceilings

- `core` depends on no ASC module.
- `utilities`, `dense`, and `sparse` are mutually independent.
- `expression` has no dependency on storage, utilities, random, or a provider.
- `dense` and `sparse` neither include nor link one another.
- `random` base has no storage dependency. Storage generation is owned by its
  separate dense and sparse facets.
- Provider SDK headers, compile definitions, libraries, and discovery remain
  in separately requested provider facets.
- A public include edge requires a matching direct target dependency. A target
  must not rely on a transitive include or link edge.

No top-level `array`, `linalg`, or generic backend module may be introduced.
Dense and sparse own their respective storage, expression evaluation, and
CPU/GPU linear algebra.

## Core enforcement

Core exposes only `asc_core` / `ASC::core`. Its direct ASC and external
dependency sets are empty. The public and compiled file sets are enforced by
the architecture and public-file policy tests.

The architecture tests:

- compare the live component graph with the dependency manifest;
- require only the approved Core product target;
- compare public headers and compiled sources with the frozen file set;
- reject provider headers and retired module paths;
- compile every public header independently and with exceptions disabled;
- verify the build-tree, installed, and relocated `core` package consumer.

Every later capability change must update the manifest, capability evidence,
component package, dependency audit, and negative consumer tests in the same
change that adds an approved edge.
