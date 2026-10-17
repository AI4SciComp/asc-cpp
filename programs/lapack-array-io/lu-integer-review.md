# Source-specific LU integer bounds

This is a corrective audit of the existing S/D/C/Z GETRF, GETRF2, GETF2,
GETRS, GETRI and GESV routes. It does not expand the public API, certify
other LU families, or close P04/P11. Equilibration and advanced LU remain
separately reviewed. The full required upstream denominator is unchanged.
Historical LU evidence identifies historical bytes; the evidence below
identifies this correction.

## Source arithmetic

Reference-LAPACK remains pinned to commit
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`. The external
`p04-lu-counts-01/precision-audit.log` retains complete fixed-form precision
comparisons and SHA256 identities for the reviewed routines and their relevant
BLAS/auxiliary dependencies. This comparison is not a substitute for the
source review: ZGETF2 retains an extra DLAMCH/SFMIN assignment, C/Z external
declaration ordering differs, and S/C GETRI uses SROUNDUP_LWORK whereas D/Z
assigns the integer directly to WORK(1).

Let `H = 2^(provider_integer_bits-1)-1`. All counts and products below are
provider INTEGER arithmetic, distinct from ASC-only storage/packing byte
arithmetic. Existing full backing-span and workspace checks still apply.

| Route | Actual source and checked bound |
| --- | --- |
| GETF2, nonempty with more than one row | `SRC/xgetf2.f` calls `xSWAP(N,...,LDA)` when a row swap occurs. `BLAS/SRC/xswap.f` updates its cursor after the final element, so `1+N*foreignLDA <= H ` is required. This also bounds GER/GERU's smaller `1+(N-J)*LDA ` cursor. |
| GETRF2, more than one row | The one-column leaf calls IxAMAX with length M; its unit-step loop must have representable final value M+1. Require `M < H `. Recursive LASWP uses two-dimensional `A(I,K)` subscripts, not an INTEGER cursor multiplied by LDA. Its trailing widths are `N-N1 < N `, so no invented `N*LDA ` cap is applied. |
| GETRF | The recursive limits apply. Pinned ILAENV GE/TRF selects NB=64. When blocked, the exact final loop value is `1+ceil(min(M,N)/64)*64`; the largest safe pivot count is `H-63`. GETRF2 is used when the pivot count is at most 64. |
| GETRS | A nonempty call reaches LASWP and TRSM loops. Require `N < H ` and `NRHS < H `. LASWP's 32-column cleanup has final block-loop value N32+1, not N32+32; the remainder and pivot loops account for the unit-step bound. No LDB-product cursor is invented. |
| GESV | The driver calls GETRF, then GETRS only on successful factorization. Both corresponding structural bounds are applied before the driver call. Zero RHS still factors nonempty A. |
| GETRI | Pinned ILAENV GE/TRI and TR/TRI select NB=64. `max(1,N*64)` is evaluated before the query return. Its checked product also bounds workspace indexing `I+(JJ-J)*LDWORK` and the much smaller execution-loop endpoints. TRTI2/GEMV and final column swaps use unit vector increments, not LDA increments. |

Empty factor/solve dimensions return before foreign work. The one-row factor
paths do not perform row swaps or trailing updates; the arithmetic helper
preserves that distinction instead of applying a blanket product limit.
GETRF2/GETF2 structural query validation no longer inherits GETRF's distinct
blocked-loop restriction. Original ASC row-major strides remain copied plan
options; only effective packed foreign leading dimensions are narrowed.

`INSTALL/sroundup_lwork.f` has SHA256
`0af9ea5734e269000b784beb910c8d7ae1fd4743fef6c6a6a22b8d41fb59e370`.
S/C GETRI first converts the exact raw integer to REAL, converts that REAL
back to INTEGER for comparison, and may multiply upward by one plus epsilon.
The new helper verifies both floating values are strictly below the exact
power-of-two upper-exclusive integer bound before the foreign call. It never
compares against an inaccurately rounded floating representation of INT64_MAX.
The same check runs for execution, because execution repeats the query formula.

D/Z GETRI does not call DROUNDUP_LWORK. Its binary64 query may round down;
the final preferred capacity is the maximum of the independently checked
source count and the checked actual returned query count. For example,
`64*(2^53+1) = 576460752303423552` must not lose 64 entries. The actual
`LWORK=-1` query is still performed; no formula-only query substitutes for
it. Minimum capacity, complex imaginary validation, byte limits, immutable
plan identity and raw INFO behavior remain in force.

## Exact corrected identities

The external snapshot `p04-lu-counts-01/source.tar` has SHA256
`71272088c5fba3eec36eda26cfad6590b2ff9e223460ff82dd007ea9a3946303`.
It starts with code commit `dde439dacc1b377b7064717daabbf7b572106b21`
and overlays only the five implementation/test files listed here. The
independent harness and dependency inputs are external.

| File | SHA256 |
| --- | --- |
| `src/dense/lapack/internal_lu_counts.h` | `834a371b8b637f787a8dcb1c08526370f564ab408f35e4cbbf285a9c5fa8a777` |
| `src/dense/lapack/reference_lu.cc` | `887d1bb721557ec716d34d9829952bee787eecd916dd1b77bb9d41df9e6ccdff` |
| `src/dense/lapack/reference_lu_expert.cc` | `55bd3cd61caa6bfc60b8b6b8cac24d4e725045fac822a0925648a399d8925426` |
| `tests/dense_lapack/lu_counts_test.cc` | `0e10b8d0031e6c6443adaa79b4f629ba9cbecec8ba540206a21abf1446d13d73` |
| `tests/dense_lapack/lu_expert_test.cc` | `e90607987d074cd6db877f8af85aaaa29c197e8c5d199c46503d03fdeaebef32` |

Provider build identities remain LP64
`7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c`
and true ILP64
`8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97`.
This is the audited GNU/Linux/static-provider configuration, not verification
of other platforms, foreign compilers or integer conventions.

## Independent regressions and verification

The public-route regression uses an actual two-element, one-column matrix
with `LDA=H ` and a one-entry pivot buffer. Its complete reachable span is
two elements; the unused next-column stride requires no fictitious storage.
Old GETF2 query code wrongly accepts it. New code rejects with `kOverflow`,
without a provider call, INFO, numerical mutation or allocation. A mismatched
safe plan and empty scratch ensure the old-code execution regression also
stops before any unsafe foreign execution. GETRF and GETRF2 accept the same
descriptor's query; GETF2 accepts `LDA=H-1`.

The old-code negative controls use exact committed production hashes
`03f8ed2d3d8093488182c89e0f563b6c84de618da123c5a990d8140f378828c3`
(reference_lu.cc) and
`a9807ba1c2487b56811e4e87b6d8b2616e1a6b14c7ad5dd44af9fe0575b070d1`
(reference_lu_expert.cc). Actual LP64 and ILP64 runs fail the two new expected
overflow assertions. No failing old result is counted as a pass.

Pure integer tests separately cover both ABI limits, blocked-loop endpoints,
GETF2's final strided cursor, empty/one-row exceptions, solve loops,
S/C initial and upward-rounding failures, and downward-rounded D/Z capacities.
No huge or unbacked descriptor is used for these synthetic arithmetic tests.

The corrected snapshot passed 13/13 CTest cases, zero failures/skips, in each
of Debug LP64, Debug ILP64, Release LP64, Release ILP64, ASan+UBSan LP64 and
ASan+UBSan ILP64. The selection includes one pure-count test and all existing
S/D/C/Z reference LU, expert LU and layout tests. Those tests retain independent
reconstruction/solve/inverse residuals, scales, genuine numerical failures,
workspace/pivot/INFO defects, minimum/preferred GETRI scratch, layout packing
and publication checks. There is no additional routine-coverage credit.

Sanitizers instrument the corrected adapters and tests, not the prebuilt
Core/Dense or Fortran archives. The earlier provider call-graph allocation
reviews remain applicable to unchanged foreign entry points; C++ allocation
and static-link allocator interception do not alone observe shared runtimes.
Clang 18.1.8 strict analysis of both production files and both tests passed
with the unchanged repository configuration; the helper is included and
instantiated in that selection. Formatting and diff whitespace checks pass.
No new public header or signature was added.

Additional Debug LP64 and ILP64 runs each passed 13/13 tests using the external
Linux libc allocation interposer, including direct libc and shared-libstdc++
positive controls before the first numerical operation. It observes exported
allocator calls from ASC, static provider code and shared runtime callers; it
does not intercept arbitrary hidden private allocators. No provider numerical
warm-up is excluded. Both runs preserve the existing independent C++/static
allocator observations and report zero allocations inside operation scopes.
The external harness initially lacked the diagnostic header include directory;
the failed build logs are retained and the corrected runs have suffix `-02`.
An earlier test compile error (attempted private default plan construction) is
also retained; the final test uses a real successful query's copied plan.

Debug adapters import `ldexp/ldexpf` for the checked power-of-two bound.
Exact libm identity is
`df621c68dbfed7e843434ef2faedb9f4d4b0543ad161e9a55eaf4d4ce2443176`.
`runtime-{ldexp,ldexpf,scalbn,scalbnf}.log` in the snapshot evidence folder
retains full disassembly: the wrappers call only their internal scaling
function, whose body has no calls. The checked inputs are one and exponent
31/63, hence neither the runtime's range-error nor NaN paths are selected.
This supplements, rather than overstates, the allocator-interposition scope.

All artifact locators in the following table are relative to external
`p04-lu-counts-01`.

| Actual evidence | SHA256 |
| --- | --- |
| `test-old-lp64-02.log` (expected two-assertion failure) | `11af9048ba6612e015cc1af74d12cd7cfdaf3c312e527b74ca261f8b9bb38938` |
| `test-old-ilp64.log` (expected two-assertion failure) | `f11a05e300d37178bc623f2d9bf87ca083ba5e4ed8185f629de526acbd461444` |
| `test-lp64-02.log` | `8825c4c87e2bf92d38041616bcda127c10bbccf6348c78bbb98bdfecbdc50f78` |
| `test-ilp64.log` | `69b46caf4a193d24b7530c5b23cfdd56d8665424add40e7f542239df654d0085` |
| `test-release-lp64.log` | `d5316c1c10a1134a5ac6775956d2695ebcf33f7bf17e9c6001285772c659f312` |
| `test-release-ilp64.log` | `d2cd3c5a4d784f8f6c3a8f7bd61eb00fa5b8cf70dc2e9f1b996d450d9c09f92e` |
| `test-asan-lp64.log` | `5e0042a0fc7dd7d0509d3d64d78eb91c5b755e758abe493e91fc2d9f27863bff` |
| `test-asan-ilp64.log` | `1f753089a3558c1ce64ba1db9bb402a3a04571cf2460bf573a5fc107f5eba466` |
| `test-libc-lp64-02.log` | `5bcb75c5547b3dae73a9c20fce48241eb355a9c1f0d632ac2a9a5613fe56d418` |
| `test-libc-ilp64-02.log` | `b9c16fa8ff555d1c7f3b6159c4cfc0a1a2b612a0fcc6d314393717d4e6ebe1c6` |
| `tidy.log` | `bcab2412213ae693363fc455878635672181068f8c4edd42ab4f414b451f7d36` |
| `precision-audit.log` | `c9d6d68e5ab7a64f094ee8700a7eed6b3f22866f93d85866d88202bb2c129b6b` |
| `binary-identities.sha256` | `5eacf3c4862343014b9d8f87fa028e52d6a3357ac3b9ddbf37f46affd006c633` |
| External `CMakeLists.txt` (final diagnostic include correction) | `77567d3faa901ee01220b80f1d3298aa4db920c52fc8e832f2fa848babf9585f` |

The retained harness records complete configure/compiler/link inputs. For
example, the corrected LP64 allocation lane is reproduced with:

`cmake -S . -B build-libc-lp64 -DCMAKE_BUILD_TYPE=Debug -DINTEGER_BITS=32 -DLIBC_PROBE=ON `,
then `cmake --build build-libc-lp64 -j 2` and
`ctest --test-dir build-libc-lp64 -V --no-tests=error`.
Use 64 and the distinct ILP64 build directory for the other ABI; ordinary
Debug/Release lanes omit `LIBC_PROBE `, and sanitizer lanes enable
`SANITIZE `. No provider source installation occurs in these commands.

Complete integrated/installed P11 gates and central evidence normalization
remain integrator-owned. The exact next integration task is registering
`tests/dense_lapack/lu_counts_test.cc` as a private test (with the repository
root include directory), then rebuilding the optional facet and all existing
provider/package tests for both ABIs on the combined frozen tree.
