# Optional Dense provider integration evidence

This record covers only the frozen column-major S/D/C/Z GETRF/GETRS slice.
It does not close P04, P11, row-major conversion, other LU families, the full
inventory, shared isolation or owner/license approval. No upstream archive is
redistributed by ASC installation, and no approval is inferred.

## Implemented component contract

`ASC_CPP_ENABLE_LAPACK` defaults OFF. Explicit enablement registers the
Dense-owned `dense_lapack` component and `ASC::dense_lapack` target, its two
provider-only headers, private implementation, tests, export file and
relocatable dependency metadata. Base umbrellas and `ASC::cpp` remain unchanged.
The CMake-only installed loader is lazy, validates exact archive/runtime hashes
and identity against the compiled facet, and reports missing optional providers
without invalidating a valid requested base component. It neither searches for
BLAS/LAPACK nor enables C/Fortran/Python in a consumer.

The implemented profile is `incremental-getrf-getrs-column-major`. Its audited
compiler boundary remains GNU 11.4.0/Linux x86-64 with the recorded libstdc++.
`ASC_CPP_LAPACK_REQUIRE_FULL_PROFILE=ON` invokes the strict frozen coverage gate;
the required full profile currently fails. Shared configuration also fails its
outstanding isolation gate. Neither rejection is completion of that profile.

Exact dependency identities remain:

- Reference source commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`.
- Source-input manifest
  `5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
- Frozen inventory
  `1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`.
- LP64 provider build
  `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`.
