# Testing and quality gates

The default and strict developer workflows are:

```bash
cmake --preset default
cmake --build --preset default --parallel
ctest --preset default

cmake --preset strict
cmake --build --preset strict --parallel
ctest --preset strict
```

GoogleTest is required only for these developer builds. The test sources are a
namespace/include-adapted migration of the selected MdeCpp suites and cover
core/device/memory, utilities, dense and sparse arrays, expressions, linear
algebra, Eigen integration, and random samplers.

## Core M1 verification

Canonical Core M1 tests live under `tests/core` and link the minimal
`ASC::core` component rather than `ASC::cpp`. The core suite verifies:

- signed 64-bit metadata aliases and the dynamic sentinel;
- stable status fields and `Result<T>` behavior, including move-only values;
- release-active contract translation and single evaluation;
- host-resource alignment, zero-size, equality, deterministic failure, and
  lifetime behavior;
- `Buffer<T>` negative/overflow rejection, move-only ownership,
  exactly-once release, object construction rollback, checked host access, and
  absence of implicit pointer conversion;
- serial context options, capabilities, resource queries, unavailable
  OpenMP/CUDA requests, synchronization, independence, and concurrent
  immutable use;
- completed-event readiness, backend identity, status, copying, and
  idempotent waiting.

After configuring and building a developer preset, run the focused suite with:

```bash
ctest --test-dir build/default -L core --output-on-failure
```

The registered tests are grouped as `asc-cpp.core.types`,
`asc-cpp.core.base`, `asc-cpp.core.memory-resource`, `asc-cpp.core.buffer`,
`asc-cpp.core.event`, `asc-cpp.core.execution-context`,
`asc-cpp.core.release-contract` when exceptions are enabled, and
`asc-cpp.core.dependency-boundary`, plus the multi-translation-unit
`asc-cpp.core.odr` check.

Failure-path tests use counting and failing test resources. They do not try to
exhaust the machine's real memory. Allocation-count checks are performance
sanity checks; they are not wall-clock benchmarks.

Core architecture checks cover canonical header self-containment, minimal
component linkage, representative multi-translation-unit use, forbidden
legacy/provider includes, and an installed core-only consumer. These checks
support M1's ownership and dependency claims. They do not establish OpenMP,
CUDA, asynchronous-execution, or explicit-copy conformance, because canonical
M1 does not implement those providers or operations.

The original generic suite remains a separate regression gate for the
unchanged `Memory<T>`, `MemoryManager`, `Device`, and `forall` compatibility
path. Passing those tests does not make the legacy interfaces canonical.

## Utilities M1 verification

Dedicated Utilities tests live under `tests/utilities` and link only
`ASC::utilities`, GoogleTest, and private project options. After configuring
and building a developer preset, run them with:

```bash
ctest --test-dir build/default -L utilities --output-on-failure
```

The focused sanitizer gate uses fail-fast AddressSanitizer diagnostics:

```bash
env ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build/sanitizers -L utilities \
  --output-on-failure --timeout 30
```

In exception/assertion-disabled builds, the release-contract executable is not
registered; the five remaining Utilities tests exercise the status paths and
structural boundaries.

The registered tests are `asc-cpp.utilities.config`,
`asc-cpp.utilities.cli`, `asc-cpp.utilities.timer`,
`asc-cpp.utilities.release-contract` when exceptions are enabled,
`asc-cpp.utilities.dependency-boundary`, and `asc-cpp.utilities.odr`.

The suite verifies every configuration value and canonical text form, exact
sorted serialization, round trips, validator and whole-load rollback, and
unknown-key policy. CLI coverage includes negative numeric values, all
supported token forms, repeat/reset behavior, failure rollback, unsupported
positionals, and help text beyond the historical fixed buffer. Timer coverage
includes empty/state contracts, more than 128 samples, lossless compression,
accumulation, reset, units, and exact print termination.

Architecture checks compile canonical and forwarding headers with the minimal
component, reject higher-component/legacy-runtime/provider includes from
canonical files, inspect the target edge, and exercise the API across multiple
translation units. The installed utilities-only consumer links
`ASC::utilities`, includes `<asc/utilities.h>`, round-trips configuration,
parses a negative option, and checks empty timer statistics. The generic suite
remains the regression gate for ordinary compatibility uses.

Additional structural tests verify:

- every enabled public header as the sole include of a translation unit;
- aggregate-header use across multiple translation units;
- independent installed consumers for all six package components;
- installation followed by relocation to a different absolute prefix;
- rejection of unknown `find_package` components.

