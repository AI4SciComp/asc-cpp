# Milestone 5 Documentation and API Review

Status: Complete; accepted after one production correction

Date: 2026-07-26

Role: documentation and API reviewer

Writable scope:

```text
docs/modules/random.md
docs/development/asc-cpp-m5-random-storage-generation/documentation-api-review.md
```

## Review boundary

This review is distinct from production implementation and independent
verification. It uses the frozen Milestone 5 contract, ownership ledger,
accepted ADRs and manifests, actual public headers, target/package behavior,
and independently exercised examples. Production headers, tests, CMake,
package integration, root documentation, architecture manifests, and other
module guides are read-only to this role.

The review does not inspect or copy MdeCpp, the user-deleted asc-cpp
implementation, historical random-storage tests, third-party random
implementations, or provider SDK examples.

The review covers:

- public names, template constraints, return types, and diagnostics;
- preservation of the storage-neutral random base;
- explicit stream, subsequence, offset, execution, and memory state;
- dense logical order, layout independence, padding, and state advancement;
- sparse candidate order, priority order, canonical finalization, exact count,
  and independent structure/value domains;
- rank-zero and zero-extent behavior;
- ownership, resource lifetime, move state, and view lifetime;
- validation-before-mutation/allocation and failure transactions;
- allocation, workspace, traversal, and reference-complexity disclosure;
- component isolation and installed usability;
- concurrency and the absence of hidden mutable state;
- provider, CPU/GPU, numerical, and statistical claim boundaries; and
- license and clean-room provenance.

The current official Google C++ Style Guide was checked at
`https://google.github.io/styleguide/cppguide.html`. Its current C++20,
self-contained-header, Include-What-You-Use, header-definition/ODR,
type/function/accessor naming, explicit-ownership, and exception guidance
agree with accepted ADRs 0003, 0004, and 0009.

## Authoritative material read

- the complete team runbook version 2.0;
- the frozen M5 milestone contract, ownership ledger, preflight, dependency
  audit, provenance record, and independent verification design;
- ADRs 0001--0004, 0007--0012, 0015, 0017, and 0018;
- the dependency manifest, capability manifest, and backend capability matrix;
- current core, expression, dense, sparse, and random-base public contracts;
- current dense and sparse module guides;
- current ASCCpp component, package-config, target, install/export, and
  isolated-consumer conventions;
- the two emerging M5 production headers and final production self-review; and
- the official current Google C++ Style Guide.

No applicable repository `AGENTS.md` was present in the retained restart tree.

## Public surface reviewed

```text
include/asc/random.h
include/asc/random/distribution.h
include/asc/random/engine.h
include/asc/random/export.h
include/asc/random/dense.h
include/asc/random/sparse.h

asc_random        / ASC::random
asc_random_dense  / ASC::random_dense
asc_random_sparse / ASC::random_sparse
asc_cpp           / ASC::cpp
```

The final facet declarations are:

```text
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>

SparseUniform01Generation<Element, ExtentsType>
  array
  next_structure_offset
  next_value_offset

GenerateSparseUniform01<Element>(
    context, extents, exact_count, resource,
    structure_stream, structure_subsequence, structure_offset,
    value_stream, value_subsequence, value_offset)
    -> Result<SparseUniform01Generation<Element, ExtentsType>>
```

The public functions and types use the approved Google-style names and remain
directly in flat `namespace asc`. Exactly unqualified `float` and `double`
participate. There is no default random state or default facet argument.

## Finding

### M5-DOC-01: non-inline sparse helper in a public header

Initial severity: release-blocking ODR and installed-API defect

Initial evidence: `include/asc/random/sparse.h` defined the ordinary
non-template function
`internal_random_sparse::StructurePriority(...)` in the public header without
`inline` or `constexpr`. Every translation unit including the facet header
therefore supplied an external definition, making a representative multi-TU
consumer ill-formed at link time. The function cannot appropriately become
`constexpr` because it calls the compiled Philox word interface.

Required resolution: make the internal header definition explicitly `inline`
and retain a multi-TU link regression.

