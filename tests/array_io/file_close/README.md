These maintained first-party tests preserve the stabilization close-failure
checks. They link only installed exported targets. The 60 ASC text/binary cases
cover all 12 wire scalar codes, nonempty rank-two Dense left/right and
COO/CSR/CSC. Two additional Matrix Market cases cover f64 general Dense array
and COO coordinate. Binary cleanup seeds are writer-produced; independent
binary format fixtures are tested separately.

Prepare fixtures in a fresh external directory, then configure this project
twice with `ASC_FILE_CLOSE_COMPONENT=dense` and `sparse`, respectively:

```sh
python3 -B tools/array_io/prepare_file_close_cases.py --output-dir <scratch>/fixtures
cmake -S tests/array_io/file_close -B <scratch>/dense \
  -DASCCpp_DIR=<relocated-prefix>/lib/cmake/ASCCpp \
  -DASC_FILE_CLOSE_COMPONENT=dense \
  -DASC_FILE_CLOSE_FIXTURES=<scratch>/fixtures
cmake --build <scratch>/dense --parallel 2
ctest --test-dir <scratch>/dense --verbose --no-tests=error
```

GNU wrapping calls the real `fclose`/`fflush` before injecting EIO/ENOSPC.
Assertions require close-only rejection, primary-error preservation with
secondary cleanup diagnostics, exactly one close, and leak-free disposal.
No atomic replacement, file rollback or durability is claimed.

The adapter explicitly rejects non-Linux and shared-Core configurations: GNU
`--wrap` cannot intercept calls inside a shared library. Those required
platform/linkage acceptance cells remain open; this project does not register
a skipped test or return success for an unsupported configuration. Empty/higher
ranks and other Matrix Market field/symmetry cases also need their separate
contract-derived mapping. This limitation is unchanged from the retained
external adapter, not permission to waive the remaining acceptance gates.
