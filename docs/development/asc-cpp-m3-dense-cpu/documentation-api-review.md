# Milestone 3 documentation and API review

Status: Complete after corrected-API re-review

## Authority and scope

The reviewer read the complete all-in-one runbook, frozen Milestone 3 contract,
ownership ledger, architecture blueprint, ADRs 0001, 0003, 0004, 0007--0011,
and 0013, current core/expression contracts, and existing module-documentation
conventions before reviewing the public dense API.

This role has exclusive write ownership only of:

```text
docs/modules/dense.md
docs/development/asc-cpp-m3-dense-cpu/documentation-api-review.md
```

Production, tests, CMake/package integration, architecture manifests, root
documentation, and other module guides are read-only. Findings outside this
scope are reported to the lead and are not documented around.

The review does not inspect or copy MdeCpp, the deleted asc-cpp implementation,
or third-party implementation/test material.

The final public surface inspected is:

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
src/dense/reference_linalg.cc
```

## Review checklist

- [x] Exact public names and signatures match the frozen contract.
- [x] The installed component example builds and runs using only
      `ASC::dense`.
- [x] Public C++20 headers are self-contained, directly included, guarded, and
      use flat `namespace asc`.
- [x] Dense depends directly only on core and expression.
- [x] Ownership, lifetime, const propagation, invalidation, and concurrency
      are explicit.
- [x] Rank, shape, layout, stride, slice, zero-size, and host-access contracts
      are explicit.
- [x] Evaluation documents expression capture, alias validation, transaction,
      allocation, and traversal.
- [x] Linear algebra documents scalar/layout/transpose requirements,
      accumulation, overlap, numerical behavior, and destination mutation.
- [x] Errors, costs, packing, workspace, transfer, synchronization, and
      deferred scope are not overstated.
- [x] GPU evidence is classified exactly as `skipped`.

## Findings

| ID | Severity | Evidence | Required resolution | Status |
| --- | --- | --- | --- | --- |
| DOC-M3-01 | release-blocking zero-extent correctness | `DenseLayoutMapping::CreateValidated`; a `LayoutLeft` shape `{0, 2}` derived strides `{1, 0}` and applied the nonempty uniqueness proof, reporting the empty mapping non-unique; a mutable view/owner would then reject required zero-extent storage | treat mappings with any zero extent as vacuously unique and exhaustive, retain logical/span size zero, add regression coverage, and validate negative shape before layout arithmetic so invalid metadata is diagnosed before derived-stride work | resolved; corrected mapping and strict zero-extent regression pass |
| DOC-M3-02 | release-blocking constraint correctness | `ReduceSum` accepted every arithmetic expression value, including `bool`, but `AddForReduction<bool>` selected the integral branch and attempted `CheckedAdd<bool>`, whose core constraint intentionally excludes `bool`; instantiation therefore failed inside the function body; `DenseElement` and the linalg scalar concept also advertised unsupported cv-qualified cases | exclude Boolean/volatile dense elements, exclude Boolean-valued reductions at the public constraint, and accept exactly unqualified float/double in dense algebra | resolved; corrected constraints and negative contracts reject unsupported cases |
| DOC-M3-03 | high API deduction/usability | strict GCC probe against the installed component: natural calls to `Copy`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm` with mutable views as readable operands all failed template deduction with incompatible `const Scalar`/`double` cv-qualifiers, even though `DenseView` intentionally supplies one-way mutable-to-const conversion | make readable operands deduce independently and accept mutable or const element views while retaining exact float/double scalar agreement and mutable destinations; add compile/runtime coverage | resolved; independently deduced readable element types pass the same strict probe |
| DOC-M3-04 | release-blocking empty-layout overflow | `DenseLayoutMapping::Create(LayoutLeft/Right)` derived strides before applying zero-extent emptiness; `LayoutLeft{max,max,0}` and `LayoutRight{0,max,max}` returned incidental overflow even though logical/span size is zero | stop canonical stride propagation at the layout-order zero boundary and deterministically zero metadata whose only derivation crosses that boundary; retain negative-metadata and nonempty/unavoidable-overflow rejection | resolved per lead ruling; exact huge-empty boundaries publish unique/exhaustive zero-size mappings |
| DOC-M3-05 | high template/error-contract correctness | public structural `DenseExtents` accepted non-copyable or throwing-move types while `DenseArray::Create` copied them and owner moves were unconditionally `noexcept`; the first core-binding correction still admitted cv-qualified `Extents` and exposed its recognition trait as an unapproved public API | bind the owner to an unqualified specialization of approved core `Extents<...>`, keep recognition machinery internal, and reject structural/cv-qualified substitutes at the concept boundary | resolved; corrected concept boundary and negative compile checks reject substitutes |
| DOC-M3-06 | medium allocation-contract precision | guide wording such as “no allocation” could be read as covering failed validation, although core `Status` diagnostics own `std::string` and may allocate | scope no-allocation guarantees to successful dense computational storage, temporaries, workspace, and packing; disclose possible diagnostic allocation | resolved in the dense guide; no production defect |

