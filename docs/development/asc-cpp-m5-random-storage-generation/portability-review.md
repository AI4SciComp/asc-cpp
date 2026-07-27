# Milestone 5 Portability, GPU, and Performance Review

Status: Complete; no unresolved release-blocking finding

Date: 2026-07-26

Role: independent portability/GPU/performance reviewer

Writable scope:

```text
docs/development/asc-cpp-m5-random-storage-generation/portability-review.md
```

## Verdict

Milestone 5 is acceptable from the reviewed portability, undefined-behavior,
concurrency, provider-isolation, allocation, packaging, and performance-evidence
perspectives. No production, API, target, or package change is required by this
review.

The earlier documentation finding `M5-DOC-01` is resolved: the ordinary
`StructurePriority` definition in the installed sparse-facet header is
explicitly `inline`, and strict multi-translation-unit consumers link and run.
No unresolved review finding remains.

The result is bounded to the locally available Linux toolchains. GCC 11 and
Clang 19 evidence passed. Native MSVC, AppleClang, macOS, Windows,
multi-configuration, Ninja, and clang-tidy evidence is absent and remains a
portability risk for the eventual support matrix.

Milestone 5 GPU evidence is exactly **skipped**. No toolkit configuration,
provider compilation, device execution, or CPU/GPU parity was performed or is
claimed.

## Review boundary and material inspected

This review read the complete runbook, frozen milestone contract and ownership
ledger, preflight, dependency/provenance records, accepted ADRs 0001--0004,
0007--0012, and 0015--0018, both production headers, base random
implementation, dense/sparse storage contracts, root and component CMake,
package configuration, independent runtime/compile tests, isolated consumers,
benchmark, module documentation, documentation/API review, production
self-review, and the verifier's frozen design.

The review did not inspect or copy MdeCpp or the user-deleted asc-cpp
implementation. It did not edit production, tests, CMake/package files,
manifests, module documentation, or another report.

## Environment

```text
repository: /home/yicai/AI4SciComp/asc-cpp
branch: feature/asc-cpp-m5-random-storage-generation
unchanged HEAD: 33b261ea33616a6395c4ad3b20646093103344f7
host: Linux x86_64 under WSL2
CPU: 11th Gen Intel Core i7-11800H, 8 cores / 16 logical CPUs
CMake: 4.1.2
GCC: 11.4.0
Clang: 19.0.0
clang-format: 19.0.0
ASCCMake: 0.1.0 installed contract
Ninja: unavailable
clang-tidy: unavailable
MSVC: unavailable
AppleClang: unavailable
```

Inventory also found CUDA 12.9, driver 576.83, and an NVIDIA GeForce RTX 3060
Laptop GPU with 6144 MiB. That inventory is not Milestone 5 GPU evidence.

## C++20 and ABI portability

The facet headers use portable C++20 library/language facilities: constrained
templates, `std::array`, fixed-extent `std::span`, fixed-width unsigned
integers, `std::same_as`, and `if constexpr`. They use no C++23 feature,
compiler extension, provider type, implementation-reserved name, public
nested module namespace, or nonstandard integer representation.

The headers are self-contained, use full-path include guards, and directly
include the facilities they use. All supported public names remain in flat
`namespace asc`; every helper namespace contains `internal`.

ODR review found:

- `PriorityLess` is `constexpr` and therefore implicitly inline;
- `StructurePriority` is explicitly `inline`;
- the remaining header functions and variables are templates or
  `inline constexpr`; and
- representative multi-TU template instantiations linked under both reviewed
  compilers.

The storage facets are interface libraries and require no DLL-export macro of
their own. Their template bodies call the already exported random/core and
storage symbols. Static builds propagate the existing `*_STATIC_DEFINE`
definitions through their public dependency closures; the shared installed
consumers exercised the corresponding import path. Native Windows DLL behavior
still requires an MSVC job before it is claimed.

