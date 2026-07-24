# Changelog

## 0.1.0 - 2026-07-20

- Migrated the selected MdeCpp core, device, utility, array, algebra, random,
  and behavioral-test sources into namespace `asc`.
- Preserved the MdeCpp shape/layout/index/iterator/CRTP array architecture and
  synchronized host/device memory model.
- Added six component-aware, relocatable CMake targets using `asc-cmake`.
- Compiled the exact Sobol direction data into the random library, removing the
  runtime source-tree file dependency.
- Corrected radical-inverse base handling, nondeterministic CSC inner ordering,
  empty compressed storage, static-view ownership, single-precision scalar
  operations, header self-containment, and cpplint/compiler diagnostics found
  during migration.
- Added CPU, OpenMP, CUDA, Eigen, MKL, precision, sanitizer, shared-library,
  header-isolation, ODR, and installed-package build paths.
- Added explicit MdeCpp provenance and random-generator third-party notices.
- Separated the stable core configuration facade from its generated CMake
  feature header and renamed the numeric facility to `math.h`.
- Removed the utilities-to-array dependency by representing structured
  configuration values with standard containers.