Resolution: production changed the definition to
`[[nodiscard]] inline ... noexcept`. Independent GCC 11 and Clang 19 strict
two-TU compilation and linking now pass. The verifier-owned M5 multi-TU
executable also covers the public operation templates. No unresolved ODR
finding remains.

## API and contract conclusions

### Storage and dependency boundaries

The base `<asc/random.h>` includes only the base engine, distribution, and
export surface. It does not include dense, sparse, expression, utilities, or
either facet. `ASC::random` links directly only to `ASC::core`.

The dense facet header includes dense but no sparse or utility header;
`ASC::random_dense` exposes exactly `ASC::random;ASC::dense`. The sparse facet
header includes sparse but no dense or utility header;
`ASC::random_sparse` exposes exactly `ASC::random;ASC::sparse`. Neither
storage module was modified to include or link random. No facet contains a
provider header, type, library, target, option, or SDK edge.

Installed component consumers confirmed the exact closures:

```text
random_dense  -> core;expression;random;dense;random_dense
random_sparse -> core;expression;random;sparse;random_sparse
cpp           -> all six modules;random_dense;random_sparse;cpp
```

### Dense facet

`FillDenseUniform01` accepts a mutable `DenseView<float, Rank>` or
`DenseView<double, Rank>` by value and returns the first unused word offset.
Top-level const, volatile, Boolean, integral, and other floating element types
fail at constraints.

The implementation validates serial execution, host placement, logical-size
conversion, word-count multiplication, and next-offset addition before the
first write. It decodes dimension-zero-fastest logical ordinals, then applies
the view's validated strides. Thus physical layout does not alter logical
word assignment and physical padding is not visited.

The documented word mappings, high/low double order, rank-zero scalar, empty
shape, exact next offsets, partition requirement, allocation behavior, and
caller synchronization match the frozen contract and current header.

### Sparse facet

`GenerateSparseUniform01<Element>` requires explicit serial execution, a host
resource, validated core extents, an exact signed count, and two distinct
structure/value `(stream, subsequence)` pairs. `Element` is explicitly
selected and is exactly unqualified `float` or `double`.

Candidate ordinals use last-dimension-fastest lexicographic order. Two
structure words form a high-word-first 64-bit priority. The
`(priority, ordinal)` comparison supplies deterministic tie breaking and
repeated scans select exactly the requested count without workspace. Builder
finalization explicitly rejects duplicates and keeps generated zero values.

Values use finalized canonical stored position rather than candidate ordinal
or priority-selection order. The guide distinguishes the structure address
domain from the value address domain and does not promise different output for
different addresses.

The result is move-only through its `CoordinateArray` field. The caller's
resource is non-owned and must outlive the result, its views, and final
deallocation. Count-zero, rank-zero, zero-extent, exact advancement,
validation-before-allocation, partial-allocation rollback, no-workspace,
explicit-zero, and canonical-structure contracts are stated explicitly.

### Error, cost, and evidence boundaries

The guide separates stable `ErrorCode` categories from unstable diagnostic
text. It limits no-allocation claims to successful computational
storage/workspace and notes that diagnostic `Status` construction may
allocate.

Dense and sparse time/storage costs are stated as serial reference costs, not
performance guarantees. No transfer, packing, hidden conversion, compressed
output, densification, synchronization, provider dispatch, or fallback is
claimed.

The numerical section guarantees only the approved bit, scalar-transform, and
storage-mapping contracts. It does not advertise a formal sparse-subset
distribution, statistical suitability for every application, or uniqueness
of output across distinct addresses.

GPU evidence is exactly **skipped**. Toolkit and hardware inventory is not
described as configure-tested, compile-tested, runtime-tested, or
parity-tested facet evidence.

## Documentation changes

`docs/modules/random.md` now documents:

- the storage-neutral base and both independent storage facets;
- build, export, component-closure, and aggregate target contracts;
- all six public random/facet headers;
- the retained exact Philox and `Uniform01` sequence contract;
- dense and sparse signatures and address mappings;
- dense dimension-zero-fastest versus sparse last-dimension-fastest order;
- exact state advancement, empty/rank-zero behavior, and partition rules;
- structure/value independence and canonical sparse finalization;
- ownership, views, memory-resource lifetime, transactions, and concurrency;
- allocation/workspace/reference-complexity costs;
- provider/GPU/numerical/statistical claim boundaries;
- M2 engine and M5 storage-facet provenance; and
- explicitly deferred later-milestone work.