Both strict compilers accepted the facet headers with exceptions disabled.
The production APIs do not throw or catch. Failure remains explicit through
`Status`/`Result`; diagnostic status construction may allocate and is not part
of the successful no-computational-allocation guarantee.

## Integer, lifetime, and undefined-behavior audit

### Dense generation

`FillDenseUniform01` rejects non-serial execution and non-host placement before
pointer use. It converts the validated signed logical size to `uint64_t`,
checks words-per-element multiplication, and checks returned-offset addition
before the first write.

Those successful checks dominate every later unsigned expression:

```text
offset + ordinal * words_per_element
```

For every visited ordinal the address is inside the already checked half-open
word range. The dense mapping constructor has separately checked every
stride/span term and total required span in signed 64-bit metadata. The
unchecked loop's physical offset therefore cannot overflow and cannot exceed
the validated accessible span for a proven-unique mutable view.

Rank zero has one iteration, an empty coordinate array, and physical offset
zero. A zero extent gives logical size zero, so the ordinal loop does not
execute and cannot perform modulo by zero or dereference a null empty-view
pointer. Padding is addressed by neither the logical traversal nor the random
word mapping.

The view is borrowed synchronously and remains non-owning. Concurrent calls
are safe only for disjoint destinations; overlapping mutation remains the
caller's responsibility.

### Sparse generation

`GenerateSparseUniform01` validates serial execution, host resource placement,
nonnegative/in-domain count, independent structure/value domains, both word
counts, and both next offsets before creating the builder.

For a nonzero count, the checked range

```text
structure_offset + 2 * logical_size
```

dominates every later structure address. The value word-count check likewise
dominates every value address. Priority composition shifts only an explicitly
widened `uint64_t`; no signed shift or signed overflow occurs.

Coordinate decoding divides only on the selected-entry path. A selected entry
implies nonzero count, and valid extents plus `count <= logical_size` imply
every extent is nonzero. Rank zero executes no decoding division and produces
the one valid empty coordinate.

Repeated scans advance a strict `(priority, ordinal)` threshold. The ordinal
tie break provides a total order, so a valid exact count cannot exhaust
candidates early. The builder receives unique coordinates, finalizes with
explicit duplicate rejection and zero retention, and owns all published
storage through the caller's resource. Partial builder allocation failure
rolls back through move-only `Buffer` ownership.

The resource must outlive the result, its views, and final deallocation.
Separate sparse calls can run concurrently only when their resources and
owners permit it; there is no hidden ASC random state.

No invalid narrowing, out-of-bounds pointer arithmetic, uninitialized read,
provider fallback, host/device transfer, densification, or hidden
synchronization was found in the reviewed paths.

## Dependency, provider, and package isolation

The observed direct facet edges match the frozen graph:

```text
ASC::random_dense  -> ASC::random;ASC::dense
ASC::random_sparse -> ASC::random;ASC::sparse
ASC::cpp           -> all six base modules plus both random facets
```

The base random target remains core-only. Dense and sparse contain no random
include/link edge, and neither facet includes or imports the other storage
module. Source scans found no CUDA, HIP, SYCL, cuRAND, cuSPARSE, OpenMP, TBB,
Eigen, or other provider/dependency include or discovery in the facets.

Fresh static package tests exercised build-tree consumption, a copied build
tree, installation, relocation, paths with spaces, required/optional/unknown
component behavior, no-component aggregate lookup, subproject use, and
isolated component closures. Fresh shared installed consumers separately
exercised `random_dense`, `random_sparse`, and `cpp` from an installation
prefix containing spaces.

No provider is configured or discovered by a provider-free component.

## Concurrency and sanitizer evidence

The runtime tests compare independent concurrent calls with serial calls.
Dense threads use disjoint destinations. Sparse threads use independent
owners and resources. Thread completion joins before result inspection.
Production contains no global, thread-local, static mutable, cached, or hidden
cursor state.

This review's Clang 19 ASan+UBSan matrix passed both runtime suites, the
positive contract, multi-TU execution, and the benchmark: 5/5 tests, with no
sanitizer finding.