Canonical core headers are additionally checked against the `ASC::core`
component alone. Legacy and higher-module headers may retain broader linkage
during their staged migrations.

## Array M1 verification

Focused tests live under `tests/array`, link only `ASC::array`, GoogleTest, and
private project options, and use the `array` label. Run them with:

```bash
ctest --test-dir build/default -L array --output-on-failure
```

The registered tests are:

- `asc-cpp.array.extents`;
- `asc-cpp.array.layout`;
- `asc-cpp.array.tensor-view`;
- `asc-cpp.array.tensor`;
- `asc-cpp.array.release-contract` when exceptions are enabled;
- `asc-cpp.array.dependency-boundary`;
- `asc-cpp.array.odr`.

The suite verifies rank-zero, zero, static, dynamic, and mixed extents;
negative values; checked product/span/offset overflow; and metadata beyond
`INT_MAX` without allocation. Mapping tests cover exact left/right/stride
formulas, holes, aliases, contiguity, conservative uniqueness, checked bounds,
and layout-independent coordinates.

View tests cover external storage, available-span and host-access validation,
mutable-to-const conversion, forbidden reverse conversion, const-owner typing,
and mutable-view uniqueness. Owner tests cover move-only/no-throw behavior,
default and zero-size descriptor states, exact allocation/deallocation counts,
failure rollback, moved-from state, and independent explicit clone. Structural
concept tests include third-party satisfying types and exact vector/matrix
ranks.

Architecture checks compile every canonical header with the minimal
component, reject legacy/higher/provider includes from canonical files, inspect
the exact `ASC::array -> ASC::core` target edge, and instantiate representative
templates across multiple translation units. The installed Array-only consumer
includes `<asc/array.h>`, constructs a mixed-extent owner, writes through a
mutable view, reads through a const view, clones explicitly, and verifies that
the owners do not alias.

The generic suite remains the regression gate for legacy `MShape`, `UArray`,
`DenseMArray`, views, expressions, and sparse arrays. Its broad coverage does
not prove canonical metadata, ownership, constness, dependency, or provider
claims.

The focused sanitizer gate uses fail-fast diagnostics and disables the local
runtime's recursive default segmentation-signal handler:

```bash
env ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:print_stacktrace=1 \
  ctest --test-dir build/sanitizers -L array \
  --output-on-failure --timeout 30
```

The completed Array gate passed all seven focused default tests; all 27 default
and strict/warnings-as-errors tests, including generic compatibility and
relocated package consumers; all seven focused
AddressSanitizer/UndefinedBehaviorSanitizer tests; and all six applicable
exception- and assertion-disabled tests. Canonical header isolation, release
contracts, dependency scanning, ODR, the minimal installed consumer, and
production/test lint also passed. The `handle_segv=0` sanitizer setting works
around the local CTest handler loop; it does not suppress or explain away a
sanitizer finding.

## Linalg M1 verification

Focused tests live under `tests/linalg`, link only `ASC::linalg`, GoogleTest,
and private project options. After configuring and building a developer preset,
run only the six canonical tests with:

```bash
ctest --test-dir build/default -R '^asc-cpp\.linalg\.' --output-on-failure
```

The existing `asc-cpp.algebra` compatibility executable also carries the
component label `linalg`. Use `ctest --test-dir build/default -L linalg` when
both the six canonical tests and that legacy regression gate should run.

The registered tests are:

- `asc-cpp.linalg.blas1`;
- `asc-cpp.linalg.blas2`;
- `asc-cpp.linalg.blas3`;
- `asc-cpp.linalg.release-contract`;
- `asc-cpp.linalg.dependency-boundary`;
- `asc-cpp.linalg.odr`.

The mathematical suite covers exactly the canonical `Copy`, `Scal`, `Axpy`,
`Dot`, `Nrm2`, `Gemv`, and `Gemm` overloads for `float` and `double`. It checks
left, right, and proven-unique stride mappings; padded spans and untouched
holes; rectangular and every supported transpose combination; empty and zero-
inner dimensions; explicit `alpha`/`beta`; and a NaN-poisoned destination that
proves the `beta == 0` no-read guarantee. Extreme finite values exercise the
scaled `Nrm2` algorithm.

The three mathematical executables contain 19 BLAS1, 20 BLAS2, and 17 BLAS3
GoogleTest cases.

