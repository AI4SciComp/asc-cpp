# Milestone 2 portability, GPU, and performance review

Status: complete; no unresolved portability correction

Review date: 2026-07-27

## Scope and independence

This review covers only the frozen Milestone 2 independent-foundation
contract:

- provider-free Utilities command-line parsing and monotonic timing;
- the storage-neutral Expression protocol and four pointwise nodes;
- the base Philox4x32-10 and scalar `Uniform01` contracts; and
- the four-component static/shared build, package, relocation, subproject, and
  isolated-consumer integration.

The review read `main:AGENTS.md`, the milestone contract and ownership ledger,
the complete current Milestone 2 production and integration surfaces, the
production self-review, verification design and review, documentation/API
review, provenance record, architecture manifests, CI definition, package
fixtures, and available validation evidence.

No MdeCpp file, deleted asc-cpp random source or test, Random123
implementation or test, prior cumulative random implementation, or external
vector corpus was inspected. This report is the reviewer's only repository
write.

## Review result

No unresolved source, test, build, package, dependency, GPU, or performance
finding remains.

The accepted relative install-RPATH correction is necessary and correctly
bounded. `ASC::utilities` and `ASC::random` are compiled libraries with a
runtime dependency on `ASC::core`; installed ELF libraries now use `$ORIGIN`
and installed Apple libraries use `@loader_path`. No RPATH is added on
Windows. A clean local Clang Release/shared run passed all 79 tests, including
both relocated compiled-component consumers, and `readelf -d` confirmed
`RUNPATH [$ORIGIN]` on the relocated Utilities and Random libraries.

The earlier independent verification found the defect before this review:
without a relative RPATH, the relocated Random consumer could load
`libasc_random.so` but its transitive `libasc_core.so` dependency was not
found. The correction was then applied by the lead and independently
revalidated. No source-level workaround, absolute install path, provider edge,
or consumer environment mutation was introduced on ELF or Apple platforms.

## Compiler and standard-library portability

### GCC and Clang

- GCC 11.4 with libstdc++ is locally compile- and runtime-tested in the
  independent Debug/static 79-test matrix. Production also compiled strict
  representative Expression instantiations with GCC.
- Clang 19 with libstdc++ is locally compile- and runtime-tested. This review
  independently configured, built, and ran the complete Release/shared
  79-test matrix with warnings as errors.
- All ten new headers compile alone in strict C++20. GCC and Clang also compile
  them with exceptions disabled where that compiler mode is supported.
- Expression positive, negative, and multiple-translation-unit
  instantiations passed on the local frontends. The implementation uses
  standard C++20 concepts, fixed-extent `std::span`, `std::array`,
  `std::reference_wrapper`, and forwarding; no compiler extension is needed.

### MSVC

MSVC and native Windows remain hosted-CI evidence rather than a local claim.
Read-only inspection found no known incompatibility with the VS 2022 job:

- the public visibility headers select `__declspec(dllexport)`,
  `__declspec(dllimport)`, or the static definition as appropriate;
- CMake installs DLLs to the runtime directory and archives/import libraries
  to the library directory;
- isolated-consumer fixtures stage the producer and relocated DLLs next to
  their executables before runtime;
- disabled-exception tests and the GNU `-fno-exceptions` option are excluded
  for the MSVC frontend;
- integer and floating `std::from_chars`, C++20 concepts, `std::span`, and
  `steady_clock` are supported by the configured VS 2022 toolchain; and
- no GCC/Clang attribute or POSIX call is present outside guarded visibility
  and RPATH branches.

The Windows multi-config Debug/Release shared job is defined but has not run
at local Publication Checkpoint B.

### AppleClang