## Documentation changes

`docs/modules/dense.md` documents the component boundary, public headers,
storage/layout semantics, views and owners, expression participation and
evaluation, reductions, serial dense algebra, numerical and failure behavior,
costs, concurrency, provider/GPU evidence, and deferred scope.

The exact guide example uses only `<asc/dense.h>` and `ASC::dense`. It covers
core extents, explicit host-resource lifetime, two move-only owners, checked
view access, expression construction, rank-zero scalar expansion, destination
evaluation, const-view propagation, and deterministic reduction.

## Example and documentation validation

A fresh GCC 11 Debug warnings-as-errors build and install used:

```text
cmake -S . -B /tmp/asc-cpp-m3-doc-review.zzqhOI/build \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug \
  -DBUILD_TESTING=OFF -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m3-doc-review.zzqhOI/prefix
cmake --build /tmp/asc-cpp-m3-doc-review.zzqhOI/build --parallel 2
cmake --install /tmp/asc-cpp-m3-doc-review.zzqhOI/build
```

Result: configure, build, and install pass. The installed package contains the
dense library, all seven public dense headers, and its component export.

The exact C++ block from `docs/modules/dense.md` was built and run as an
installed component consumer:

```text
cmake -S /tmp/asc-cpp-m3-doc-review.zzqhOI/doc-example \
  -B /tmp/asc-cpp-m3-doc-review.zzqhOI/doc-example-build \
  -DCMAKE_PREFIX_PATH=/tmp/asc-cpp-m3-doc-review.zzqhOI/prefix \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/asc-cpp-m3-doc-review.zzqhOI/doc-example-build --parallel 2
/tmp/asc-cpp-m3-doc-review.zzqhOI/doc-example-build/dense_documentation_example
/usr/lib/llvm-19/bin/clang-format --dry-run --Werror \
  -style=file:/home/yicai/AI4SciComp/asc-cpp/.clang-format \
  /tmp/asc-cpp-m3-doc-review.zzqhOI/doc-example/main.cc
```

Result: configure, strict compile, link, runtime, and repository formatting
pass. The consumer requested only component `dense` and linked only
`ASC::dense`.

The original strict installed-header linalg probe reproduced DOC-M3-03 for all
six read-source APIs. Re-running the same source against the corrected headers
passes:

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Wsign-conversion -Werror -Iinclude -fsyntax-only \
  /tmp/asc-cpp-m3-doc-review.zzqhOI/api_probe.cc
```

Result: pass with no diagnostic.

The final bounded contract probe checks unqualified core-extents binding,
Boolean/volatile/scalar negatives, and both huge zero-containing named
layouts:

```text
g++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion \
  -Wsign-conversion -Werror -Iinclude \
  /tmp/asc-cpp-m3-doc-review.zzqhOI/corrected_contract_probe.cc \
  /tmp/asc-cpp-m3-doc-review.zzqhOI/build/src/core/libasc_core.a \
  -o /tmp/asc-cpp-m3-doc-review.zzqhOI/corrected_contract_probe
/tmp/asc-cpp-m3-doc-review.zzqhOI/corrected_contract_probe
```

Result: compile, link, and runtime pass. `git diff --check` passes for both
documentation-owned files.

An initial tests-enabled configure during the active independent-verification
write wave stopped because several sources named by the concurrently edited
dense test CMake file had not landed yet. The tests-disabled production/package
configuration above then passed. This transient coordination state is not
reported as a project configure failure or test result. The lead's clean
matrix owns the post-correction install refresh and complete CTest result.

## Evidence boundary and remaining risks

GPU evidence for Milestone 3 is exactly **skipped**. This review makes no
provider, GPU, sanitizer, hosted-platform, or performance claim without
executed evidence.

The guide distinguishes external-pointer preconditions from checked metadata:
view creation cannot prove pointer provenance, alignment, allocation length,
or actual placement. It also states that direct expression-protocol scalar
reads assume already validated host accessibility and indices. No-allocation
claims are expressly limited to successful computational storage/workspace
paths; failure diagnostics may allocate through core `Status`.

Hosted MSVC/AppleClang, sanitizer, benchmark, relocation, path-with-spaces, and
complete package/consumer evidence remain owned by the lead and the independent
verification/portability roles. Subject to those gates, this documentation and
public API review accepts Milestone 3 with no unresolved finding.
