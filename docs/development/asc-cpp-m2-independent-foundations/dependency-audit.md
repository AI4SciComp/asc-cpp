# Milestone 2 dependency audit

Status: Pass for the local Publication Checkpoint B candidate

Date: 2026-07-26

## Audited graph

```text
ASC::core        -> []
ASC::utilities   -> [ASC::core]
ASC::expression  -> [ASC::core]
ASC::random      -> [ASC::core]
```

The build-tree targets are `asc_core`, `asc_utilities`, `asc_expression`, and
`asc_random`. Core, utilities, and random are compiled libraries that follow
`BUILD_SHARED_LIBS`; expression is an interface library. Dense, sparse,
`random_dense`, `random_sparse`, `cpp`, and every provider target are absent.

Utilities, expression, and random have no sibling edge. Expression has no
storage edge. Random base has no storage edge. No Milestone 2 production
header or source includes a provider, dense, sparse, or retired array/linalg
header.

## Mechanical enforcement

The configure-time target-property audit requires:

- no direct or interface dependency for `asc_core`;
- exactly `ASC::core` as the direct and interface dependency of the three new
  targets;
- an interface target for expression and compiled static/shared targets for
  utilities and random; and
- exactly the four approved build targets and aliases.

The file/include audit requires the exact frozen public-header and compiled
source inventories, scans each module for forbidden sibling/provider includes,
and rejects retired production paths. Every public header is compiled alone
under C++20 and, with GCC/Clang, with exceptions disabled.

The installed-package audit requires separate
`ASCCpp{Core,Utilities,Expression,Random}Targets.cmake` exports. A requested
new component loads core first and does not import either sibling. Unknown,
unavailable, optional, required, and no-component requests are exercised.
Utilities, expression, and random each configure, build, run, install,
relocate through a path containing spaces, and run again as isolated
consumers.

## Local evidence

```text
ctest --test-dir /tmp/asc-cpp-m2-validation.ckoFuy/gcc-debug \
  -L architecture --output-on-failure
```

Result: pass, 4/4. This covers the manifest graph, approved product target
inventory, exact public/source file policy, and forbidden-include/direct-link
audit.

The full minimum-toolchain GCC Release static and Clang Debug shared matrices
both passed 82/82 tests, including all nine consumer cases and the
comprehensive build-tree/install/relocation component checks. The Clang
ASan/UBSan matrix passed all 70 eligible non-package/non-consumer tests.

## Conclusion

The source, build-tree, installed-package, and isolated-consumer graphs match
the frozen six-module ceilings through Milestone 2. No unapproved dependency,
provider discovery, storage coupling, aggregate target, or later-milestone
component was introduced.