AppleClang and macOS runtime behavior also remain hosted-CI evidence. The
required job runs on macOS 15. The code uses Apple-compatible C++20 facilities,
and Apple's official
[C++ language support table](https://developer.apple.com/xcode/cpp/) lists
floating-point `std::from_chars` with a minimum deployment target of macOS
13.3. Thus the command-line double conversion is compatible with the intended
macOS 15 runner.

The guarded install RPATH is `@loader_path`, the correct same-directory form
for Utilities or Random to locate Core after moving an installed prefix.
That branch has been inspected but cannot be runtime-tested on the local Linux
host. Consumers targeting macOS older than 13.3 would need a separately
approved numeric fallback or an explicit newer deployment target; no
pre-13.3 compatibility claim is made here.

## Numeric, timing, and template portability

### Command-line numeric conversion

- Signed and unsigned conversion uses the corresponding fixed-width integer
  overload of `std::from_chars`, verifies both `std::errc{}` and complete-token
  consumption, and therefore catches invalid and out-of-range inputs without
  locale or coercion.
- Double conversion uses `std::chars_format::general` and the same exact
  result checks. It does not consult the process locale and rejects a
  comma-decimal token through incomplete or invalid conversion.
- The local GCC 11 and Clang 19 toolchains both compiled and ran a strict
  floating `from_chars` probe. The verification environment had only the
  `C`, `C.utf8`, and `POSIX` locales, so a positive parse while a comma-decimal
  locale is active remains untested.
- Parser lookup is linear in option count and its validation containers and
  configuration tree use ordinary standard allocation. No no-allocation or
  constant-time parser claim is made.

### Timer

`Timer` uses only `std::chrono::steady_clock` time points and durations.
Start, stop, elapsed, last, average, and reset retain native duration
arithmetic and do not convert through wall-clock or floating seconds. The
standard monotonic guarantee supports non-negative intervals on the reviewed
platforms. Every method is constant time apart from the implementation-defined
clock call and ordinary error-status construction.

The sample count is `std::size_t` while duration division converts it to
`Duration::rep`. Exceeding the positive range of that representation is a
theoretical very-long-run boundary; no realistic test can complete enough
samples to reach it, and no finite milestone benchmark is inferred.

### Expression templates

- Arithmetic terminals are held by value, external lvalues by
  `std::reference_wrapper<const T>`, and rvalues and nested nodes by value.
  The representation contains no direct reference to an rvalue.
- Construction validates only rank and fixed-size shape. It performs no
  scalar read, destination mutation, transfer, synchronization, provider
  selection, or framework result allocation.
- Shape validation is linear in rank; node construction is otherwise one
  operand copy/move per captured operand. A captured external type may itself
  allocate or retain a non-owning view, so the zero-framework-allocation claim
  does not override operand semantics.
- Scalar reads recurse through the expression tree and are allocation-free for
  the built-in nodes. Deep trees can increase compile time, code size, and
  scalar-read depth; no deep-tree benchmark or threshold is approved.
- Exact-rank `std::span` makes an incompatible read ill-formed. Rank-mismatched
  binary construction returns a shape error, and only rank-zero scalar
  expansion is accepted.

## Random portability and exactness assumptions

The current implementation was reviewed only against the frozen mapping and
the independent review records:

- all products are widened to unsigned 64-bit before multiplication;
- low/high extraction uses fixed unsigned shifts and narrowing conversions;
- Weyl additions occur in unsigned 32-bit arithmetic and therefore have
  defined modulo-2^32 behavior;
- ten rounds are explicit, with exactly nine between-round key increments;
- stream, subsequence, block, and lane mapping is expressed arithmetically
  rather than through host byte order;
- checked offset advance subtracts before adding and cannot wrap; and
- scalar transforms select exactly 24 or 53 high bits and multiply by exact
  binary powers.

The raw-word contract is endian-independent because public lanes are
`std::array` elements, not serialized memory. The exact object-bit transform
evidence assumes the IEEE-754 binary32/binary64 representations and
significand widths supplied by the approved GCC, Clang, MSVC, and AppleClang
platforms. An exotic non-binary or differently sized floating implementation
is not validated.

Philox block generation performs a fixed ten rounds. Positioned-word
generation recomputes one block and selects one lane; callers that need all
four words can use the direct block API. `Uniform01` is constant time and does
not call a standard distribution. No throughput, vectorization, statistical,
entropy, storage-fill, or GPU performance claim is made, and no benchmark was
invented.

## Dependency, provider, and GPU review

The target and public dependency graph remains exact:

```text
ASC::utilities  -> ASC::core
ASC::expression -> ASC::core  (interface library)
ASC::random     -> ASC::core
```

Mechanical and independent text scans found no sibling-module include,
Dense/Sparse include, provider SDK header, provider handle/type, generated
table, or external dependency in the ten public headers or four production
sources. Installed component lookups import Core plus exactly the requested
foundation target; forbidden sibling targets remain absent.

GPU evidence is **skipped**. Milestone 2 contains no GPU provider target,
source, compile unit, device runtime, or CPU/GPU parity surface.
`configure-tested`, `compile-tested`, `runtime-tested`, and `parity-tested`
are not claimed.

## Evidence reviewed

Independent verification evidence:

```text
GCC 11.4 / CMake 4.1.2 / Debug / static / warnings-as-errors:
  PASS, 79/79

GCC 11.4 / CMake 4.1.2 / Release / shared / warnings-as-errors:
  initial FAIL, 78/79 (relocated transitive Core lookup)
  correction PASS, focused 2/2 and complete 79/79

GCC 11.4 / ASan+UBSan / Debug / static:
  PASS, focused 67/67

Clang 19 strict direct verification:
  PASS, 11/11 compiled and ran
```

This review's clean independent shared run:

```sh
port_review_dir=$(mktemp -d /tmp/asc-cpp-m2-portability-clang.XXXXXX)
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B "$port_review_dir/build" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build "$port_review_dir/build" --parallel 4
ctest --test-dir "$port_review_dir/build" --output-on-failure
```

Result: pass, 79 of 79 tests; zero failures and zero skips. Evidence directory:
`/tmp/asc-cpp-m2-portability-clang.w9VQvI`.

```sh
readelf -d \
  "/tmp/asc-cpp-m2-portability-clang.w9VQvI/build/tests/package/"\
"install and relocate work/relocated ASCCpp prefix with spaces/lib/"\
"libasc_utilities.so"
readelf -d \
  "/tmp/asc-cpp-m2-portability-clang.w9VQvI/build/tests/package/"\
"install and relocate work/relocated ASCCpp prefix with spaces/lib/"\
"libasc_random.so"
```

Result: pass; each relocated compiled module has
`NEEDED libasc_core.so` and `RUNPATH [$ORIGIN]`.

Additional read-only checks:

```sh
git diff --check
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m2_dependency_check.cmake
rg -n -i \
  '(cuda|cublas|cusolver|cusparse|curand|hip|rocm|sycl|mkl|provider)' \
  include/asc/{utilities.h,expression.h,random.h} \
  include/asc/{utilities,expression,random} \
  src/{utilities,expression,random}
```

Results: pass. `git diff --check` and the dependency audit produced no error;
the provider scan produced no match.

## Remaining risks

- The required hosted MSVC/Windows and AppleClang/macOS jobs have not run at
  local Publication Checkpoint B. They remain CI evidence, not local claims.
- The Apple `@loader_path` branch is inspected but not runtime-tested locally.
- The minimum CMake 3.25 executable was unavailable for this independent
  review; current CMake 4.1.2 passed. Minimum-CMake validation must come from
  the lead's local evidence or the pinned CI job.
- The macOS floating `from_chars` facility requires macOS 13.3 or newer.
- Non-x86 hosts and exotic non-IEEE floating representations were not tested.
- ThreadSanitizer and standalone LeakSanitizer were not run in the independent
  verification wave; ASan+UBSan passed.
- Parser scaling, deep expression-tree compile/code-size behavior, and random
  throughput have no approved numerical threshold and were not benchmarked.
- External expression adapters remain responsible for valid indices, extents,
  referenced-storage lifetime, and synchronization. `Timer` mutation and query
  likewise require caller synchronization.
- GPU evidence remains **skipped**.
