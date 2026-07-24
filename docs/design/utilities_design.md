# Utilities Module Design

**Status:** Approved by the Lead Architect for Utilities Milestone 1 implementation  
**Date:** 2026-07-22  
**Authority:** `architecture_blueprint_v1.md` and the Phase III Utilities team  
**Public component:** `ASC::utilities`

## 1. Purpose

Utilities M1 makes configuration, command-line parsing, and timing safe enough
to use as lightweight foundations. It preserves the useful pre-1.0 class names
while replacing inherited implementation hazards with explicit status,
state, and serialization contracts.

This design predates every Utilities M1 source, test, and user-documentation
change. Implementation, testing, and documentation work may begin only after
this file exists.

## 2. Team roles

### Implementation Engineer

Owns the canonical utility headers, compiled implementations, compatibility
forwarding, explicit CMake manifests, and dependency boundary.

### Testing Engineer

Owns `tests/utilities`, minimal `ASC::utilities` linkage, failure and
transaction tests, header/dependency/ODR checks, and the installed utilities
consumer.

### Documentation Engineer

Owns the Utilities module and migration guides and updates shared API,
architecture, testing, and migration records. Shipped and planned behavior
must remain visibly distinct.

## 3. Responsibilities and non-goals

Utilities M1 owns:

- standard-container configuration values, validation, parsing, and canonical
  serialization;
- command-line option declaration, typed values, parsing, and help text;
- monotonic per-instance timing and lossless summary statistics;
- a canonical `<asc/utilities.h>` umbrella;
- compatibility for ordinary uses of the existing public names;
- dedicated tests and user documentation.

Utilities M1 does not own:

- numerical arrays, execution contexts, memory or backend selection;
- logging policy for lower modules, tracing, telemetry, or profiling systems;
- workflow scheduling, experiment databases, checkpoints, or persistence;
- positional command-line operands, short-option clustering, environment
  expansion, or a second configuration-file syntax in the CLI parser;
- a stable binary ABI for subclassing the current polymorphic option types.

Diagnostics/tracing are deferred. A sink used by Core cannot be owned by
Utilities without reversing the component graph; a future minimal sink
contract must be placed at the appropriate lower boundary first.

## 4. Dependency contract

The only public component edge remains:

```mermaid
flowchart LR
  utilities[ASC::utilities] --> core[ASC::core]
```

Canonical utility headers and implementation files may use the C++ standard
library and canonical Core configuration, types, status/result, and contracts.
They must not include:

- legacy Core globals, error policy, casts, strings, device, memory, CUDA,
  `forall`, or device-coupled operators;
- Array, Linalg, Random, or the aggregate `cpp` component;
- optional provider SDK headers.

An isolated compiled compatibility translation unit may call the legacy error
translator or `mout` to preserve an old wrapper. It is not included by the
canonical umbrella, contains no canonical implementation, and is called only
by explicitly documented compatibility overloads.

`ASC::utilities` links publicly only to `ASC::core`.

## 5. Public and source surface

M1 adds:

```text
include/asc/utilities.h
include/asc/utilities/cli.h
src/utilities/cli.cc
src/utilities/compatibility.cc
```

M1 retains and updates:

```text
include/asc/utilities/config.h
include/asc/utilities/timer.h
include/asc/utilities/optparser.h
src/utilities/config.cc
src/utilities/timer.cc
```

`<asc/utilities.h>` includes canonical configuration, CLI, and timer headers.
`<asc/utilities/optparser.h>` becomes a compatibility forwarding spelling for
`<asc/utilities/cli.h>`; it does not carry a second implementation.

All public headers are explicit members of the existing Utilities file set.
No new exported target is created.

## 6. Error and compatibility policy

Recoverable failures return `Status` or `Result<T>`, including:

- malformed configuration or command-line text;
- file I/O failure;
- conversion overflow or unsupported non-finite values;
- validator rejection;
- unknown keys/options/positionals;
- missing option values or required options.

Programmer errors use release-active Core contracts, including invalid timer
state transitions and invalid output buffers.

Canonical status-returning signatures do not change with exception mode.
Existing void/value wrappers remain for a bounded compatibility window and
translate a failed status through the historical throw-or-abort path. New code
must use the status-returning APIs. Compatibility wrappers do not define the
canonical failure model.

M1 preserves ordinary source use of `ConfigValue`, `ConfigParser`, `Option`,
`Variable<T>`, `Switch`, `OptionParser`, `Timer`, and `TimeUnit`. Internal
layout changes mean M1 makes no pre-1.0 ABI promise for option subclassing or
objects exchanged by value across separately built binaries.

## 7. Configuration contract

### 7.1 Values and access

`ConfigValue` retains these alternatives:

```text
empty, bool, int, double, string, ConfigVector, ConfigMatrix
```

