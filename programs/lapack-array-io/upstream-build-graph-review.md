# P00/P04/P09 upstream build-graph review

Review date: 2026-09-07.

This is source-selection and archive-definition evidence, not ASC binding,
ABI, link, numerical, allocation or mode-coverage evidence. No required row is
removed or newly marked implemented/verified. It supplements the recorded
transitive-build-list limitation in [inventory-review.md](inventory-review.md);
the frozen inventory, lock, mapping and their schemas are unchanged.

## Exact inputs and result

Reference-LAPACK commit `6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`, tree
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`, unsigned annotated tag object
`5ebe92156143a341ab7b14bf76560d30093cfc54` remain the sole required source
denominator. The inventory SHA256 is
`1397a216e4ffd8b92c5b91976c1569c26ecc62b775136769f0ffb731b57a51c7`;
the lock SHA256 is
`e40cc81715470c6059bb6f742421585c308121a6f3157b425207f09b18585bb3`.

Both existing static provider attestations were reverified read-only, including
every attested installed file. Their identities are:

- LP64: `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`.
- True ILP64: `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.

The audit retains all 2,113 required mathematical routine identities. Counts
below are derived from the frozen inventory and actual archive definitions,
not from an assumed four-precision expansion.

| Observation | LP64 | True ILP64 |
| --- | ---: | ---: |
| Required source routines | 2,113 | 2,113 |
| Required routines with actual Fortran definitions | 1,983 | 1,983 |
| Required routines without definitions | 130 | 130 |
| Required deprecated routines with definitions | 44 | 44 |
| Required routines with any actual LAPACKE definition | 1,211 | 1,211 |
| Source-selection / required-symbol disagreements | 0 | 0 |

There are 1,247 required identities with LAPACKE declarations and 866 without
them. Of the 866, 772 already have Fortran definitions in each prepared provider
and 94 belong to the missing XBLAS-guarded set. The other 36 missing required
routines have LAPACKE declarations but no compiled wrappers in these builds.
Fortran, C, `_work` and `_64` declarations never multiply the denominator.

## Reviewed source-list semantics

The new offline reader follows literal `set`, `list(APPEND)`, positive option
guards, duplicate removal and the actual LAPACKE child-directory list-transfer
macro. It does not run upstream CMake or execute shell text from metadata.
Unsupported source-list syntax fails closed. Every final source has an
OR-of-AND option formula, definition/expansion locations, and its occurrence
count before deduplication. Every required row references its exact source
instance hashes and observed archive/member definitions.

The relevant pinned files are:

| Source input | SHA256 |
| --- | --- |
| Root `CMakeLists.txt` | `1621bfa04748e84eb69a1c09d59b6b46e609458884c7ba6ea95c3279e4e95ca8` |
| `SRC/CMakeLists.txt` | `61e3692d4c1f924170d4bc8c20f03a65fd5cb57c876f697e40e2ba4ffc1b3797` |
| `BLAS/SRC/CMakeLists.txt` | `1f2f6e4797552f8f0911a1b1b4c004fa56f41520e22b1a0fc19a9874fe05f6fd` |
| `LAPACKE/CMakeLists.txt` | `627ca7fead3394ddd52d0b130b74e5c3e6d2bda9a418bf9f3caa1c8cee1d24d8` |
| `LAPACKE/src/CMakeLists.txt` | `cb15b0dd5de903cfbf43d48878d3a74f40152e1860aa01ec57c11353a873d3a7` |
| `LAPACKE/utils/CMakeLists.txt` | `ff41df3b6a37ae25f6a51e428938b71a888ad2b40469335c89ae191c20dde552` |