The independent verifier additionally reported:

- the full final sparse adversarial suite passed under Clang 19 ThreadSanitizer
  at `/tmp/asc-m5-verifier-clang-tsan-sparse-final.ilCgzH`; and
- a dedicated dense disjoint-destination concurrency executable passed under
  Clang 19 ThreadSanitizer at
  `/tmp/asc-m5-verifier-clang-tsan-dense.9wj8i2`.

The verifier classified the full dense adversarial executable itself as
ThreadSanitizer-skipped because its deliberate global `new`/`delete`
allocation counter conflicts with Clang's ThreadSanitizer interceptors. That
is a test-harness incompatibility, not a production race finding.

## Allocation and complexity review

Dense generation performs:

```text
time: O(logical_size * Rank)
auxiliary automatic metadata: O(Rank)
dynamic computational storage/workspace: none
```

The allocation counter observed zero C++ allocation calls in each measured
successful dense operation. Static source review found no resource,
`new`/`delete`, container, packing, or temporary-buffer path.

Sparse generation performs:

```text
priority selection: O(exact_count * logical_size) time
selection metadata: O(Rank) automatic storage
builder finalization: O(exact_count^2 * Rank) reference worst case
value generation: O(exact_count)
dynamic output: exactly the builder's coordinate and value buffers
selection workspace: none
```

The measured nonempty benchmark made exactly two resource allocations and two
deallocations per sparse generation. Runtime fault injection independently
covers first- and second-allocation rollback and exactly-once release.

This is intentionally a deterministic bounded-memory reference algorithm.
Its repeated Philox evaluation and quadratic builder finalization are material
performance limits for large shape/count inputs, not hidden costs. Optimized
selection is deferred and no speed promise is appropriate.

## Performance smoke evidence

The GCC 11.4 Release/static benchmark was executed five additional times on
the identified CPU after the fresh build. Its fixed workload was:

```text
backend: serial-reference
dense: float, shape 64x64, unique padded stride, 500 iterations
sparse: double, shape 32x32, exact_count 64, 20 iterations
```

Observed results:

| Metric | Five-run result |
| --- | --- |
| dense elapsed | 34599, 35402, 35278, 35266, 35724 us |
| dense median | 35278 us |
| dense allocations | 0 on every run |
| dense next offset | 2048000 on every run |
| dense checksum | 2046.81 on every run |
| sparse elapsed | 39207, 41211, 40712, 41006, 42400 us |
| sparse median | 41006 us |
| sparse resource calls | 40 allocations and 40 deallocations on every run |
| sparse next offsets | structure 40960, value 2560 on every run |
| sparse checksum | 39724 on every run |

The harness warms the dense operation, keeps results observable through
offsets/checksums, and measures only successful calls. It reports one aggregate
elapsed duration per process, has no previous/provider baseline, and does not
produce a statistically controlled timing distribution. System load also
affected the initial post-build observation. Therefore this evidence is a
correctness/allocation/performance smoke, not a regression threshold,
provider comparison, or performance guarantee.

## Exact independent commands and results

### GCC 11 Release, static, package, relocation, and consumers

```text
cmake -S . \
  -B /tmp/asc-cpp-m5-portability-gcc.18yrk6/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m5-portability-gcc.18yrk6/prefix \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m5-portability-gcc.18yrk6/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m5-portability-gcc.18yrk6/build \
  --output-on-failure -L milestone-5
cmake --install /tmp/asc-cpp-m5-portability-gcc.18yrk6/build
```

Result: configure, full build, and install passed. The M5 selection passed
39/39: runtime, positive/multi-TU/header contracts, six compile negatives,
benchmark, architecture/dependency checks, 17 consumer-labeled cases, and 20
package-labeled cases. Both package matrices and the registry-unchanged test
passed.

### Clang 19 Debug, shared installation and isolated consumers