`ConfigVector` is `std::vector<double>` and `ConfigMatrix` is a nested standard
vector. Configuration never depends on ASC arrays.

`AsInt()` accepts an integer directly. A stored double may convert only when it
is finite, exactly integral, and within `int` range; unchecked truncation and
overflow are forbidden. Other `As*` type mismatches are programmer contract
violations. Status-oriented parsing and lookup are used for untrusted input.

Non-finite doubles are not valid canonical configuration values. Matrices in
canonical configuration are rectangular; a ragged matrix is rejected when it
is defined, set, parsed, or serialized.

### 7.2 Status-oriented operations

`ConfigParser` adds status/result operations with the following semantics:

- `TryAddConfig` defines or replaces a key after name, value, and validator
  checks;
- `TrySetConfig` updates an already defined key and does not implicitly create
  one;
- `FindConfig` returns a value result without exposing an unstable reference;
- `TryParseValue` parses one canonical value;
- `TryLoadFromString` and `TryLoadFromFile` parse and validate transactionally;
- `Serialize` returns canonical text or a status if existing compatibility
  state cannot be represented.

Legacy `AddConfig`, `SetConfig`, `GetConfig`, `LoadFromFile`, and `ParseValue`
remain wrappers. In particular, legacy `SetConfig` may retain add-if-missing
behavior; canonical `TrySetConfig` never does.

Unknown loaded keys fail by default. An explicit `UnknownConfigKeyPolicy`
allows adding them when a caller intentionally requests that behavior. A
failed set or load leaves the complete previous collection unchanged.
Validators are applied to defaults and updates. M1 retains the existing
Boolean validator type; rejection produces `kInvalidArgument` with the key in
the diagnostic.

### 7.3 Canonical text grammar

A collection is serialized as one `key = value` entry per line. Keys must be
non-empty and consist of ASCII letters, digits, `_`, `.`, or `-`; the first
character must be a letter or `_`. Canonical output sorts keys
lexicographically, independent of definition order, and ends each entry with
`\n`.

Canonical values are:

```text
null
true | false
-?[0-9]+
finite round-trippable decimal floating point
"quoted string with \\" \\\\ \\n \\r \\t escapes"
vector[<double>, ...]
matrix[[<double>, ...], [<double>, ...], ...]
```

Canonical double output is locale-independent, preserves a `double`
round-trip, and contains a decimal point or exponent when needed to remain
distinct from an integer. `vector[]` and `matrix[]` distinguish empty vector
and empty matrix. Matrix rows may be empty only when every row has the same
length.

Input accepts surrounding ASCII whitespace, lowercase/uppercase Boolean
literals for compatibility, and an unquoted non-reserved token as a legacy
string value. Canonical output always uses lowercase Booleans and quotes
strings. `#` begins a comment only outside a quoted string. Empty values,
invalid escapes, malformed brackets, duplicate keys in one input, integer or
floating overflow, non-finite values, and ragged matrices return a stable
failure status.

## 8. Command-line contract

### 8.1 Public model

The familiar `Option`, `Variable<T>`, `Switch`, and `OptionParser` hierarchy is
retained in canonical `<asc/utilities/cli.h>`. Built-in canonical value types
are `int`, `float`, `double`, `bool`, and `std::string`.

Short and long names are owned `std::string` values. Names are supplied without
leading hyphens, must be non-empty in at least one form, and may not contain
whitespace or `=`. Silent four-character truncation is removed. Duplicate
short or long declarations are rejected.

`TryParse` parses an argv-style range and returns status. `TryParseFile`
retains the legacy `key = value` file facility during migration. Existing
`Parse` methods are compatibility wrappers.

### 8.2 Token grammar

M1 supports:

```text
-n value
--number value
--number=value
```

A value-taking option always consumes its following token, even when that
token begins with `-`; therefore negative integers, decimals, and scientific
notation work. A switch consumes no following token. Short-option clustering
is not supported.

`--` ends option recognition. Because M1 has no positional facility, every
token after it is reported as an unsupported positional argument. A token
without an option prefix is likewise reported. Unknown options, missing
values, invalid values, overflow, and required options not supplied return
status rather than being ignored.

Repeated occurrences are allowed and the last value wins. A successful parse
represents that invocation: options are reset to unset/default state before
candidate values are committed. Parsing is transactional. All names, values,
required constraints, and conversions are checked first; a failed parse leaves
the previously committed option values and bound output variables unchanged.

### 8.3 Help and compatibility

Help and usage text use dynamic standard containers/streams and have no fixed
line-count buffer. Canonical overloads return text or write to an explicitly
provided stream. They do not use a global sink. No-argument printing may remain
as a compatibility overload implemented outside canonical headers.

Custom `Option` subclasses are compatibility-only in M1 unless they implement
the status-oriented validation/commit hooks. They are not part of the stable
canonical extension contract.

