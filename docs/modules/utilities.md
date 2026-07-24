# Utilities module

`ASC::utilities` provides lightweight configuration, command-line, and timing
facilities. Utilities Milestone 1 (M1) corrects inherited parsing and timer
hazards while preserving familiar pre-1.0 class names.

| Surface | M1 status | Intended use |
| --- | --- | --- |
| `ConfigValue` and status-oriented `ConfigParser` operations | Canonical M1 | New configuration code |
| `Option`, `Variable<T>`, `Switch`, and status-oriented `OptionParser` operations | Canonical M1 through `<asc/utilities/cli.h>` | New command-line code |
| Hardened `Timer` | Canonical M1 | New timing code |
| Existing void/value parser wrappers | Compatibility M1 | Migration of existing callers |
| `<asc/utilities/optparser.h>` | Forwarding compatibility header | Existing include spelling |
| Diagnostics and tracing | Deferred | Not implemented in M1 |

Use the component umbrella and minimal target:

```cpp
#include <asc/utilities.h>
```

```cmake
find_package(ASCCpp REQUIRED COMPONENTS utilities)
target_link_libraries(my_target PRIVATE ASC::utilities)
```

The canonical Utilities implementation depends only on the C++ standard
library and stable Core configuration, types, status/result, and contracts. It
does not depend on numerical arrays, execution backends, provider SDKs, or a
global output/error policy.

The installed utilities-only consumer builds and runs the following sequence
while linking only `ASC::utilities`:

```cpp
#include <asc/utilities.h>

#include <string>

int main() {
  asc::ConfigParser configuration;
  if (!configuration
           .TryLoadFromString("answer = 42\n",
                              asc::UnknownConfigKeyPolicy::kAdd)
           .ok()) {
    return 1;
  }
  asc::Result<std::string> serialized = configuration.Serialize();
  if (!serialized.ok() || serialized.value() != "answer = 42\n") {
    return 1;
  }

  int number = 0;
  asc::OptionParser options;
  options.AddOption<asc::Variable<int>>("n", "number", "number", 0,
                                        &number);
  const char* argv[] = {"consumer", "--number", "-7"};
  if (!options.TryParse(3, argv).ok() || number != -7) {
    return 1;
  }

  const asc::Timer timer;
  return timer.GetMeasurementCount() == 0 && timer.LastTime() == 0 &&
                 timer.TotalTime() == 0 && timer.AverageTime() == 0
             ? 0
             : 1;
}
```

## Error handling

Failures caused by external input return `Status` or `Result<T>`. Examples are
malformed configuration text, a missing file, validator rejection, an unknown
option, conversion overflow, and a missing required option. Their signatures
do not change when exception translation is disabled.

Programmer errors use release-active contracts. Timer state misuse and invalid
output buffers are examples.

Legacy void/value wrappers remain during the bounded pre-1.0 transition. A
wrapper translates a failed canonical status through the historical
throw-or-abort path. New code should call the `Try*` operation and inspect its
status instead.

## Configuration values

`<asc/utilities/config.h>` provides `ConfigValue` and `ConfigParser`.
`ConfigValue` stores one of:

| Kind | C++ representation | Canonical text |
| --- | --- | --- |
| Empty | `std::monostate` | `null` |
| Boolean | `bool` | `true` or `false` |
| Integer | `int` | decimal integer |
| Floating point | `double` | finite round-trippable decimal |
| String | `std::string` | quoted and escaped string |
| Vector | `ConfigVector`, an alias of `std::vector<double>` | `vector[...]` |
| Matrix | `ConfigMatrix`, a nested standard vector | `matrix[[...], ...]` |

Configuration storage is deliberately independent of ASC numerical arrays.

### Checked access

`AsInt()` reads an integer directly. A stored double converts to `int` only
when it is finite, exactly integral, and in range. It never silently truncates
or overflows. `AsDouble()` accepts an integer by widening it. Other mismatched
`As*` calls are programmer-contract violations.

Canonical configuration rejects non-finite doubles. A matrix must be
rectangular; empty rows are valid only when every row has the same length.
`ConfigValue` constructors still permit raw compatibility values, but
`TryAddConfig`, `TrySetConfig`, parsing, and `Serialize` reject non-finite or
ragged state at the canonical collection boundary.

### Status-oriented collection operations

Use these operations for data that can be invalid:

- `TryAddConfig` defines or replaces a key after validating its name, value,
  and optional validator;
- `TrySetConfig` updates an existing key and never creates a misspelled key;
- `FindConfig` returns a value result without exposing a reference whose
  lifetime could be invalidated by later changes;
- `TryParseValue` parses one canonical value;
- `TryLoadFromString` and `TryLoadFromFile` load a complete collection
  transactionally;
- `Serialize` produces canonical text.

A failed set or load leaves the entire previous collection unchanged.
Validators run for defaults and updates. A rejected value returns an invalid
argument status identifying the key.

Unknown loaded keys fail by default. Pass the explicit
`UnknownConfigKeyPolicy` that permits additions only when the input is intended
to extend the collection's schema. Duplicate keys in one input always fail.

The existing `AddConfig`, `SetConfig`, `GetConfig`, `LoadFromFile`, and
`ParseValue` names remain compatibility wrappers. In particular, legacy
`SetConfig` can still add a missing key; canonical `TrySetConfig` cannot.

### Canonical configuration text

A collection contains one `key = value` entry per line. Canonical output sorts
keys lexicographically and terminates every entry with a newline. This makes
serialization independent of definition order.

