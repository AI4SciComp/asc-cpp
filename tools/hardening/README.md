# ASCCpp hardening tools

These CMake-language tools record and check the Milestone 8 public/package
surface without changing product sources, headers, libraries, or package
metadata.

## Public surface

`CheckPublicSurface.cmake` checks every `include/asc/*.h` file against the
normalized SHA-256 baseline in `abi/public-headers.sha256`. Line endings are
normalized to LF before hashing so a normal CRLF checkout does not create a
false API change. The check also loads the repository's actual
`ASCCppComponents.cmake` and compares every known component and direct
component dependency with `abi/targets-and-components.txt`.

```sh
cmake \
  -DASC_CPP_HARDENING_SOURCE_DIR=/path/to/asc-cpp \
  -DASC_CPP_HARDENING_OUTPUT=/path/out/public-surface.txt \
  -P tools/hardening/CheckPublicSurface.cmake
```

Set `ASC_CPP_HARDENING_INSTALLED_INCLUDE_DIR` to an installed `include`
directory to check that the installed `asc/*.h` tree is byte-equivalent to the
source baseline.

## Configured targets

`TargetInventory.cmake` defines
`asc_cpp_hardening_check_target_inventory()`. It must be called after all
product targets have been created, normally from the M8 hardening-test
subdirectory. It checks target kind, export/output name, C++20 propagation,
static-definition macro, exact direct link set, and public header-file-set
ownership against the committed baselines.

```cmake
include("${PROJECT_SOURCE_DIR}/tools/hardening/TargetInventory.cmake")
asc_cpp_hardening_check_target_inventory(
  BASELINE
    "${PROJECT_SOURCE_DIR}/abi/targets-and-components.txt"
  HEADER_OWNERS
    "${PROJECT_SOURCE_DIR}/abi/header-owners.txt"
  OUTPUT
    "${PROJECT_BINARY_DIR}/hardening/target-inventory.txt"
)
```

The function only reads target properties. Its report must be outside the
source tree.

For an integration-free fresh configure, load
`ProjectTargetInventoryHook.cmake` through CMake's project-include mechanism:

```sh
cmake -S /path/to/asc-cpp -B /path/out/build \
  -DCMAKE_PROJECT_INCLUDE=/path/to/asc-cpp/tools/hardening/ProjectTargetInventoryHook.cmake \
  -DASC_CPP_HARDENING_TARGET_OUTPUT=/path/out/target-inventory.txt
```

The hook defers the check until all top-level product targets exist. It does
not change their properties.

## Shared ELF observations

`InspectElfAbi.cmake` records ELF class, byte order, machine, SONAME, direct
runtime dependencies, and defined dynamic symbols for supplied shared
libraries. Addresses and other build-placement details are omitted.

```sh
cmake \
  '-DASC_CPP_HARDENING_LIBRARIES=/path/libasc_core.so;/path/libasc_dense.so' \
  -DASC_CPP_HARDENING_OUTPUT=/path/out/elf-abi.txt \
  -DASC_CPP_HARDENING_BASELINE=/path/to/a/reviewed-baseline.txt \
  -P tools/hardening/InspectElfAbi.cmake
```

The tool uses `readelf`, `nm`, and `c++filt`. If any is unavailable, or an
input is not ELF, it reports `skipped` instead of inferring a pass. An exact
baseline comparison is performed only when inspection succeeds.

All report destinations are required to be outside the source tree. None of
these tools updates a baseline; reviewed baseline changes are ordinary source
changes.
