# Milestone 0 foundation implementation review

Status: Foundation engineer self-review

Scope: `.clang-format`, `.clang-tidy`, and this review only

## Decisions

- Formatting derives from clang-format's Google C++ style, fixes the language
  mode at C++20, and retains the 80-column limit required by the runbook.
- Pointer and reference declarators bind to the type. Qualifier order is left
  unchanged so formatting cannot silently alter source meaning.
- Includes are case-sensitively sorted and regrouped using clang-format's
  Google categories. This supports related-header-first and direct-include
  practices without naming an optional SDK or provider.
- Braces are inserted for control statements, LLVM-style brace removal is
  disabled, and every formatted namespace receives a closing comment. Namespace
  contents and preprocessor directives remain unindented.
- Static analysis enables Clang's analyzer plus broad bug-prone, Google,
  concurrency, performance, portability, and readability families. Selected
  miscellaneous checks cover include hygiene, headers, linkage, special
  functions, and redundant constructs. Findings from enabled checks are errors.
- Modernization checks are an explicit C++20-safe allowlist. In particular,
  checks proposing C++23 facilities such as `std::print` are not enabled.
- Selected C++ Core Guidelines checks cover narrowing, slicing, and special
  member functions. Broad guideline families that conflict with low-level
  scientific storage/provider work are not enabled wholesale.
- High-noise checks for easily swappable parameters, TODO issue syntax,
  cognitive-complexity thresholds, identifier length, and magic numbers are
  disabled. These concerns remain review responsibilities where material.
- Project diagnostics are limited to `include/asc`, `src`, and `tests`; system,
  generated, and optional SDK headers are not analyzed as project headers.
- Mechanically unambiguous ADR 0003 naming is configured for types, constants,
  namespaces, variables, parameters, and non-public data members. Function
  naming remains a review check because the policy distinguishes PascalCase
  ordinary functions from snake_case accessors.
- No production file, target, package behavior, provider discovery, or
  dependency is introduced by this change.

## Policy basis

- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
  consulted 2026-07-26, targets C++20 and requires self-contained `.h` headers,
  direct includes, and no non-standard extensions.
- LLVM's published clang-tidy 18.1 check index was used to verify that every
  explicitly named check is present in the hosted Clang 18 baseline.
- Approved ADR 0003 fixes namespace, file-extension, header, and source policy.
  ADR 0004 informs `nodiscard` and conservative `noexcept` analysis. ADR 0018
  prohibits a Milestone 0 production target or release claim.

## Host inventory

The following tools were discoverable during implementation:

| Tool | Result |
| --- | --- |
| `clang-format-19 --version` | Ubuntu clang-format 19.0.0 |
| `cmake --version` | 4.1.2 |
| `g++ --version` | GCC 11.4.0 |

The following tools were not available through `PATH` or the installed LLVM
directories inspected:

- `clang-tidy` (including version-suffixed binaries 16 through 20);
- `clang++`;
- `ninja`.

Their absence is an environment limitation, not a pass or skip claim for the
future CI matrix.

## Checks executed on this host

All commands below passed:

```sh
clang-format-19 --style=file --assume-filename=probe.cc \
  --dump-config >/dev/null
clang-format-19 --style=file --assume-filename=probe.h \
  --dump-config >/dev/null
```

A representative stdin-only `src/core/status.cc` formatting smoke test also
passed. It verified related-header-first grouping, C-system and C++-library
separation, brace insertion, and `}  // namespace asc`.

```sh
python3 -c \
  'import yaml; assert isinstance(yaml.safe_load(open(".clang-tidy")), dict)'
```

The stronger YAML assertion used during final validation also verified all
required top-level keys, `WarningsAsErrors: '*'`, `SystemHeaders: false`, and
string-valued check options.

Every explicitly named enabled or disabled check was compared with LLVM's
published 18.1 inventory. `misc-use-internal-linkage`, which first appears
after that baseline, is intentionally not enabled. Category wildcards select
only checks implemented by the executing clang-tidy version.

```sh
! rg -n '[[:blank:]]+$' .clang-format .clang-tidy \
  docs/development/asc-cpp-m0-foundation/foundation-implementation-review.md
awk 'length($0) > 80 {bad = 1} END {exit bad}' \
  .clang-format .clang-tidy \
  docs/development/asc-cpp-m0-foundation/foundation-implementation-review.md
! rg -n 'modernize-use-std-(format|print)' .clang-tidy
```

A stdin-only GCC smoke test using `-std=c++20 -pedantic-errors -fsyntax-only`
passed for a C++20 concept and `static_assert`. No repository source or
temporary test file was created.

## Frozen-contract self-review

- Strict C++20 formatting and analysis: satisfied by Google base formatting,
  `Standard: c++20`, the explicit C++20 modernization allowlist, and
  warnings-as-errors.
- Current Google style: satisfied by `BasedOnStyle: Google` plus the
  repository-specific ADR 0003 constraints.
- Future self-contained `.h` and `.cc` code: supported by related-header and
  include regrouping, scoped project-header analysis, include-hygiene checks,
  naming checks, and no provider-specific assumptions. Header
  self-containment itself remains a compile-contract test.
- No C++23 or C++20 modules: no C++23 check is enabled and the formatter parses
  C++20. The files add no source or module declaration.
- No optional SDK requirement: no provider name, SDK include, compiler flag, or
  discovery action is present.
- No production code or target: satisfied; only the two policy files and this
  review are added.
- No release or compatibility claim: satisfied; no version, target, component,
  tag, or publication state is changed.
- Assigned ownership only: satisfied; no file outside the foundation engineer
  write set is edited.

## Deferred verification

- Execute `clang-tidy` against the generated C++20 compilation database when
  the tool and production/test translation units exist.
- Run format and tidy gates with each compiler/platform in the approved CI
  matrix.
- Compile every future public header in isolation to verify include guards,
  direct includes, provider neutrality, and C++20 self-containment.
