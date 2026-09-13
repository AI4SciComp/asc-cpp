# Bounded Reference-LAPACK LU provider review

This is an implementation/evidence record, not an owner or license approval.
It covers only S/D/C/Z GETRF and GETRS, column-major host/pinned-host storage,
and the exact separately selected GNU LP64 or true ILP64 reference builds.
Row-major packing, other routines, shared-provider audits, suffixed64, callbacks,
complex return values and alternative compiler/library ABIs remain outside this
verified slice. The full 2,113-routine required denominator is unchanged.

## Selected call boundary

The private implementation uses the pinned installed `lapack.h` typed
`LAPACK_{s,d,c,z}getrf/getrs` declarations/macros. It does not invent Fortran
symbol spellings or hidden string-length arguments. This per-routine decision
avoids LAPACKE's negative-INFO shift for its extra layout argument and permits
the exact signed Fortran INFO to survive in `LapackReport`. GETRF/GETRS do not
have an LWORK=-1 query; their ASC queries calculate only checked integer
conversion capacities. This choice is not a universal direct-Fortran binding
policy.

Private `HAVE_LAPACK_CONFIG_H` and `LAPACK_COMPLEX_CPP` select upstream's
documented `std::complex` declarations. `LAPACK_ILP64` is defined only for the
true 64-bit build, not the separate suffixed64 API. Neither foreign headers nor
foreign integer types appear in public headers or base Dense linkage.