Failure coverage includes shape and mode errors, inaccessible storage,
non-unique outputs, insufficient or overflowing spans/metadata, exact aliases,
partial/conservative overlap, failure-before-access, and byte-for-byte
unchanged outputs. Allocation instrumentation checks that a provider call does
not allocate after views exist. These are allocation sanity tests, not
wall-clock performance benchmarks. The completed check wrapped global
`new`/`new[]` and observed zero allocations while all seven operations ran on
prepared 24-by-24 operands.

Architecture checks compile each canonical Linalg header with the minimal
component, reject legacy/higher/provider includes from canonical files, inspect
the exact base target edges to `ASC::array` and `ASC::core`, verify the compiled
capability symbols and `serial-reference` identity, and instantiate the API in
multiple translation units. The installed Linalg-only consumer includes
`<asc/array.h>` for canonical owner/view construction and `<asc/linalg.h>` for
operations, runs at least one BLAS1 and one matrix operation, checks
status/results, and links only `ASC::linalg`. This verifies the public Array
target edge without turning the Linalg umbrella into an Array umbrella.

The existing `asc-cpp.algebra` executable remains a separately labeled
MdeCpp-migration regression gate for context-free legacy arrays, broad BLAS,
decompositions, solvers, and optional Eigen integration. Passing it does not
establish canonical shape, view, alias, status, provider, or dependency claims.

The focused sanitizer command uses the same local recursive signal-handler
workaround as Array:

```bash
env ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:print_stacktrace=1 \
  ctest --test-dir build/sanitizers -R '^asc-cpp\.linalg\.' \
  --output-on-failure --timeout 30
```

The completed Linalg gate passed all 33 default tests and all 33 strict
warnings-as-errors tests, including legacy algebra/random compatibility and
installed package coverage. The six focused sanitizer tests passed on rerun;
the BLAS2 and BLAS3 executables also passed directly with 20/20 and 17/17
GoogleTest cases respectively. An initial aggregate sanitizer invocation ended
in a nonreproducible host-level `SIGSEGV` and emitted no ASan or UBSan report.
The complete rerun used the documented local `handle_segv=0` setting and did
not suppress a sanitizer finding.

The exception-disabled/assertion-disabled profile passed all five applicable
canonical tests with warnings-as-errors disabled. Enabling warnings-as-errors
in that profile is currently blocked before Linalg compilation by a pre-existing
legacy Array unused-parameter warning when `ASC_DEBUG` is off; this does not
change the canonical Linalg test result.

Canonical header isolation, release-contract behavior, dependency scanning,
compiled capability identity, ODR, the minimal installed consumer, allocation
sanity, and relocated package coverage passed in default and strict builds.
Cpplint reported zero findings for canonical production/public files, focused
tests, and the installed consumer. Broader optional-provider milestones remain
deferred; a legacy backend build option is not promoted to canonical capability
by these results.

## Random M1 verification

Focused tests live under `tests/random`, link only `ASC::random`, GoogleTest,
and private project options, and use the `random` label. Run the canonical
executables by name:

```bash
ctest --test-dir build/default -R '^asc-cpp\.random\.' --output-on-failure
```

The registered tests are:

- `asc-cpp.random.engine`;
- `asc-cpp.random.distribution`;
- `asc-cpp.random.fill`;
- `asc-cpp.random.release-contract` when exceptions are enabled;
- `asc-cpp.random.dependency-boundary`; and
- `asc-cpp.random.odr`.

The engine suite freezes independently calculated Philox4x32-10 vectors for
zero, consecutive, keyed, subsequence, and high-counter inputs, including raw
word order and `Generate64` concatenation. The distribution suite checks exact
`float`/`double` zero, midpoint, maximum-below-one, and discarded-low-bit
behavior without using a standard-library distribution as an oracle.
The engine executable contains three cases and the distribution executable
contains four.

The fill suite covers `float` and `double`; rank-zero, rank-one, rectangular
rank-two, left, right, and unique padded-stride views; logical-coordinate
equality and hole preservation; two-way, irregular, reverse-order, and
concurrent partition equivalence; empty/null storage and counter boundaries;
foreign structural descriptors; validation ordering; byte-for-byte failure
transactions; compile-time scalar/authority rejection; and zero operation-time
allocation after setup.
The fill executable contains 20 typed cases. An enormous stride that could wrap
iteration arithmetic is rejected by checked byte-span overflow before
traversal; ordinary padded row-wrap traversal runs under UBSan and preserves
its holes.

