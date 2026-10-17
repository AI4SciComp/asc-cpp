# Checked triangular storage conversion

This leaf adds S/D/C/Z TRTTP and TPTTR through sixteen public query/execute
declarations. The eight required Reference rows have eight modes each: upper
or lower triangle and four independent dense/packed layout pairs. These are
callable-unverified routes; no Reference verification, wider-platform admission
or owner acceptance is inferred. Native20 and the accepted I/O subset retain
their separate identities.

## Source and integer review

All eight complete pinned definitions and their emitted GNU11 LP64/global
ILP64 declarations were reviewed. Installed C declarations agree with the
emitted signatures after removing input-pointee const qualifiers only. The
native probes check the full integer width, character length, real/complex
storage and complete guarded argument/output arrays. Sixteen archive-member
disassemblies preserve the selected loop and address arithmetic: native packed
cursors have the selected INTEGER width, and array addresses use 64-bit
arithmetic in both prepared x86-64 providers.

The routines only assign selected values. They initialize INFO to zero and
report illegal UPLO/N/LDA through negative INFO and XERBLA. Checked preflight
rejects those illegal arguments before foreign entry. S/DTRTTP label A as an
input but mistakenly say "On exit" in its prose; the executable source only
reads A. The complex documentation correctly says "On entry". There is no
scaling, conjugation, diagonal normalization or numerical failure channel.

I/J loop terminals form N+1; K reaches N(N+1)/2 by increments. Pure arithmetic
checks bound these expressions, the actual native leading dimension and the
public packed descriptor's undivided N(N+1) product. The latter also bounds
dense and row-packed offsets. The existing neutral workspace validator limits
LP64 kScratch entries to INTEGER maximum, so dense-output staging has a
stronger ASC bound than native K. The pure count limits are N=65,535 for
TRTTP and N=46,340 for TPTTR under LP64, and N=3,037,000,499 before byte limits
under ILP64. Actual descriptors and aggregate workspace bytes impose further
bounds. Boundary tests use abstract counts and plan metadata, never invented
large backing arrays or oversized native calls.

## Public behavior

Both directions take a square dense view, a packed view of the same order,
and explicit triangle selection. Packed row/column layout follows the existing
Dense BLAS packed descriptor contract. Every diagonal element is stored;
complex imaginary diagonal components remain visible. Signed zeros,
subnormals, finite extremes, infinities and quiet NaNs are valid copied values.
Copies perform no finiteness filtering and return no warning just because a
copied value is nonfinite.

Queries inspect metadata only. Plans bind scalar, direction, order, triangle,
dense leading dimension, both layouts and provider identity. Metadata overlap
is rejected before report reset. Other preflight failures reset the report and
leave numeric arrays unchanged. Empty order is local, with absent native INFO.
Operands, metadata objects and workspace must be disjoint. Independent contexts,
plans, buffers, workspace and reports are reentrant.

Caller-provided live scalar arrays hold output staging in kScratch and row-input
conversion in kLayoutConversion. Only selected dense input cells are read. No
old caller output is read, and only selected dense output cells publish; unused
triangles and padding remain unchanged. Full native INFO is initialized to the
actual integer maximum. Any nonzero returned value is a provider defect and
staged output is not published. Negative argument decoding avoids negating the
minimum integer. Successful native return with INFO zero publishes the staged
copy; that protocol does not certify arbitrary provider writes or detect
omitted writes. Explicit omitted/wrong-output controls retain that limitation.
Calls allocate, transfer and synchronize nothing implicitly.

## Maintained checks

Twenty-two local test groups cover four scalars in mathematical, fidelity,
validation and concurrency modes; four scalar fault groups; pure count limits;
and the native ABI probe. Ordinary cases use eight orders from zero through 33,
three padding values, both triangles, both directions, four layout pairs and
sixteen fixture seeds. Each scalar mathematical/fidelity group has 6,144 cases
and reuses each plan twice. Independent closed-form packed indexing supplies
the byte oracle. Fidelity separately calls the real provider with column
storage. Entire caller arrays, output complements and workspace guards are
checked. Four concurrent threads use separate provider contexts and storage;
serial calls audit allocation.