It includes three standalone C++20 examples: the base scalar API, a dense
facet fill, and an exact-count sparse facet generation.

## Independent validation

### Producer build and installation

A fresh static GCC 11 Debug producer was configured, built, and installed:

```text
cmake -S . -B /tmp/asc-cpp-m5-doc.HMY8lI/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m5-doc.HMY8lI/prefix \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m5-doc.HMY8lI/build --parallel 4
cmake --install /tmp/asc-cpp-m5-doc.HMY8lI/build
```

Result: configure, warnings-as-errors build, and install passed. The
installation contained both facet headers and separate random, dense, sparse,
random-dense, random-sparse, and aggregate exports.

### Examples, formatting, and constraints

Each exact C++ block in `docs/modules/random.md` was extracted from the
document. All three passed:

```text
clang-format-19 --dry-run --Werror

GCC 11.4:
  -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion
  -Wsign-conversion -Werror
  strict compile, installed-library link, and runtime

Clang 19:
  -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion
  -Wsign-conversion -Werror
  strict compile, installed-library link, and runtime
```

An independent constraints translation unit compiled with both compilers and
`-fno-exceptions`. It accepted `float`/`double` dense and sparse calls,
rejected const/volatile/integral facet scalar participation, and verified that
`SparseUniform01Generation<double, Extents<>>` is movable but not copyable.

The corrected sparse header was separately compiled into two translation
units with both strict compilers and `-fno-exceptions`, then linked with a
third main translation unit. Both executables ran successfully. This is the
independent recheck for M5-DOC-01.

All three installed isolated consumers were configured, built, and run:

```text
cmake -S tests/consumer/random_dense \
  -B "/tmp/asc-cpp-m5-doc.HMY8lI/random_dense consumer" \
  -DASCCpp_DIR=/tmp/asc-cpp-m5-doc.HMY8lI/prefix/lib/cmake/ASCCpp \
  -DASCCPP_EXPECT_LIBRARY_TYPE=INTERFACE_LIBRARY

cmake -S tests/consumer/random_sparse \
  -B "/tmp/asc-cpp-m5-doc.HMY8lI/random_sparse consumer" \
  -DASCCpp_DIR=/tmp/asc-cpp-m5-doc.HMY8lI/prefix/lib/cmake/ASCCpp \
  -DASCCPP_EXPECT_LIBRARY_TYPE=INTERFACE_LIBRARY

cmake -S tests/consumer/cpp \
  -B "/tmp/asc-cpp-m5-doc.HMY8lI/cpp consumer" \
  -DASCCpp_DIR=/tmp/asc-cpp-m5-doc.HMY8lI/prefix/lib/cmake/ASCCpp \
  -DASCCPP_EXPECT_LIBRARY_TYPE=INTERFACE_LIBRARY
```

Each was followed by `cmake --build <build-directory> --parallel 4` and its
generated executable. Result: all three configure/build/runtime sequences
passed, including their required/forbidden imported-target assertions.

Public include scans found no storage edge in the base, no sparse/utility edge
in the dense facet, and no dense/utility edge in the sparse facet.
`git diff --check` passes for both documentation-owned files.

## Provenance and remaining evidence

The documentation distinguishes the established M2 engine provenance from the
new M5 project-owned storage mappings. No MdeCpp, deleted asc-cpp,
third-party implementation, external test vector, table, or prose was copied.
No dependency or third-party notice is added; Apache-2.0 remains unchanged.

There is no unresolved documentation or public-API finding. This role's fresh
build was static, GCC-hosted, non-sanitized, and non-relocated. Complete
static/shared, GCC/Clang, sanitizer, package relocation, path-with-spaces,
subproject, benchmark, and source/provenance scans remain owned by the lead,
verification, and portability reviews. This document does not preempt their
evidence or Publication Checkpoint B decision.