Keys are non-empty. The first character is an ASCII letter or `_`; remaining
characters may also contain digits, `.`, or `-`.

```text
enabled = true
label = "baseline\nrun"
matrix = matrix[[1.0, 2.0], [3.0, 4.0]]
origin = vector[0.0, 0.0, 0.0]
samples = 64
step = 1.0e-3
```

Strings use `\"`, `\\`, `\n`, `\r`, and `\t` escapes. `#` starts a comment
only outside a quoted string. Canonical output uses lowercase Booleans and
quoted strings. Input also accepts case-insensitive Booleans and an unquoted,
non-reserved token as a compatibility string.

The following inputs fail rather than being guessed: an empty value, invalid
escape, malformed bracket, duplicate key, integer or floating-point overflow,
non-finite number, and ragged matrix.

`vector[]` is an empty vector. `matrix[]` is an empty matrix. Floating output is
locale-independent and contains a decimal point or exponent when necessary to
remain distinct from an integer.

## Command-line parsing

`<asc/utilities/cli.h>` contains `Option`, `Variable<T>`, `Switch`, and
`OptionParser`. Built-in value types are `int`, `float`, `double`, `bool`, and
`std::string`.

Option names are owned strings and are declared without leading hyphens. At
least one of the short and long names must be present; names cannot contain
whitespace or `=`. Duplicate short or long declarations fail. Short names are
not silently truncated.

### Supported tokens

```text
-n value
--number value
--number=value
```

A value option consumes the next token even when it begins with `-`, so
negative integers, decimals, and scientific notation work. A switch consumes
no following token. Short-option clustering such as `-abc` is not supported.

`--` ends option recognition. M1 has no positional-argument facility, so any
token after `--` returns an unsupported positional-argument status. An
unprefixed token in ordinary parsing is also reported instead of ignored.

Unknown options, missing values, invalid values, overflow, and unsatisfied
required options return status. Repeated occurrences are allowed and the last
value wins.

### Transaction and lifetime rules

`TryParse` validates names, conversions, required constraints, and the complete
candidate invocation before committing. On success, that invocation replaces
the option set/unset state and values from the previous parse. On failure,
previously committed values and bound output variables remain unchanged.

The parser owns its option objects and names. A pointer supplied through the
output-binding facility must outlive the option and parser operations that can
update it. `AddOption` returns the owned option object for state inspection;
the existing `GetOption<T>` lookup remains a compatibility path rather than a
new status-oriented retrieval API.

`TryParseFile` retains the compatibility file form:

```text
number = -3
label = run-a
```

It is not a second canonical configuration system. New applications should use
`ConfigParser` for configuration files and `OptionParser` for argv input.

### Help and usage

Canonical help and usage operations return text or write to an explicitly
provided stream. Formatting uses dynamic standard containers and has no fixed
line-count limit. It does not write to global `mout` unless a caller explicitly
uses the no-argument compatibility overload.

Custom subclasses of `Option` are compatibility-only in M1. The option
subclass ABI and extension hooks are not a stable pre-1.0 interface.

## Timing

`<asc/utilities/timer.h>` provides the hardened `Timer`. It uses
`std::chrono::steady_clock`; durations are reported as `real_t` seconds unless
milliseconds are requested.

### State transitions

| Current state | Operation | Result |
| --- | --- | --- |
| Idle | `Start()` | Starts a measurement |
| Running | `Start()` | Restarts the active interval for compatibility |
| Running | `Stop()` | Records a non-negative interval and becomes idle |
| Idle | `Stop()` | Release-active precondition failure |
| Either | `Reset()` | Clears measurements and becomes idle |

`IsRunning()` reports the state. `GetMeasurementCount()` reports the true
number of completed measurements.

All queries are safe before the first measurement:

| Query | Empty value |
| --- | ---: |
| `LastTime()` | `0` |
| `TotalTime()` | `0` |
| `AverageTime()` | `0` |
| `GetMeasurementCount()` | `0` |
| `AccumulateTime()` | no writes |

Total, last, average, count, and accumulation prefixes remain correct after
more than 128 samples and after `Compress()`. M1 implements `Compress()` as a
semantic no-op so no retained prefix is lost. Seconds and milliseconds differ
by exactly `1000`.

`Print` writes the last interval—not the total or minimum—to the explicit
stream, followed by `s` or `ms` and exactly one newline. An invalid unit is a
contract violation.

A `Timer` instance is not thread-safe. Independent timers contain no shared
mutable state and may be used by different threads.

## Compatibility boundary

`<asc/utilities/optparser.h>` forwards to the canonical CLI declarations so
existing include paths remain valid; it is not a second parser. Existing
void/value wrappers remain available during the documented pre-1.0 migration
window.

Behavior that was unsafe or contradicted the documentation is intentionally
corrected in M1. Negative numeric options work, unknown positional input fails,
switches do not consume a following value, help has no 500-line limit,
configuration loading is transactional on the canonical path, and empty or
long-running timers produce defined statistics.

No removal release is declared by M1. Compatibility wrappers are removed only
after an equivalent status-oriented path exists, known downstream users have
migrated, and a breaking release is announced.

## Deferred scope

Utilities M1 does not provide tracing, telemetry, profiling orchestration,
workflow scheduling, an experiment database, or persistent result storage.
An injected diagnostic sink usable by Core must live at a lower dependency
boundary; Utilities cannot own it without creating a reverse dependency.

See [Utilities migration](../migration/utilities.md) for detailed mapping from
the inherited interfaces.
