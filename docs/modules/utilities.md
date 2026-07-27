# Utilities module

`ASC::utilities` is the provider-free C++20 convenience component in the
unreleased ASCCpp `0.9.0` candidate. It owns transactional command-line
configuration parsing and monotonic elapsed-time measurement.

It does not own the recursive configuration model, which remains in
`ASC::core`. It also contains no local configuration-file parser, environment
or response-file input, numerical storage, expression operation, random
facility, provider adapter, or GPU code.

## Build and dependency contract

```text
build target:     asc_utilities
build-tree alias: ASC::utilities
installed target: ASC::utilities
direct ASC deps:  ASC::core
external deps:    none
```

The library follows `BUILD_SHARED_LIBS`. A consumer requests only this
component:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS utilities)
target_link_libraries(my_target PRIVATE ASC::utilities)
```

`ASC::core` is loaded transitively. `ASC::expression`, `ASC::random`, dense,
sparse, the aggregate, and every provider target remain absent from an
isolated utilities consumer.

## Public headers

| Header | Contract |
| --- | --- |
| `<asc/utilities.h>` | complete Milestone 2 utilities surface |
| `<asc/utilities/command_line.h>` | option schema, parser, parse result, and deterministic help |
| `<asc/utilities/timer.h>` | monotonic accumulated timer |
| `<asc/utilities/export.h>` | shared-library symbol visibility |

The headers are self-contained. All supported declarations are directly in
`namespace asc`.

## Command-line option declaration

`CommandLineOption` declares:

- one long option name without leading dashes;
- an optional one-character short name;
- a JSON Pointer path naming one configuration-schema leaf;
- a caller-facing value label; and
- caller-owned help wording copied into the validated parser.

`CommandLineParser::Create` requires a schema whose root is
`ConfigurationValueType::kObject` and validates the complete table before
returning a parser. A scalar, list, or null root is rejected rather than
creating an ambiguous configuration when no options are present; the failure
code is `ErrorCode::kInvalidArgument`. Long names, short names, and destination
paths must each be unique. Names use the frozen ASCII option-name grammar.
Each destination must resolve to a schema leaf of one supported type:

```text
bool, signed 64-bit integer, unsigned 64-bit integer, double, UTF-8 string
```

Null, list, and object destinations return `ErrorCode::kUnsupported`.
Malformed names, duplicates, missing schema paths, and incompatible
declarations fail before a parser is published.

The parser owns its validated schema and option table. Callers may destroy the
declaration values after successful creation. The references returned by
`schema()` and `options()` borrow the parser and must not outlive it.

`CommandLineParser` has value copy/move semantics. Copying deep-copies the
recursive schema, options, and owned strings and may allocate in proportion to
that content. Moving transfers the owned content; use a moved-from parser only
for assignment or destruction.

## Command-line example

`Parse` receives the tokens to interpret. An application using `argc`/`argv`
normally passes the sequence beginning at `argv[1]`; the parser does not
special-case an executable name. Tokens containing text use UTF-8 bytes.
Windows applications that start from native wide `wchar_t` arguments must
perform an explicit checked UTF-8 conversion before calling this narrow-byte
API; the parser does not guess or use the active Windows code page.

```cpp
#include <asc/utilities.h>

#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

int main() {
  asc::ConfigurationSchema schema(asc::ConfigurationValueType::kObject);
  asc::ConfigurationSchema steps(asc::ConfigurationValueType::kSignedInteger);
  steps.SetRequired(true);
  if (!schema.AddField("steps", std::move(steps)).ok()) {
    return 1;
  }

  std::vector<asc::CommandLineOption> options;
  options.push_back(asc::CommandLineOption{
      .long_name = "steps",
      .short_name = std::optional<char>('n'),
      .configuration_path = "/steps",
      .value_name = "COUNT",
      .help = "Set the signed step count.",
  });

  auto parser =
      asc::CommandLineParser::Create(std::move(schema), std::move(options));
  if (!parser.ok()) {
    return 1;
  }

  constexpr std::array<std::string_view, 3> arguments = {"--steps", "-3",
                                                         "mesh.dat"};
  auto parsed = parser->Parse(arguments);
  if (!parsed.ok()) {
    return 1;
  }

  auto value = parsed->configuration.Find("/steps");
  if (!value.ok()) {
    return 1;
  }
  auto count = value->get().AsSignedInteger();
  if (!count.ok()) {
    return 1;
  }

  return *count == -3 && parsed->positional_arguments.size() == 1 ? 0 : 1;
}
```

The returned `CommandLineParseResult` owns both the validated
`Configuration` and copies of positional token bytes.

## Accepted command-line syntax

Long value options accept:

```text
--count=64
--count 64
```

Short value options accept exactly:

```text
-n 64
```

Short clusters and attached short values are unsupported. A token expected as
a value is consumed even when it begins with `-`, so negative signed integers
and floating-point values are ordinary values.

Boolean options accept:

```text
--enabled
--no-enabled
--enabled=true
--enabled=false
-e
```

Boolean negation is long-form only. Supplying the same destination more than
once is an error, including long/short aliases and positive/negative boolean
spellings.

`--` ends option recognition. Subsequent tokens and otherwise bare tokens are
returned in order as positional arguments. Milestone 2 does not interpret
positionals or subcommands.

Unknown options, malformed tokens, missing values, unsupported spellings,
duplicate destinations, invalid UTF-8 strings, incomplete numeric
conversions, and numeric overflow return a failed `Result`. Integer and
floating conversion is locale-independent and never coerces between schema
types. A value rejected by its leaf schema reports the configured constraint
or range along with argv and option context; the received value remains
redacted when the schema marks it sensitive. Diagnostic prose is not a stable
API.

## Transaction, validation, and origin

Each parse constructs a fresh recursive `ConfigurationValue`, validates the
complete tree against the core `ConfigurationSchema`, and publishes a
`CommandLineParseResult` only after success. On failure, no partial
configuration or positional result is returned, and the reusable parser has
no mutable committed invocation state to corrupt.

Schema defaults are inserted by core validation. Every non-default leaf
parsed from the command line receives
`ConfigurationOriginKind::kCommandLine`; its diagnostic location records the
zero-based token index. Sensitive schema values remain redacted by core
rendering and parser diagnostics.

The approved precedence is:

```text
schema default < approved files in command order < command line
               < explicit programmatic override
