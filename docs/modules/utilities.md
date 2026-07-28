# Utilities module

`ASC::utilities` provides two independent facilities: transactional
command-line configuration and monotonic interval timing. It is a compiled
library with one direct ASC dependency, `ASC::core`, and no external
dependency.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS utilities)
target_link_libraries(my_target PRIVATE ASC::utilities)
```

```cpp
#include <asc/utilities.h>
```

The umbrella includes `<asc/utilities/command_line.h>` and
`<asc/utilities/timer.h>`. Utilities has no dependency on Expression, Random,
Dense, Sparse, a provider SDK, or a global diagnostic stream.

## Command-line option table

`CommandLineOption` describes:

- one nonempty long name made from ASCII letters, digits, `-`, and `_`,
  without leading `--`;
- an optional one-character ASCII letter or digit without leading `-`;
- a JSON Pointer destination path into a Core `ConfigurationSchema`;
- a value label; and
- caller-owned help content copied into the parser.

`CommandLineParser::Create` takes its `ConfigurationSchema` by value and
copies the supplied option span. The resulting parser therefore owns both its
schema and option table. Long names, short names, and destination paths are
each unique. Invalid names, duplicate aliases, duplicate destinations, absent
schema paths, and otherwise inconsistent declarations fail before a parser is
published.

An option's accepted type comes from its schema destination. Milestone 2
supports these leaf alternatives:

| Schema type | Accepted command-line value |
| --- | --- |
| bool | explicit true/false or a flag spelling |
| signed 64-bit integer | complete locale-independent decimal token |
| unsigned 64-bit integer | complete locale-independent decimal token |
| double | complete locale-independent floating token |
| UTF-8 string | one validated UTF-8 token |

Null, list, and object destinations return `kUnsupported`. Numeric types are
not converted into one another.

## Token grammar

Long value options accept both forms:

```text
--name=value
--name value
```

Short value options accept exactly:

```text
-n value
```

Short clusters and attached short values are unsupported. A token expected as
a value is consumed even when it starts with `-`, so negative signed integers
and doubles are ordinary values.

Boolean options accept:

```text
--flag
--no-flag
--flag=true
--flag=false
-f
```

Long-form negation is the only negation spelling. A destination may be
supplied only once at command-line precedence, including through aliases or
opposite boolean spellings. Repetition is a duplicate scalar error rather
than last-value-wins behavior.

`--` ends option recognition. The parser returns subsequent tokens in order as
positional arguments. Bare tokens encountered during ordinary parsing are
also positional. Unknown options, malformed option tokens, missing values,
invalid UTF-8, incomplete numeric conversion, and numeric overflow fail.

`Parse` receives only the argument tokens to interpret; it does not give the
first element executable-name semantics.

```cpp
asc::ConfigurationSchema schema(asc::ConfigurationValueType::kObject);
asc::ConfigurationSchema count(
    asc::ConfigurationValueType::kSignedInteger);
count.SetRequired(true);
if (!schema.AddField("count", std::move(count)).ok()) {
  return 1;
}

const std::array<asc::CommandLineOption, 1> options{{
    {.long_name = "count",
     .short_name = 'n',
     .destination = "/count",
     .value_name = "INTEGER",
     .help = "Number of samples"},
}};
auto parser = asc::CommandLineParser::Create(std::move(schema), options);
if (!parser.ok()) {
  return 1;
}

const std::array<std::string_view, 3> arguments{
    "--count", "-3", "input.dat"};
auto parsed = parser->Parse(arguments);
if (!parsed.ok() ||
    parsed->positional_arguments != std::vector<std::string>{"input.dat"}) {
  return 1;
}
```

## Transaction, precedence, and origins

Parsing builds a fresh candidate tree and then validates the complete tree
with the Core schema. The parse result is published only after conversion,
duplicate checks, schema validation, defaults, bounds, and unknown-key checks
all succeed. A failure exposes no partial `Configuration` or partial
positional list.

This milestone implements:

```text
schema defaults < command line
```

It does not implement the approved future file layer or explicit programmatic
merge layer. Non-default parsed leaves have
`ConfigurationOriginKind::kCommandLine`; the zero-based index of the option
token is retained as the origin location, independent of attached or separated
value spelling. Schema defaults keep `kDefault` origin.

Core JSON Pointer rules apply to destinations and metadata lookup: the empty
path denotes the root, `/field` descends through an object, list tokens are
decimal indices, and `~0` and `~1` escape `~` and `/`.

## Help rendering

`RenderHelp` returns deterministic caller-owned text from the validated option
table. It does not print, inspect terminal width, mutate parse state, or
implement implicit `--help` control flow. Applications choose whether and
where to display it.

## Parser ownership and concurrency

The parser owns its schema and validated option descriptions. Parse results
own their configuration and positional strings. Any non-owning references
exposed by a Core `ConfigurationValue` remain limited by the owning result's
lifetime.

After creation the parser exposes only const operations. Concurrent const
parses and help rendering read immutable parser state and construct
caller-owned results; the parser must not be moved or destroyed concurrently.
Mutable access to a returned configuration or positional vector remains the
caller's responsibility. There is no Utilities-owned shared mutable registry.

## Monotonic timer

`Timer` uses `std::chrono::steady_clock` and has three explicit states:

| Current state | Operation | Result |
| --- | --- | --- |
| Empty | `Start` | Starts an interval; state becomes running |
| Stopped | `Start` | Starts another interval; state becomes running |
| Running | `Start` | `kInvalidState` |
| Running | `Stop` | Records and returns a representable non-negative interval |
| Empty or stopped | `Stop` | `kInvalidState` |
| Any | `Reset` | Discards all state and samples; state becomes empty |

`Elapsed` returns the completed total plus the current interval while running.
`Last` and `Average` return `kInvalidState` until at least one interval has
completed. `Average` uses duration arithmetic rather than floating conversion.
The sample count increments exactly once per successful `Stop`.

`Stop` checks interval subtraction, accumulated-duration addition, and sample
count conversion before changing any published timer state. A backwards clock
observation reports `kInvalidState`; an unrepresentable interval, total, or
count reports `kOverflow`. Either failure leaves the completed total, last
interval, sample count, and running state unchanged. `Average` also reports
`kOverflow` if its divisor cannot be represented by the clock duration.
Because the historical `Elapsed` API returns a `Duration` rather than a
`Result`, a running clock regression or elapsed-total overflow saturates to
`Duration::max()` instead of wrapping.

`Timer::Duration` is `std::chrono::steady_clock::duration`; Utilities does not
silently convert it to seconds or floating point.

```cpp
asc::Timer timer;
if (!timer.Start().ok()) {
  return 1;
}
auto interval = timer.Stop();
if (!interval.ok() || *interval < asc::Timer::Duration::zero() ||
    timer.sample_count() != 1) {
  return 1;
}
```

The timer has no wall-clock meaning, fixed sample buffer, global registry,
logging side effect, thread synchronization, provider event, or GPU timing
capability. A timer object must not be mutated and queried concurrently
without caller synchronization. Independent timers share no Utilities-owned
mutable state.

## Deliberately absent

Milestone 2 Utilities does not provide:

- a local configuration-file syntax or parser;
- environment-variable or response-file input;
- list repetition or short-option clusters;
- implicit programmatic merging;
- terminal output or global help handling;
- tracing, telemetry, profiling orchestration, or logging; or
- GPU event timing.

The [frozen Milestone 2 contract][contract] is authoritative if a historical
page appears to imply a broader parser or timer.

[contract]: ../development/asc-cpp-m2-independent-foundations/milestone-contract.md
