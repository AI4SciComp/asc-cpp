# TPTRI/TPTRS source and ownership self-review

Scope: eight S/D/C/Z TPTRI/TPTRS routes at Reference-LAPACK 3.12.1 commit
6ec7f2bc4ecf4c4a93496aa2fa519575bc0e39ca, GNU11 Linux x86_64 static LP64
and true ILP64. This is a self-review, with no independent approval or complete
program capability credit. TPCON/TPRFS remain separate required work.

## Actual call closure and ABI

Each actual archive closure contains22 objects: eight roots, four TPMV,
four TPSV, four SCAL, LSAME and XERBLA. Actual object/source hashes are in
basic-closure-01/source-closure-{abi}.json. There are no writable static
symbols and no remaining external imports after the conditional XERBLA edge.
The exclusion is justified below; merely finding the symbols is not proof.
The 32 actual GNU compiler-emitted prototypes for all16 inventoried packed
routines were compared with installed declarations. The eight basic calls
also have a permanent complete function-type regression test: exact signed
native primitive int/long, real/complex representation, void return, pointer
depth/order and two (TPTRI) or three (TPTRS) trailing size_t character lengths.
These observations do not establish another compiler, platform or ABI.

Complete fixed-format statement/label comparisons establish S/D and C/Z
precision counterparts for the four basic stems and transitive BLAS TPMV,
TPSV and SCAL. The BLAS comparisons preserve197 complex TPMV/TPSV statements,
163 real ones,33 real SCAL and20 complex SCAL statements. SCAL argument
identifier renaming is explicit; no executable ordering/branch is erased.
The broader14-pair comparison additionally covers TPCON/TPRFS/LACN2/LANTP/
LATPS but does not admit their algorithms. Its failed first LANTP conversion
comparison is preserved; the reviewed second normalization is recorded.

## Integer and address bounds

Let n be the matrix order, p=n(n+1)/2 and M the actual signed native maximum.
The ASC packed descriptor independently checks n+1, the product n(n+1),
element-byte count, alignment and the complete real backing range. Adapter
packed offsets are within p and use products bounded by that checked product.
Row/column conversion uses caller p live scalar slots; no n-by-n dense array
is constructed. Row RHS conversion separately uses n*nrhs live scalar slots.
The common workspace validator checks typed alignment, capacity, total bytes,
aliasing and accessible placement before any packing or foreign entry.

TPTRI upper uses p+1 cursors; lower evaluates n*(n+1) before dividing by2.
The lower product bound also covers JJ+n before subtraction, the first JC
expression, and terminal JC=-n. TPTRI calls TPMV with suborder at most n-1,
INCX=1, valid UPLO, TRANS=N and validated DIAG. Upper AP matrix and current
column vector are adjacent disjoint ranges; lower trailing AP starts after
the current vector. Subroutine packed indices and SCAL contiguous vectors
remain inside the descriptor/packed workspace.

TPTRS validates native n,nrhs,ldb and ldb>=max(1,n). Its nonunit singularity
scan executes even with nrhs=0. The lower JC=JC+N-INFO+1 expression requires
p+n<=M, including its intermediate addition. Otherwise p+1 suffices for the
scan and forward TPSV paths. Descending TPSV paths (upper N, lower T/C)
actually evaluate n*(n+1), so that stronger bound is mandatory when nrhs>0.
TPSV receives INCX=1 and valid UPLO/TRANS/DIAG. No general-stride branch is
reachable. The native RHS DO terminal nrhs+1 is protected by nrhs<M. Each
RHS lies in the descriptor's checked reachable span, including original LDB;
row-layout calls use packed LDB=n. ASC strides and actual foreign dimensions
remain separate parts of the query identity.

Both source-ordered scalar loop controls terminate within the protected
packed bounds. Real SCAL's MOD5 cleanup and step5 loop end at suborder+1;
its final unrolled access is suborder. Complex SCAL uses unit-step control
through suborder+1. Every SCAL suborder is at most n-1. This proves the extra
loop terminals without pretending the small virtual integer oracle emulates
all native BLAS constants. The independent oracle walks source expressions
under virtual maxima3..600 and separate actual LP64/ILP64 boundary values;
it reports1465114 successful checks without fabricated backing spans.

Zero-order operations complete locally with absent INFO. For n>0,nrhs=0,
unit TPTRS does not read A or B and reaches no TPSV; the adapter avoids A
packing and supplies a live scalar dummy B. Nonunit row-packed zero-RHS
copies diagonal entries only, preserving the source singularity scan.

## Diagnostics, mutation, finite control and memory

All root and transitive BLAS error flags/dimensions/increments are validated
as above; LSAME receives fixed valid characters. Thus XERBLA's diagnostic
Fortran I/O/STOP path is excluded for admitted calls. No handler is replaced.
All eight root INFO locals start at the native signed minimum. Source roots
write0 before numerical work and return positive INFO only for a zero
nonunit diagonal found before any numerical destination update. Positive
1..n maps to singular/unchanged with a zero-based index. Unit-positive,
negative, unwritten and impossible INFO map to provider defect/unusable;
row-packed publication is withheld. Direct provider writes are not undone.

Complete D/Z root and transitive TPMV/TPSV/SCAL execution bodies and LSAME
were reviewed; the precision comparisons carry their control structure to
S/C. All loops are integer bounded, independently of floating values. There
is no iterative convergence state, value-dependent unbounded loop, variable
length local array, source allocation or reachable I/O. The exact basic
archive closure has no external allocator import or writable static state.
Complex division is emitted within these actual objects, with no unresolved
helper import. This supports finite control and allocation-free source paths
on the pinned build; it is not a universal finite-result guarantee.