```text
cmake -S . \
  -B /tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/build \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  "-DCMAKE_INSTALL_PREFIX=/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces" \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build \
  /tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/build --parallel 4
cmake --install \
  /tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/build
```

For each of `random_dense`, `random_sparse`, and `cpp`:

```text
cmake -S tests/consumer/<component> \
  -B "/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/<component> consumer" \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  "-DASCCpp_DIR=/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/lib/cmake/ASCCpp" \
  -DASCCPP_EXPECT_LIBRARY_TYPE=INTERFACE_LIBRARY
cmake --build \
  "/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/<component> consumer" \
  --parallel 4
"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/<component> consumer/asc_cpp_<component>_consumer"
```

Result: producer configure/build/install and all three isolated
configure/build/runtime sequences passed.

### Clang 19 strict exception-disabled consumption

Both installed facet consumer sources and both header-alone parses passed:

```text
clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion \
  -Wsign-conversion -Werror -fno-exceptions \
  -I"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/include" \
  tests/consumer/random_dense/main.cc \
  -L"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/lib" \
  -Wl,-rpath,"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/lib" \
  -lasc_dense -lasc_random -lasc_core \
  -o /tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/random_dense_no_exceptions

clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra -Wconversion \
  -Wsign-conversion -Werror -fno-exceptions \
  -I"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/include" \
  tests/consumer/random_sparse/main.cc \
  -L"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/lib" \
  -Wl,-rpath,"/tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/prefix with spaces/lib" \
  -lasc_sparse -lasc_random -lasc_core \
  -o /tmp/asc-cpp-m5-portability-clang-shared.ooKHh7/random_sparse_no_exceptions
```

Both generated executables ran successfully.

### Clang 19 ASan and UBSan

```text
cmake -S . \
  -B /tmp/asc-cpp-m5-portability-clang-sanitize.yxjJjL/build \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build \
  /tmp/asc-cpp-m5-portability-clang-sanitize.yxjJjL/build \
  --parallel 4 \
  --target asc_random_dense_test asc_random_sparse_test \
           asc_m5_random_contract asc_m5_random_multi_tu \
           asc_random_storage_benchmark
ctest --test-dir \
  /tmp/asc-cpp-m5-portability-clang-sanitize.yxjJjL/build \
  --output-on-failure \
  -R '^asc_cpp\.random_(dense\.runtime|sparse\.runtime|storage\.(compile_contract|multi_tu|benchmark))$'
```

Result: 5/5 passed with no ASan or UBSan finding.

### Format and whitespace

`clang-format-19 --dry-run --Werror` over all M5 production, test, consumer,
and benchmark C++ files passed. `git diff --check` over the reviewed M5 paths
passed.

## GPU evidence

```text
classification: skipped
configure-tested: no
compile-tested: no
runtime-tested: no
parity-tested: no
```

Reason: the frozen Milestone 5 contract is CPU-only and explicitly excludes
provider targets, options, discovery, source, compilation, runtime, and parity.
Running a CUDA check here would implement or test later-milestone scope and
would misclassify inventory as facet evidence.

## Remaining risks and deferred evidence

- Native MSVC/Windows DLL, AppleClang/macOS, and multi-config generators remain
  untested locally.
- Ninja and clang-tidy are unavailable.
- The full dense allocation-counter executable is incompatible with Clang
  ThreadSanitizer interception; the dedicated dense concurrency executable is
  the TSan evidence.
- The reference sparse selector intentionally rescans the complete logical
  domain for every selected entry and uses the sparse builder's quadratic
  reference finalizer.
- Very large valid shape/count inputs can therefore be impractically slow even
  though arithmetic and storage bounds are checked.
- Benchmark timings have no prior/provider baseline and are not stable CI
  thresholds.
- Device memory, GPU execution, GPU random generation, and CPU/GPU bit parity
  remain later milestones.

None of these limitations contradicts the frozen Milestone 5 publication
contract. They must not be converted into stronger portability, provider, or
performance claims at Publication Checkpoint B.