- True ILP64 provider build
  `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.

The original unbound-metadata module `cmake/ConfigureLapackProvider.cmake`
had SHA256 `1656527d21435b918ae4c227dada3d99e64e7472f0f45f379dbcaa794a251925`;
the corresponding loader `cmake/ASCCppLapackDependency.cmake` had SHA256
`3759328f72b1a61b5682f8af452bef35189b9f1ba97019d555ffc3b8f4fb20a2`.
Provider production/header identities are unchanged from the
[private ABI review](provider-abi-review.md).

## Executed evidence

The external root for the following paths is the sibling
`asc-cpp-evidence/lapack-array-io` directory; the local absolute-path index is
kept outside the source tree.

- Actual LP64 and true ILP64 configure probes compile and run the pinned
  C/C++/Fortran/library interoperability checks. The initial ILP64 integrated
  probe correctly rejected a 32-bit Fortran probe object. CMake propagated its
  caller language flags after `CMAKE_FLAGS`; the correction binds the probe's
  true integer width before `try_run` and restores the caller flags afterward.
  No failing check was skipped. Initial failed logs remain external.
- Original LP64 registered suite: `logs/p04-integration-lp64-provider-04.log`.
  Original true ILP64 suite: `logs/p04-integration-ilp64-provider-01.log`.
  10/10 tests each, zero skips. Four scalar numerical/allocation processes,
  four self-contained/no-exception header executables, one installed package
  test and one source configuration test. The installed package test includes
  24 lazy/required/optional cases plus both numerical example routes; the source
  test includes six explicit configuration gates. Counts do not denote 10
  newly supported routines. A later review found a flawed required-rejection
  oracle; those required negative cases are not credited until the corrected
  suite below executes. The original real numerical, optional and base checks
  remain distinct evidence. The final registration uses separate multi-value
  labels so exact `-L '^lapack$'` selects the real full ten-test subset.
- Source/build-hidden consumer run: `p04-install-isolation-02/logs/run.log`.
  both ABIs pass the copied C++-only factor-once/two-RHS array-I/O example's
  native and reference tests after original source, build, ASC install and
  provider-prefix paths are absent. ASC and dependencies are relocated to
  paths containing spaces. Link maps select the exact relocated archives;
  rival empty BLAS/LAPACK archives on search paths are not selected. The
  original tree was not hidden or altered: only dedicated external copies were
  moved. The snapshot is based on feature HEAD
  `066d355f75cb4c77652466af0110d2c1c662c834` plus then-current implementation
  changes; `source.tar` SHA256 is
  `be7bf5191ff6363e2d982f1ae3ed87cc7f53ae57324283cccd05a4d44edb36a7`.
  `run.sh`, configure/build/install logs, link maps and dynamic dependency
  reports are in that external directory.
- Integrated ASC sanitizer run: `p04-install-isolation-02/logs/asan-run.log`.
  4/4 scalar tests per ABI on the same frozen snapshot. Linked ASC Core, Dense,
  provider and test objects were rebuilt with ASan/UBSan. Upstream Fortran/BLAS
  archives and shared runtime internals were not instrumented. The earlier
  static-allocation/source-call-closure proof retains its bounded scope.
- Minimum CMake run: `p04-install-isolation-02/logs/cmake325-run.log`.
  CMake 3.25.2 configures and executes the ABI probe, builds the provider and
  passes 4/4 scalar tests plus 2/2 relocated C++-only example tests for each
  integer ABI on the same frozen snapshot. This is a minimum-version provider
  smoke, not the entire baseline suite. The local PyPI CMake wheel SHA256 is
  `f9587645cea5298d5eb20649f58a83cc53c9efa7d57b0716449a39f366a74986`;
  its download and installation logs are external. No privileged installation
occurred.

## Metadata digest binding correction

The original installed loader trusted the generated JSON record, as it trusted
the installed CMake config. Comparing only its identity label did not bind
library/runtime records to that label: a rewritten record with a matching
replacement archive hash could retain the original identity. A normal complete
old record with a different identity was rejected, but a mixed record was not.

The corrected producer computes the exact generated metadata file SHA256 and
embeds it in `ASCCppConfig.cmake`. The installed loader checks this digest
before parsing JSON, and only when the provider is requested. This detects
stale/mixed metadata under the trusted-generated-config model. It is not
cryptographic authentication against someone who can modify both installed
config and metadata.

The original required-provider negative probe also unconditionally failed if
the provider target existed after lookup; an incorrectly accepted provider
could therefore look like a successful rejection test. The corrected probe
returns success after a successful required lookup, includes a valid-provider
control, and verifies the intended digest-mismatch diagnostic for a rewritten
record whose archive hash actually matches a modified dedicated test copy.
There are now 28 installed isolation/control cases, plus the real numerical
example routes. No provider archive outside the dedicated fixture is modified.

New code identities:

- Configure module:
  `18827e388a829ad78c368dcc0ddf57e279c7eb2970fbb23b9ac69cff551ef595`.
- Installed dependency loader:
  `107658010f23276251b3bd94efd57b91771ac09c38efbcb49ca8017e8d7ce9ba`.
- Package config template:
  `2ec9cc96efbe50eac9c92468b4ef2de24c13cdecbeed4858a6d12248d51c701f`.
- Corrected package test:
  `74a60b7ddf90c1061032866d78a1ae1fa0e80a9c2c49402a46eda5efc22de660`.

Fresh source snapshot: `p04-metadata-binding-01/source.tar`, SHA256
`bd64e0be4dc07fbc4a6193382fbf861e672a207d43034032b286def3d6e6b6c0`.
It is the integrator's frozen `97f806d` tree archive plus the exact four-file
binding/test delta; it excludes unfinished expert LU files. Both original
provider attestations were reverified read-only. Fresh LP64 and true-ILP64
configure/build runs each passed all 20 selected CTest cases, with no skips:
10 provider tests, four package-metadata closure tests, and six required
fixture preparation/install actions. Their raw logs are
`p04-metadata-binding-01/logs/test-lp64.log` and
`p04-metadata-binding-01/logs/test-ilp64.log`. The four scalar numerical tests
and the 28-case package-isolation test are included, not extra CTest counts.

A separate same-input reproducer records the old unbound loader accepting
the mixed record and the new bound loader rejecting it before target creation:
`p04-metadata-binding-01/mixed-record-proof/old.log` and
`p04-metadata-binding-01/mixed-record-proof/new.log`. This is diagnostic evidence,
not additional numerical capability credit.

Retain the local toolchain's explicit `COMPILER_PATH` when executing CTest's
nested configuration checks. An earlier invocation omitted it and CMake's
Fortran driver-identification emitted a `cc1` diagnostic, although the actual
ABI probe succeeded. The final LP64 and ILP64 invocations carry the documented compiler
environment; no full-profile failure is accepted unless it reaches and reports
the intended strict coverage gate.

## Remaining integration gates

The final coherent committed tree still needs the integrator's full
Debug/Release/provider-free/shared-base/docs/header/ABI regression lanes. The
first active-tree targeted run passed architecture, M3 and all four enabled
package-metadata closures but detected concurrent Core/header hash changes;
those are not counted as passing frozen-tree surface verification. Subsequent
Core/resource fixes and any expert LU additions require their own exact-tree
evidence. Minimum-version CMake smoke is separate from the executed CMake 4.1.2
package suite. Shared-provider, additional compiler/ABI/runtime and
full-profile coverage remain required, not silently removed from scope.