Every dimension and leading dimension is checked before narrowing. The
`kInteger` caller workspace role holds live provider-width pivot integers;
the `kPivotConversion` role, whose foundational contract is ASC index-width,
is not repurposed. Nonallocating placement array new establishes a real array
lifetime in aligned writable caller bytes. Its zero-overhead guarantee follows
[CWG 2382](https://cplusplus.github.io/CWG/issues/2382.html). Output pivot ranges
are completely checked before signed one-based widening is published.

## Complex-language and ABI proof boundary

Representation sizes alone are insufficient. The following guarantees and
implementation evidence are used together:

- C++20 [complex.numbers]/4 guarantees interleaved array-oriented access to
  the real/imaginary components of live `std::complex<float/double>` objects.
  This is an explicit language-library guarantee, not an inferred struct
  layout. See [WG21 N4861, section 26.4](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/n4861.pdf).
- C complex types have the representation/alignment of two corresponding real
  elements in real/imaginary order. This establishes the C representation side,
  not a universal cross-language aliasing permission. See
  [WG14 N1570, section 6.2.5 paragraph 13](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf).
- GNU Fortran uses the platform's C99 complex argument convention, passes
  ordinary arrays by address, and uses trailing size_t character lengths on
  this compiler generation; `-ff2c` changes those assumptions and is excluded.
  New probes use ISO_C_BINDING names/kinds. See
  [GNU argument conventions](https://gcc.gnu.org/onlinedocs/gfortran/Argument-passing-conventions.html)
  and [intrinsic C-interoperable kinds](https://gcc.gnu.org/onlinedocs/gfortran/Intrinsic-Types.html).
- The inspected GNU libstdc++ 11 implementation stores the float/double complex
  specializations in its compiler's `__complex__` member type. The local
  `/usr/include/c++/11/complex` SHA256 is
  `5816151c9816930182a0cb8d7932dcae2480d57cc14b5329b1d8254d9c041c43`.
  The implementation decision is restricted to GCC/GFortran 11.4.0,
  libstdc++ `__GLIBCXX__=20230528`, x86_64 Linux and the recorded flags.
  Unknown toolchains require a new review/probe or explicit conversion shim.

The actual C probe moves representations via memcpy into C-owned complex
objects before C arithmetic, so it does not impose a C effective type upon a
live C++ object. The independent bind(C) Fortran probe reads and mutates actual
live C++ complex arrays. Finally, both linked library probes execute complex
GETRF and GETRS N/T/C with nonzero imaginary components and padding guards.
The true ILP64 test initializes high pivot bits to a nonzero marker; both INFO
and pivot writes must have the selected full width. No provider struct pointer
reinterpretation occurs in the production implementation.

These executed, implementation-specific interoperation checks supplement the
language guarantees. They do not assert that every C++/C/Fortran combination is
ISO-language compatible merely because its type sizes match.

## Pinned identities

Reference source commit:
`6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca`; tree:
`7217db728e4f7ee87dabf545a1a87a3d6cd30e9b`; v3.12.1 annotated tag object:
`5ebe92156143a341ab7b14bf76560d30093cfc54` (unsigned).

The provider report's source digest is the lock's source-input manifest SHA256
`5a0b8771c9496e65a2e40d1b9ffd7add38332cadde334abe8f762b750aefea9a`.
The independent compiled ASC wrapper identity is its source/commit, not this
upstream digest.

| Build | External build-attestation identity SHA256 |
| --- | --- |
| LP64 static | `7334974ceff5d71da38f7df94ffb10803d136b15bd7fa32e6cc1d49e465b210c` |
| True ILP64 static | `8460d29a665eb2aedc0c6d081026280b966d44015227bc47a69b362ca6f5da97` |

Both installs have these private header identities:

| Header | SHA256 |
| --- | --- |
| lapack.h | `225f20409a7d6674953f5af1de8c47188bb792ca11d46d465bb28dde917fcafc` |
| lapacke_config.h | `480a19beb74832b7db0d72be6581cf22a1c54ebe26ffacbaeca87d01e6a76dc2` |
| lapacke_mangling.h | `aed4772ddd94c8ec82b93155e15026badf0cc8b156baa085e1543d9c085077eb` |

The parent integration must verify these selected installed inputs/configuration
and reject unreviewed toolchains rather than relying on caller-supplied identity
macros. A prebuilt installed C++ consumer must not need C or Fortran compilers.

## Verification and allocation scope

External evidence root is the sibling `../asc-cpp-evidence/lapack-array-io`
recorded by the local program state; raw absolute-path commands stay external.
Standalone evidence directories are `provider-lu-lp64-01` and
`provider-lu-ilp64-01`; each contains reproducible `build-probe.sh`,
`build-tests.sh`, `build-asan.sh`, raw logs and executables outside source.

- `abi-probe.log`: C/C++/Fortran and actual linked complex GETRF/GETRS N/T/C.
- `test-{s,d,c,z}.log`: independent cold processes, each with small square,
  tall and wide factor reconstructions at three scales, nine reusable solves
  with both solution error and op(A)X-B residual, and a 67-by-67 blocked GETRF
  reconstruction. The pinned ILAENV chooses NB=64 for GETRF. Each scalar also
  executes real provider singularity with INFO=2.
- Tests preserve padding and immutable reusable factors/pivots; validate
  capacity/alignment/overlap/placement/plan tampering/staleness; distinguish
  LP64 dimension rejection from true64 metadata acceptance; reject malformed
  pivot values, mixed native/provider factors and exact-zero U before RHS
  mutation; and distinguish empty successful noncalls from foreign INFO=0.
- Test-only GNU link wrapping injects negative INFO, invalid returned pivots
  and excessive positive INFO to verify defensive diagnostics/publication.
  These are synthetic report tests, not upstream numerical evidence and not
  a global XERBLA replacement.
- Allocation observation spans each complete ASC execution call, including
  the first foreign operation in each process, repeated operations, and error
  paths. It combines the existing C++ new probe with malloc/calloc/realloc/
  aligned_alloc/posix_memalign wrapping across ASC and linked static provider
  objects. No numerical warm-up is excluded from the observation.

The ELF wrapping is not, by itself, dynamic interposition inside shared
runtimes. `static-call-closure.json` records the exact archive-symbol closure,
excluding only XERBLA, whose argument-error branches are prevented by complete
preflight and checked downstream dimensions. That closure has no allocator or
GFortran runtime entry: the remaining external symbols are cabs, cabsf, logf,
lroundf, memcmp and memset. This supports the bounded valid-input route's
allocation audit; it must not be generalized to other routines or described
as a dynamic audit of every shared runtime internals path.

Standalone ASan/UBSan covers the new ASC provider/foundations/tests. Linked
baseline Core/Dense and upstream Fortran/BLAS static archives are not sanitizer
instrumented by these standalone commands; full integrated sanitizer and
installed-component checks remain parent-integration gates.

Clang 18.1.8 formatting/static-analysis and public-header self-containment are
checked separately. Neither generated declarations, successful probes alone,
nor the upstream build's 111 tests imply full ASC LAPACK coverage.