Architecture checks compile `<asc/random.h>` and every canonical Random header
with only the minimal component, scan for legacy/higher/provider residue,
inspect the exact `ASC::random -> ASC::array, ASC::core` public edges, and use
the API across multiple translation units. The installed Random-only consumer
includes `<asc/array.h>` and `<asc/random.h>`, requests package component
`random`, links only `ASC::random`, and verifies a frozen result after
relocation.

The inherited `asc-cpp.random` executable is a separate MdeCpp-migration
regression gate linked through `ASC::cpp`. It covers legacy engines,
permutations, low-discrepancy sequences, and statistical samplers. Its range,
moment, and coverage tests do not establish canonical Philox, uniform-transform,
logical-partition, dependency, or cross-library bitwise guarantees.

The focused sanitizer command follows the existing local signal-handler
workaround:

```bash
env ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:halt_on_error=1:handle_segv=0:print_stacktrace=1 \
  ctest --test-dir build/sanitizers -R '^asc-cpp\.random\.' \
  --output-on-failure --timeout 30
```

The completed Random gate passed all six focused default tests. The full
default and strict warnings-as-errors suites each passed 39/39 tests, including
the inherited Random regression, header isolation, and relocated package
consumer. The exception-disabled/assertion-disabled profile passed all five
applicable canonical tests; as with Linalg, that profile leaves warnings as
errors disabled because of a pre-existing legacy Array warning when
`ASC_DEBUG` is off.

All six focused ASan/UBSan tests passed with the documented local
`handle_segv=0` setting and no sanitizer finding. `detect_leaks=0` reflects the
local sanitizer runtime configuration rather than suppression of an observed
Random leak. Canonical dependency scanning, multi-translation-unit ODR,
release-active transaction behavior, the 20-case fill/allocation executable,
the installed Random-only consumer, and cpplint over canonical production,
tests, and consumer sources all passed.

## Phase III cross-module integration gate

After all five module gates, the integrated default and strict builds each
passed 39/39 registered tests, including all canonical header-isolation
targets, inherited compatibility regressions, and relocation of every
installed component consumer. The five component dependency-boundary tests
also passed together.

The combined canonical ASan/UBSan selection passed 34/34 tests with
`detect_leaks=0` and the local `handle_segv=0` workaround. Some aggregate
CTest launches terminated unrelated executables during ASan startup without a
sanitizer diagnostic; direct execution passed every affected GoogleTest case,
and `ctest --repeat until-pass:3` completed the 34-test selection without a
finding.

With exceptions and assertions both disabled, every canonical module test,
dependency check, ODR test, inherited Linalg/Random regression, and relocated
package consumer passed. The broad inherited `asc-cpp.generic` executable is
not an all-green gate in that profile: eight legacy death tests assume
assertion-enabled failure or match the old `ASC` diagnostic text. The profile
therefore reports 33/34 registered executables while the applicable canonical
module gates remain green. Release-contract executables are intentionally not
registered in this configuration, and warnings-as-errors remains disabled for
the documented legacy Array warnings.

Recommended build matrix:

```bash
# OpenMP
cmake -S . -B build/openmp -DASC_CPP_ENABLE_OPENMP=ON \
  -DASC_CPP_BUILD_TESTING=ON

# Eigen
cmake -S . -B build/eigen -DASC_CPP_ENABLE_EIGEN=ON \
  -DASC_CPP_BUILD_TESTING=ON

# Single precision
cmake -S . -B build/single -DASC_CPP_PRECISION=single \
  -DASC_CPP_BUILD_TESTING=ON

# Shared libraries
cmake -S . -B build/shared -DBUILD_SHARED_LIBS=ON \
  -DASC_CPP_BUILD_TESTING=ON

# Sanitizers
cmake --preset sanitizers
```

Build each tree and run `ctest --test-dir <tree> --output-on-failure`.
Sanitizer builds omit the installed-package relocation test because downstream
executables would need the same sanitizer link flags.

Run cpplint from the repository root:

```bash
cpplint --recursive include/asc src tests examples
```

`CPPLINT.cfg` records the MdeCpp-compatible style policy, including a 100-column
limit and the intentional filter set. Production, test, and example sources are
expected to produce zero findings under that configuration.

Documentation must not promote a planned capability on the strength of an enum
or build option. A backend is documented as canonical only after its provider
passes the same context, resource, event, failure, package, and concurrency
contract suite.