```

Milestone 2 implements only schema-default plus command-line input. There is
no implicit merge with an existing programmatic configuration.

`RenderHelp` returns deterministic caller-owned text. It performs no terminal
I/O and has no implicit `--help` control flow. The application chooses when and
where to display it.

## Monotonic timing

`Timer` uses `std::chrono::steady_clock` and has three explicit states:

| State | Meaning |
| --- | --- |
| `TimerState::kEmpty` | no active interval and no completed sample |
| `TimerState::kRunning` | one interval is in progress |
| `TimerState::kStopped` | at least one sample is complete |

`Start` is valid from empty or stopped. Calling it while running returns
`kInvalidState` without restarting the interval. `Stop` is valid only while
running; it records one non-negative interval, adds it to the total,
increments the sample count, transitions to stopped, and returns the interval.

`Elapsed` returns accumulated completed time plus the current interval while
running. The `total()` accessor returns completed time only. `Last` and
`Average` return `kInvalidState` until the first completed sample. Average uses
duration arithmetic. `Reset` is valid in every state, discards all samples and
any running interval, and returns to empty.

```cpp
#include <asc/utilities/timer.h>

int main() {
  asc::Timer timer;
  if (!timer.Start().ok()) {
    return 1;
  }

  auto interval = timer.Stop();
  if (!interval.ok()) {
    return 1;
  }

  auto average = timer.Average();
  return average.ok() && timer.state() == asc::TimerState::kStopped &&
                 timer.sample_count() == 1
             ? 0
             : 1;
}
```

Clock or accumulated-duration overflow returns an explicit error. A failed
`Stop` does not publish a sample or transition out of running state.

The timer owns fixed-size timing state and no sample buffer. Successful timing
state operations do not intentionally allocate; failed operations return core
status diagnostics, whose strings follow the core status-storage contract. The
timer performs no terminal output, logging, global registration, wall-clock
conversion, GPU event timing, or hidden synchronization. A timer instance is
not safe for concurrent mutation. Independent timers contain no shared mutable
state.

`Timer` has value copy/move semantics. Copying duplicates the current state,
completed summaries, and start time. Copying a running timer therefore creates
two independent timers whose in-progress intervals share the same historical
start point but whose later stops, starts, and resets mutate only their own
state.

A successfully created `CommandLineParser` is immutable. Concurrent `Parse`
and `RenderHelp` calls on one shared parser use only const parser state and
call-local working storage and are safe. Concurrent assignment or destruction
of that parser requires caller synchronization, as does mutation of an object
whose borrowed `schema()` or `options()` view is in use.

## Cost, failure, and scope summary

| Surface | Ownership | Failure and mutation | Cost |
| --- | --- | --- | --- |
| parser creation | returned parser owns schema/options/text | validates complete declarations before publication | linear in schema and option-table content, plus owned copies/allocations |
| command-line parse | returned result owns configuration and positionals | transactional; no partial result | scales with tokens, option/path lookup, conversion text, owned output, and complete configuration validation |
| help rendering | caller owns returned string | no I/O or process exit | linear in rendered option text |
| timer | value owns state and samples summary | invalid transitions return status | constant-time operations; no sample buffer |

No operation selects an execution provider, allocates numerical storage,
transfers memory, or synchronizes a device.

## Deferred work and provenance

No local-file format has been approved. Environment variables, response files,
option repetition/list append, short clustering, subcommand interpretation,
completion output, scoped-timer registry, statistics buffer, and built-in-array
wrapper are deferred rather than inferred.

The implementation is project-owned and follows the frozen
[Milestone 2 contract](../development/asc-cpp-m2-independent-foundations/milestone-contract.md).
MdeCpp is behavior and test-category evidence only; no MdeCpp/deleted asc-cpp
source, test, or literal corpus is copied.
