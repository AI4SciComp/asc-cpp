# P00 provider-free baseline

The pre-change Debug static and Release static baselines each configured and
built successfully and passed all **219 selected/executed tests, with zero
failures and zero skips** on 2026-09-07. This evidence applies to the frozen
baseline, not subsequent implementation changes. No LAPACK capability is
credited by these results.

## Frozen input and dependency

The source was frozen using `git archive` before implementation edits. The
original dirty checkout was not altered. All source snapshots, dependency
archives, builds, installed test prefixes, and logs are outside the repository.

| Identity | Value |
| --- | --- |
| implementation commit | `46412183b2ae86101b2361c52376a8db8efff264` |
| Git tree | `1661da575d25f1225b577e4f68f7c7c3d26650d9` |
| source tar SHA-256 | `d60756c9e52fa4f44d051b7d052abb822f6900d2d6773c47909c14bbd2a59f6a` |
| archived-source dirty diff | empty; the archive contains committed files only |
| ASCCMake version/commit | `0.1.0` / `8a7dcbad3a97267cce59810aff24de800a3497a7` |
| ASCCMake archive SHA-256 | `67765391bef06c6c9a1a0c43e934d0db7a9c52876e66e0cf644042eeb0c2a5c9` |

ASCCMake was obtained from the exact API archive URL in
[`docs/installation.md`](../../docs/installation.md), and its observed checksum
matched that document. Its extracted source is
`dependencies/asccmake-8a7dcbad/source` relative to the program evidence root.
No privileged installation was used.

The archived source preserves every public header, target definition, API
inventory, ABI baseline, contract, and Random test fixture. The supplemental
Random evidence records hash every archived file before and after execution,
including `docs/api-compatibility.md`, `docs/contracts/{dependency-manifest,
capability-manifest,blas-coverage,random-crosswalk}.yaml`, and the affected
Dense/Random headers and tests. Neither fixtures nor expected values changed.

## Configuration and commands

The host was Linux `6.18.33.2-microsoft-standard-WSL2`, x86_64. Both builds used
GNU C++ 11.4.0 (`Ubuntu 11.4.0-1ubuntu1~22.04.3`), libstdc++ release 11
(`__GLIBCXX__=20230528`), CMake/CTest 4.1.2, and Unix Makefiles. The checked-in
presets selected C++20, warnings as errors, testing and installation enabled,
static libraries, and CUDA disabled. Debug used `-g`; Release used
`-O3 -DNDEBUG`. The suite includes public-header compilation with exceptions
disabled. Ordinary recoverable failures retain the existing Status/Result
contract.

Enabled targets were the six modules, `random_dense`, `random_sparse`, and the
provider-free `cpp` aggregate. No LAPACK provider or LAPACK integer ABI was
selected. The nine-target inventory check passed. Installed public-header,
component, relocation, no-exception, BLAS, Random, provenance, and architecture
tests are included in the full 219-test selections.

Exact argument vectors and working directories are recorded in external
`baseline-46412183/index.json`; compiler/dependency identity is in its
`metadata.json`. The command pattern was:

```bash
cmake --preset test-debug -B "$BASELINE_ROOT/build-debug" \
  -DFETCHCONTENT_SOURCE_DIR_ASCCMAKE="$ASCCMAKE_SOURCE"
cmake --build "$BASELINE_ROOT/build-debug" --parallel 2
ctest --test-dir "$BASELINE_ROOT/build-debug" --no-tests=error \
  --output-on-failure --output-junit "$BASELINE_ROOT/logs/ctest-debug.xml"
```

Release used the corresponding `test-release` preset and `build-release`
directory. Each command's combined output was piped through its own `tee` log
with Bash `set -euo pipefail`; both complete pipelines exited zero. Explicit
`-B` prevented collisions with the user's existing preset build directories.

## Evidence index

All paths in this table are relative to `baseline-46412183/` under the external
program evidence root. The local absolute-path index and raw artifacts are
kept external; they need sanitization before public attachment.

| Evidence | Result | Record/artifacts |
| --- | --- | --- |
| Debug configure/build/full CTest | all command exits 0; 219 passed | `index.json`; `logs/{configure,build,ctest}-debug.log`; `logs/ctest-debug.xml` |
| Release configure/build/full CTest | all command exits 0; 219 passed | `index.json`; `logs/{configure,build,ctest}-release.log`; `logs/ctest-release.xml` |
| Random Dense Debug verbose regression | 5 passed, zero failed/skipped | `random-dense-debug/record.json`, `command.log`, `ctest.xml` |
| Random Dense Release verbose regression | 5 passed, zero failed/skipped | `random-dense-release/record.json`, `command.log`, `ctest.xml` |
| evidence-runner subprocess tests | 11 passed | `evidence-tool-tests/record.json`, `command.log` |

The Random selection was `^asc_cpp[.]random_dense[.]`: advanced samplers,
generation, QMC fill, thread partitioning, and allocation. The full baseline
also passed existing engine/distribution, Sparse adapters, and reproducibility
tests. Verbose outputs are preserved; silent successful tests are not described
as emitted numeric datasets.

## Evidence tooling and remaining lanes

[`run_evidence.py`](../../tools/lapack_io/run_evidence.py) executes one
argument vector without a shell, exclusively creates an external record
directory, hashes source inputs and logs, preserves subprocess exits, and
checks actual CTest JUnit cases. Its CTest mode enforces `--no-tests=error` and
rejects skipped tests even when CTest itself exits zero. Source changes during
execution invalidate the record. Arbitrary commands have null test counts;
they do not become numerical test evidence.

Independent subprocess tests cover success, failed commands, unusable logging
paths, overwrite refusal, in-source artifacts, changing source inputs, mixed
CTest outcomes, zero tests, and skips with a successful CTest process exit.
Python 3.12.4, Black 24.4.2 (80 columns), and Pylint 2.16.2 were used; Pylint
reported no findings on the final files. The live
[Google Python guide](https://google.github.io/styleguide/pyguide.html) was
retrieved on 2026-09-07; its external HTML snapshot SHA-256 is
`9b02fa0d1aa05bfc8a4b5a95d3594665124f7a0414c82a69359f4a0b2f65e1c0`.

No baseline configure/build/test failure remains. Full shared, sanitizer,
strict Doxygen, full Clang, Windows/MSVC, macOS/AppleClang, provider, and GPU
lanes are not established by this baseline. Clang 19 and clang-format 19 are
available; the repository-pinned Clang 18 formatter/tidy were not initially
on PATH. The full baseline includes a Clang compile/object observation, not a
full Clang build. Doxygen 1.9.8 is present, but its strict documentation lane
was not run as part of this bounded P00 baseline.