The reader rechecks every inventory source-input hash, not only this shortlist.
The following relationships were reviewed against the
[pinned LAPACK lists](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/SRC/CMakeLists.txt)
and [pinned LAPACKE list assembly](https://github.com/Reference-LAPACK/lapack/blob/6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca/LAPACKE/CMakeLists.txt).

- `ALLMOD` objects are always added to the reference LAPACK target; `ALLAUX`
  accompanies each enabled precision. `SCLAUX` is shared by single-real and
  single-complex builds; `DZLAUX` by double-real and double-complex builds.
- `DSLASRC` is selected by either `BUILD_SINGLE` or `BUILD_DOUBLE`. Consequently,
  `sgetrf` is not selected solely by its apparent single-precision spelling.
  `ZCLASRC` similarly joins `BUILD_COMPLEX` and `BUILD_COMPLEX16`; repeated
  literal `cgetrf` entries remain provenance occurrences, not extra routines.
- Each of the four deprecated lists appends 11 source files to its matching
  precision list under `BUILD_DEPRECATED`. All 44 remain required compatibility
  entries. The prepared builds enable this branch.
- `USE_XBLAS` appends `SXLASRC`, `DXLASRC`, `CXLASRC` and `ZXLASRC` to the
  respective primary lists. The final formula combines that option with the
  actual consuming precision option. This accounts for exactly 130 required
  absent routines: 18 classified drivers and 112 classified computational
  entries. None is treated as optional to ASC.
- LAPACKE has separate precision options, checked against the corresponding
  Fortran precision options. Its deprecated and extended lists are appended
  under `BUILD_DEPRECATED` and `USE_XBLAS` independently of its four precision
  list selections. Partial-precision builds therefore require their own
  object/link checks; these all-four-precision builds are not evidence for them.
- LAPACKE's `MATGEN` list is selected by `LAPACKE_WITH_TMG`. The prepared builds
  disable that option. `BUILD_TESTING` separately builds test generators; those
  source/test identities remain in the inventory and are not expert capability
  exclusions inferred from absent wrappers.
- The root selects `second`/`dsecnd` implementation files after platform probes.
  The symbolic graph preserves these substitutions; each actual cache selects
  `INT_ETIME`. The excluded timing implementations do not silently absorb any
  required numerical source.

The reader's scope is reference BLAS/LAPACK object assembly, not a general
CMake interpreter. The root's user-supplied and optimized BLAS/LAPACK branches
can replace whole libraries and require different identity/capability evidence.
Both attested configurations use the actual reference source objects. Their
`BUILD_INDEX64_EXT_API` is OFF. True ILP64 changes integer compilation and
archive names to `blas64`, `lapack64`, `lapacke64`; it does not imply suffixed
`_64` API exports. The separate extended-API duplication/mangling branch is
reviewed source behavior, not built or verified here.

## Independent object and symbol reconciliation

GNU nm 2.38 reads the three exact installed archives per ABI. Defined global
symbols retain archive/member identities; module data, BLAS helpers and C
utility symbols are not silently promoted to required LAPACK capabilities.
`lsame`, `xerbla` and `xerbla_array` each have two actual definitions across
reference BLAS and LAPACK archives, but still each name one required identity.

Alternate `SRC/VARIANTS` implementations, INSTALL compatibility sources, and
test-source procedures sharing names are retained as distinct source instances
with their original inventory classification. An instance not selected by the
provider object lists is explicitly recorded; a required row must still have
a reviewed selected-source route. Named blocked/two-stage APIs remain separate
required identities rather than being collapsed into algorithm alternatives.

Independently generated CMake `DependInfo.cmake` object lists were read, not
executed. Each build cache must match the hash in its attestation. Static graph
selection agrees exactly with actual object-source lists in both builds:
155 BLAS, 1,983 LAPACK including module/timing sources, and 2,578 LAPACKE sources.
The coincidental equality between the LAPACK source-file count and the required
defined-routine count does not establish one-file/one-routine equivalence.

Every absent required routine and every missing LAPACKE declaration is listed
individually in the external audit, with source formulas and archive evidence.
No evidence claim depends solely on the counts above.

## XBLAS dependency findings and next gate

The [authoritative Netlib XBLAS page](https://www.netlib.org/xblas/) identifies
version 1.0.248 and its C implementation with a toolchain-specific Fortran
bridge. An external read-only source observation from its linked archive has
SHA256 `b5fe7c71c2da1ed9bcdc5784a12c5fa9fb417577513fe8a38de5de0007f7aaa1`.
This is a first-download identity, not cryptographic authentication or an
approved dependency-lock update. It is not built or distributed by ASC.

The archive license was read in full and matches the separately retrieved
[Netlib license](https://www.netlib.org/xblas/LICENSE), SHA256
`d8fbddb866858a6a3a97bd00b7ef7758822fff72c635be67ad64b8c8ef71ca43`.
Its notice/redistribution conditions still require the repository's actual
owner review before any disposition that requires approval. Program approval
is not recorded as that review.

The source audit finds 18 direct Fortran callers of 28 distinct `BLAS_*_X`
symbols. They are the real/complex general, band, positive-definite,
symmetric and Hermitian extended-refinement kernels present in the inventory.
The remaining 112 routines under the same build guard are drivers or helpers;
absence of a direct XBLAS call does not remove their upstream build condition.
The audit records direct calls only, not a purported complete transitive call
graph or allocation proof.

The observed XBLAS `src/gemv/BLAS_dgemv_x-f2c.c` takes C `int*` dimensions,
increments and options and calls an `int`-indexed C kernel. Its SHA256 is
`61109a5b5d3d678d284ed4ea4639d68c4a56068540621bd0d3db71ca886bd11e`.
`src/f2c-bridge.h`, SHA256
`a432c80398d75f9809f80f5e526af5a077eb6a352e73925933f25042774213fc`,
provides mangling controls but describes default 64-bit Fortran integer support
only as a possible future configuration. Changing LAPACK's Fortran integer
flags cannot establish a compatible XBLAS bridge or wider C indexing.

The observed `BLAS_error` default path writes to stderr and exits; its optional
XERBLA route is not a substitute for checked ASC preflight. Extra-precision
kernels also contain conditional FPU control save/restore code. Required work
therefore includes an exact accepted source/license disposition, a separately
attested LP64 build, a genuine audited ILP64-compatible route, numerical
extra-precision tests, error-path isolation, FPU-state and allocation checks,
and actual bindings for all affected rows. No ordinary-BLAS substitution,
unchecked integer reinterpretation or process-global error override is approved.

## Reproduction and tooling evidence

The repository tool is [audit_build_graph.py](../../tools/lapack/audit_build_graph.py);
its independent fixtures are
[test_audit_build_graph.py](../../tools/lapack/test_audit_build_graph.py).
The tool records its own hash and those of its imported repository validators,
the unchanged inventory identity, exact provider identities/archive hashes,
sanitized nm output, raw-output hashes, and object-list input hashes. It rejects
stale output, missing required source routes, malformed graph/symbol records,
unattested/mixed prefixes and source/definition disagreements. Artifacts are
created exclusively outside the source tree.

Use the external evidence root from the program's local path index as
`LAPACK_EVIDENCE`; the following command does not write into the repository:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 -B tools/lapack/audit_build_graph.py \
  --source-root "$LAPACK_EVIDENCE/lapack-source" \
  --inventory docs/contracts/lapack-upstream-inventory.json \
  --provider-lock docs/contracts/lapack-provider-lock.json \
  --provider lp64 "$LAPACK_EVIDENCE/provider-lp64-attestation-01.json" \
    "$LAPACK_EVIDENCE/installs/lapack-lp64-static" \
  --provider ilp64 "$LAPACK_EVIDENCE/provider-ilp64-attestation-01.json" \
    "$LAPACK_EVIDENCE/installs/lapack-ilp64-static" \
  --build lp64 "$LAPACK_EVIDENCE/builds/lapack-lp64-static" \
  --build ilp64 "$LAPACK_EVIDENCE/builds/lapack-ilp64-static" \
  --output "$LAPACK_EVIDENCE/p00-build-graph-01/audit-final-02.json" --check
```

Final exact identities:

- Audit tool: `734e8918b9c36bcc8090ca22ff26980f2d77cff92363580ae677a2f3e3896e29`.
- Independent test file:
  `8853ed82a353fc7b6dab073741f67cabb2e2c3f6e527655a0dba9c239c27dbac`.
- External audit `p00-build-graph-01/audit-final-02.json`:
  `561ded07827e306e170ea13630421899b54a899abff816b37ede6376aa1b4793`.

The exact-tool generation and byte-for-byte `--check` both passed, with logs
`p00-build-graph-01/generate-final-02.log` and
`p00-build-graph-01/check-final-02.log`. Its 13 independent unit tests pass with
no skips (`unit-final-02.log` in the same external directory); the complete
current LAPACK Python suite passes 73 tests with no skips
(`all-lapack-python-01.log`). Pylint reports no diagnostics and 10.00/10
(`pylint-final-02.log`). These are tooling tests, not LAPACK numerical tests.
The existing `asc_cpp.program.lapack_tools` CTest discovers every matching
LAPACK unit file, including this one once included in the frozen source tree.
The integrator separately reran all13 tests and the actual byte-for-byte
audit check successfully (`integrator-unit-01.log`, `integrator-check-01.log`).
No zero-test CTest listing is counted as execution.

Integrator tree `1be952b427157f8ee4ec7c10c85fd7f6a51d3055` executes the
ordinary four-test program CTest lane successfully, zero skips; the LAPACK
suite includes all73 Python cases. Logs/JUnit are
`p00-audit-integrated-01/ctest-02.{log,xml}`. An earlier invocation before
configuration completed found zero tests and failed; its log is retained and
is not credited. This tooling-only snapshot excludes pending LU/Matrix Market
sources and the next foundation correction.

This audit does not replace P09's installed link probes, missing-interface
shims, routine-specific contract/ABI reviews or mathematical tests. Full-profile
completion remains blocked by the missing required routes; the denominator is
unchanged.
