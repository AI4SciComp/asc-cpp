# Band20 unwritten-INFO correction

This is an additive, locally executed correction to the completed recovered band-expert candidate03 handoff. The preserved worktree and all earlier candidates/logs are unchanged. The new source changes exactly five production statements, initializing each native INFO to `std::numeric_limits<lapack_int>::min()` after structural preflight, plus three new test/support files. Each production TU already directly includes `<limits>`. No public declaration, workspace layout, provider pin, numerical algorithm, existing test or required mathematical gate changes.

The old adapters falsely accepted a successful real provider call whose INFO output was not published: `control02` uses exact final test bytes with the original five zero initializers. Both LP64 and true ILP64 execute 1,152 successful native calls, including 576 ordinary controls and 576 omissions; all omissions falsely return success. All four scalar test processes fail on each ABI. These failures are preserved as executed regression controls, not XFAIL tests.

The new wrapper endpoints use the exact existing pinned prototypes. Every wrapper calls its real native entry with a live native-sized local INFO. Only the selected routine's INFO publication is omitted; PBSVX's nested PBCON/PBRFS calls retain their real INFO. Counters verify exactly one selected entry call and its native INFO=0. This is injected output-omission evidence with real native execution, not proof of every upstream mathematical result.

The corrected source returns `kProvider`, retains the minimum native INFO, reports `kProviderArgument/kUnusable`, and handles minimum signed values without negation overflow. LP64 records argument 2147483648; true ILP64 leaves native_argument absent. Packed band factors/solutions stay unpublished on defect. Direct native output mutation and directly bound scale/statistic/error outputs remain observable under the unusable report. Tests verify independent layouts, both triangles, all 20 scalar routines and PBSVX N/E/F, caller/workspace padding and zero operation allocations. Ordinary controls independently check factor reconstruction/solutions where used. E runs use the real routine's unequilibrated result for this well-scaled fixture; the preserved existing fault suite still tests E/Y publication and all minimum-INFO failures.

Final candidate02 completed eight complete focused lanes: LP64/true ILP64 × GCC Debug/GCC Release/Clang ASan+UBSan/GCC Release libc-allocation observation. Each selects and executes 88 tests, passes 78 and fails the same ten required mathematical gates, with zero skips. Across eight lanes the four new scalar tests pass 32/32; 4,608 ordinary native-success controls and 4,608 withheld-INFO rejections execute. The ASC adapters/test callers are instrumented in the ASan lanes; the preserved foreign provider libraries are not newly instrumented.

The required failures remain complex lower scaled KD=1 PBCON (RCOND .25 instead of 1/9), all-scalar PBRFS tiny FERR=Inf, and all-scalar PBSVX N/F tiny RCOND=0/FERR=Inf plus E scaling overflow/X=0/NaN. Raw full CTest commands exit 8. Nothing is excluded, waived, relabelled inapplicable, or substituted with fidelity success. Full Reference coverage remains 2,113 required routines and incomplete.

Strict passes cover all seven affected TUs on both ABIs. Twelve production/wrapper checks were executed in candidate01 and are reused only after exact TU/header/dependency equality proof; the final test TU has fresh candidate02 checks on both ABIs. The earlier test include/nested-conditional strict failures remain frozen. All eight affected files pass format. The final ledger verifies all raw record artifacts, exact normal/omission profile counts, every failed test name, 37 external dependency hashes and compiled source dependencies.

This self-review checks the defect reported independently by root. The artifact is a direct-link diagnostic handoff. Actual integration, shared registries, installed consumers and platform release evidence belong to root and have not been claimed here. The prior integrated reference_cholesky_band.cc zero seeds at lines 367/421 were reported to root and left unchanged under the ownership restriction.

Integration files are exactly `changed-code.sha256`. Add `band_unwritten_info_test.cc` plus `band_unwritten_info_support.cc`, the existing allocation audit/probe and normal-return guard to the provider test build. Register s/d/c/z arguments. Link `--wrap=` for each s/d/c/z × pbsv/pbequ/pbcon/pbrfs/pbsvx exact raw symbol ending in `_`, alongside the existing allocation wrappers. Keep all existing test registrations and mathematical gates enabled. The standalone diagnostic registration is in this candidate's CMakeLists.txt.

Reproduction in a NEW external candidate directory copied from this source/harness:
`python3 run_checks.py <new-candidate-name> lanes`

Audit this completed candidate without overwriting the ledger:
`python3 audit_handoff.py --output candidate02/verification-ledger-02.json`

Native acceptance work is preserved separately in native-mode-evidence-a0a1e15-01 and resumes independently.