## 9. Timer contract

M1 hardens `Timer` in place and does not introduce a competing stopwatch type.
It continues to use `std::chrono::steady_clock` and `real_t` seconds.

The timer has explicit idle/running state:

- construction and `Reset()` produce idle state with zero samples;
- `Start()` while idle begins a measurement;
- repeated `Start()` restarts the active interval for compatibility;
- `Stop()` while idle is an always-active precondition violation;
- successful `Stop()` records a non-negative interval and returns to idle.

Queries are safe before any measurement:

| Query | Empty result |
| --- | ---: |
| `LastTime()` | `0` |
| `TotalTime()` | `0` |
| `AverageTime()` | `0` |
| measurement count | `0` |
| accumulation | no writes |

The implementation tracks lossless total, last, and true measurement count.
More than 128 measurements and `Compress()` must not corrupt total, last,
average, or count. The old fixed buffer may be replaced with dynamic storage;
`Compress()` may compact representation but not observable statistics.

M1 adds `Reset()`, `IsRunning()`, and `GetMeasurementCount()`. Accumulation
requires a non-null destination only when samples exist and returns prefix
sums for retained measurements. Invalid time units are contract violations.
Seconds and milliseconds differ by exactly `1000`.

Legacy `Print` continues to print the last interval, with an explicit stream,
unit suffix, and one terminating newline. Documentation must not call it a
total or minimum. A Timer instance is not thread-safe; independent instances
have no shared mutable state.

## 10. Source and build ownership

- `src/utilities/CMakeLists.txt` lists every compiled file and public header
  explicitly.
- Canonical implementation lives in `config.cc`, `cli.cc`, and `timer.cc`.
- Historical error/global-output adaptation is isolated in
  `compatibility.cc`.
- `optparser.cc` is retired from implementation or reduced to a harmless
  compatibility stub; it must not duplicate CLI symbols.
- Utilities introduces no external dependency.
- The canonical aggregate `<asc/cpp.h>` remains source compatible through the
  forwarding `optparser.h` spelling.

## 11. Test design

Dedicated tests live in `tests/utilities` and link only `ASC::utilities`,
GoogleTest, and private project options.

### 11.1 Configuration

- every value alternative and checked numeric access;
- exact canonical serialization and lexical key ordering;
- parse/serialize/parse round trips, escaping, comments, vectors, and matrices;
- empty/malformed/overflow/non-finite/ragged input;
- validator acceptance/rejection;
- unknown-key policy, duplicate keys, and malformed names;
- full rollback after failed set/load.

### 11.2 CLI

- short, long, and `--name=value` forms;
- negative integer, decimal, and exponent values;
- switch non-consumption;
- unknown option/positional, missing/invalid value, overflow, and required
  option failures;
- defaults, repeated occurrences, repeated parse, reset, and transactionality;
- long names/descriptions and more than 500 help lines;
- explicit help/usage sinks and compatibility forwarding.

### 11.3 Timer

- empty query and accumulation behavior;
- release-active Stop-before-Start failure;
- running/idle transitions and repeated Start;
- last/total/average/count identities over multiple and more than 128 samples;
- compression, reset, unit conversion, print statistic/newline, invalid unit,
  and output-pointer contracts;
- non-negative elapsed values without fragile wall-clock thresholds.

### 11.4 Architecture and package

- every canonical/forwarding header self-contained with `ASC::utilities`;
- canonical files contain no higher-component, legacy-runtime, or provider SDK
  include;
- `ASC::utilities` has no forbidden link edge;
- representative configuration/CLI/timer use across multiple translation
  units;
- installed utilities-only consumer includes `<asc/utilities.h>` and exercises
  canonical serialization, negative CLI input, and empty timer behavior;
- existing generic compatibility and package relocation remain regression
  gates.

## 12. Documentation deliverables

M1 adds:

```text
docs/modules/utilities.md
docs/migration/utilities.md
```

It updates API, architecture, testing, and migration inventory documentation.
Examples must be compiled by a dedicated or installed consumer test. Planned
tracing and removed compatibility behavior must not be presented as shipped.

## 13. M1 acceptance gate

Utilities M1 is complete only when:

- this design predates implementation changes;
- the canonical public surface has only stable Core dependencies;
- configuration round-trips every advertised type and mutates transactionally;
- canonical CLI parsing handles negative values and reports unsupported input;
- timer empty and greater-than-128-sample semantics are safe and lossless;
- dedicated tests and header checks link only `ASC::utilities`;
- canonical dependency and multi-TU ODR checks pass;
- a utilities-only installed consumer runs the documented quick start;
- default and strict full builds/tests pass;
- focused sanitizer and exception-disabled status paths pass where supported;
- documentation and dependency review find no claim beyond implementation;
- no unrelated component implementation is changed.

Only after this gate may the Lead Architect begin the Array module design.