The adapter reads only selected nonunit entries and never reads or publishes
stored unit diagonals. TPTRS leaves packed A unchanged. All numeric/workspace
ranges and live provider/plan/workspace/report metadata are checked before
mutation; detected metadata alias preserves the report, other rejection resets
it. Initial/repeated allocation probes and sentinel checks are scoped test
evidence, not full foreign-runtime instrumentation. ASC-only ASan/UBSan leaves
the GNU-built foreign archives unsanitized. No full finiteness/conditioning,
all-platform memory, concurrency-stress or complete P05 acceptance claim is
made. Full cross-cutting evidence and missing required operations remain open.

## Bound identities

- `inventory.json` SHA256 `5b5cc75f4f8e99ce4e76e871ba164a95fcc2e6b5690485569eb7ccb6a18b43e7`.
- `precision-review-02.json` SHA256 `7190b8f8887c4d789145de6c743754d5a6abd163d30ecba210b4ec25976524d0`.
- `precision-blas-01.json` SHA256 `28ab6e406c70e34adb56959e66073b4886f6412b883f30a7b386059defcc7b90`.
- `basic-closure-01/source-closure-lp64.json` SHA256 `fbb3aa2acf1b6d843d5204879b91f24b1b6178fb52030e7b4840185df8a86520`.
- `basic-closure-01/source-closure-ilp64.json` SHA256 `b1ea88fc7682be5c35c2dad756625e0473aedf5722e57a41fe07143f6d89e1c0`.
- Candidate03 `include/asc/dense/providers/lapack_triangular_packed.h` SHA256 `332ff03b173be914118c1f535563217eb69b38a598f715fac0ad8848b55a607b`.
- Candidate03 `src/dense/lapack/internal_packed_triangular.h` SHA256 `d02964eed42516c23c16bec947bd55299f10dd560e88971268d16735a2e9fa59`.
- Candidate03 `src/dense/lapack/internal_packed_triangular_counts.h` SHA256 `8e95d9beb68529ace4af64c6c16a53b58b2b3c43d728a8baf381f4765b89cb83`.
- Candidate03 `src/dense/lapack/reference_packed_triangular.cc` SHA256 `907e4b71fe06aa09b777a30247e22c34841efb131cad5b326fb4ea855e6987f1`.

## Root registration checkpoint

Candidate06 audit39f10144406404999481f7078cc0c084c781048453d8de2f8c061b188b0d2522
binds144 raw candidate records and six current4/4 lanes,zero skips. Each
runs6496 numerical workflows/737728 assertions,12480 failure/preflight
profiles/87744 assertions,1465114 pure count checks and8 exact signatures.
Four changed-test strict checks pass;12 earlier strict and4 public-header
checks apply to unchanged bytes. All48 current adapter/test objects and
the explicitly reused61-TU production libraries per lane are rehashed.
The source review's phrase "four basic stems" refers to the four basic
precision pairs: two stems,TPTRI/TPTRS. No extra routine is credited.

Eight basic routes are now registered as partial v17 capability,with113
public headers and21 installed families. Current root full/Debug/sanitizer/
installed/source-policy/documentation verification is pending. The program
remains302 partial Reference rows,1811 not_started,20 unverified native and
zero fully verified of2113 required. TPCON/TPRFS8 and all other P00-P11 work
remain required. Existing54 mathematical failures and130 missing XBLAS
routines remain visible.

## Executed root v17 integration

V17 GNU Release full suites and affected GNU Debug/ASC-only Clang19 sanitizer suites, both actual integer ABIs. All62 primary Core/Dense/provider TUs freshly compiled each lane,372 objects total. Full837 each:783 pass54 required mathematical failures;Debug46 each:38 pass8 required mathematical failures; ASC-only sanitizer25 each:17 pass8 required mathematical failures. Zero skips. All four relocated21-family packages,12 strict and6 static checks pass. Foreign archives unsanitized. TPTRI/TPTRS8 remains partial; no full normalized mode/routine/program verification or independent approval.

Audit SHA256 `8ab7e96c9c2da48c24199393e8d3136458ff116c69999e47689a57fafd31ea52`
binds42 original records and the exact frozen tree
`c18710718c4d9e8980d21141251e43d24613e79e`, archive
`9a9eb88659e84d5c409464a54ff7de82747b28922f7a25b4d34472e07dab9c50`.
The completion record rehashes the original records, all source files,
production objects/dependencies, installed consumers and both providers.
Doxygen covers113/113 headers and2100 documented public members,zero warnings.
All four packed basic tests pass in each lane with6496 public workflows/
737728 assertions,12480 fault profiles/87744 assertions,1465114 pure count
checks and8 exact native signatures. No failed mathematics is waived.

## Documentation reconciliation

The three final documentation/contract prose files passed six fresh checks.
Audit `bff845b1e7e68feb8e6b441e2fbd951b095ce26960a451b18a253012219b19f9` binds the
new source snapshot and rehashes every earlier numerical compiler input and
production artifact unchanged. The numerical and documentation trees remain
separate evidence identities. The2399-record ledger preserves the43 numerical
completion records and6 new documentation records. A first state-update helper
failed after appending those43 records (duplicate scope keyword); a fresh helper
verified the append before completing state and documentation. Both logs remain
external. No source, test, or prior result was replaced.