Validation covers stale counts, triangle/layout changes, unknown triangle
enums, exact one-byte-short active regions, operand/workspace aliases, report
metadata preservation, and Device/Managed operand or workspace rejection.
Ten bounded fault modes cover full INFO omission, positive INFO, negative
argument positions, minimum INTEGER, partial-width zero, omitted output and
wrong output, in addition to an ordinary control. Each typed wrapper first
passes a real three-by-three baseline with independent expected values and
nonzero complex components. These return-value tests involve no intentional
crash, protected page or malformed foreign call.

The public installed consumer uses only public headers and `ASC::dense_lapack`.
Its 6,144 cases cover every scalar/direction/layout/triangle, eight orders,
three padding values and four fixture seeds with repeated plans. A prepared
installation can build the maintained consumer as follows:

```sh
cmake -S tests/dense_lapack/installed_lu -B /tmp/asc-storage-consumer \
  -DASCCpp_DIR=/path/to/relocated/lib/cmake/ASCCpp \
  -DASC_CPP_LAPACK_ROOT=/path/to/prepared-reference \
  -DTEST_RUNTIMES='/absolute/libgfortran.so|/absolute/libquadmath.so'
cmake --build /tmp/asc-storage-consumer --target installed_triangular_storage_conversion
ctest --test-dir /tmp/asc-storage-consumer --no-tests=error \
  -R '^installed_triangular_storage_conversion$' --output-on-failure
```

The exact prepared provider and runtime files remain separately installed.
This leaf changes no dependency, upstream algorithm or floating-point setting.
Deliberate-fault memory observation, instrumentation requiring the historical
ASLR workaround, remote/security checks and broader-platform admission remain
required and deferred under the user's current execution constraint. No
existing test is disabled, skipped or weakened, and no earlier failure is
waived. This is an implementation self-review, not owner approval.

## Local execution records

Raw records are under
`master-continuation-20260910-01/continuation-20260912-01/completion-execution-01/`.
Both `triangular-storage-native-01-*` prerequisite probes and the maintained
`native-02-*` probes pass 6,144 cases per ABI. The initial maintained build01
retains an incorrect test error-code identifier; build03 retains an invalid
default plan construction in the pure metadata test. These were corrected
using the existing error code and a real empty-query plan. Final initial04
passes all 22 groups on both ABIs.

All eight `triangular-storage-{static,shared}-{release,debug}-{abi}-01`
profiles pass 22/22: 176 passed, zero failed or skipped. The five maintained
translation units pass strict02 after direct includes, narrow enum storage,
index casts and splitting validation into smaller helpers. All original cases,
numeric checks and allocation assertions remain intact.

Four `triangular-storage-installed-02-*` packages install, relocate and run
the public consumer. The final `triangular-storage-consumer-final-03-*` copies
each pass 6,144 cases against those unchanged product packages. Consumer edits
propagate setup failures as ordinary error returns and bind the provider by
const reference; initial strict findings remain in installed-strict01/02.
The final consumer adds no private header or producer source include path.

Four normal/no-exception header checks, ten package-manifest checks and three
architecture checks pass in `triangular-storage-integration-01`. The source
and all four installed 167-header surfaces pass. Both integer ABIs add sixteen
exports and remove zero. The actual CI selector passes with 1,869 tests,
adding exactly 22 conversion groups and two header checks, with no prior test
removed.

These are ordinary Release/Debug configurations. Fortran, BLAS and runtime
libraries remain uninstrumented. Shared wrapped tests bind the exact production
PIC objects into a test DSO; installed consumers use the production library.
The deferred observations and broader release gates above remain open.
