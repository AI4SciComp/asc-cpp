# Higher-layer handoff

> [!WARNING]
> **Superseded historical document.** The body below records the deleted
> five-component implementation at commit
> `33b261ea33616a6395c4ad3b20646093103344f7`. It is retained only for audit
> history and does not describe the active API or package. See the
> [current documentation][current-docs] and the
> [approved Stage A architecture][stage-a].

[current-docs]: ../README.md
[stage-a]: ../development/asc-cpp-architecture/architecture-blueprint.md

asc-cpp contains only the reusable numerical foundation selected from MdeCpp.
The remaining MdeCpp domains belong above it:

```text
asc-cmake -> asc-cpp -> asc-xde -> asc-kinetic -> asc-lab
```

| MdeCpp area | Intended owner | Foundation it may consume |
| --- | --- | --- |
| `functional`, `geometry`, `fem`, `integrate`, `mesh`, `odeint` | `asc-xde` | arrays, linalg, random, utilities |
| `simulate`, kinetic-specific nonlinear functions, VPFP/plasma support | `asc-kinetic` | asc-xde and asc-cpp |
| `analyze`, `visualize`, experiment databases, application workflows | `asc-lab` | asc-kinetic and lower layers |
| benchmarks, examples, old CMake/config, `develop` experiments | deferred or rewritten in the owning repository | stable exported targets only |

The boundary prevents domain concepts from leaking back into the foundational
array and execution APIs. Reuse of source from the remaining MdeCpp areas should
be recorded explicitly in the destination repository, just as asc-cpp records
its selected source in [inventory.md](inventory.md).
