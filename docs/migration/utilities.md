# Utilities migration

> [!WARNING]
> **Superseded historical document.** The body below records the deleted
> five-component implementation at commit
> `33b261ea33616a6395c4ad3b20646093103344f7`. It is retained only for audit
> history and does not describe the active API or package. See the
> [current documentation][current-docs] and the
> [approved Stage A architecture][stage-a].

[current-docs]: ../README.md
[stage-a]: ../development/asc-cpp-architecture/architecture-blueprint.md

Utilities M1 replaces inherited parsing and timer hazards without introducing a
second public module or a second permanent class hierarchy. Familiar class
names remain, while status-oriented operations define the canonical failure
model.

## Milestone status

| Area | M1 status | Later work |
| --- | --- | --- |
| Configuration values, validation, parsing, and serialization | Canonical M1 | Remove legacy throw-or-abort wrappers after migration |
| Command-line parsing and dynamic help | Canonical M1 | Stabilize an extension protocol only if concrete custom-option demand exists |
| Lossless timer statistics | Canonical M1 | Performance methodology belongs to a later project-wide gate |
| Legacy `optparser.h` include | Forwarding compatibility header | Remove only at a declared breaking release |
| Diagnostics and tracing | Deferred | Place a minimal sink below Utilities before integrating with Core |

M1 makes no stable binary-ABI promise for polymorphic option subclasses or
objects passed by value between separately built binaries.

## API mapping

| Inherited API or behavior | Canonical M1 direction | Compatibility disposition |
| --- | --- | --- |
| `<asc/utilities/optparser.h>` | `<asc/utilities/cli.h>` | Old header forwards to the same implementation |
| component headers included individually | `<asc/utilities.h>` | Narrow headers remain supported |
| `ConfigParser::AddConfig` | `TryAddConfig` | Void wrapper translates failed status |
| `ConfigParser::SetConfig` and implicit insert | `TrySetConfig` updates only defined keys | Legacy wrapper may retain add-if-missing behavior |
| `ConfigParser::GetConfig` | `FindConfig` result for untrusted names | Reference wrapper retained for known schema keys |
| `ConfigParser::ParseValue` | `TryParseValue` | Value wrapper translates failed status |
| line-by-line `LoadFromFile` mutation | transactional `TryLoadFromString`/`TryLoadFromFile` | Void file wrapper translates failed status |
| no configuration serialization | canonical `Serialize` | New status-oriented operation |
| CLI `Parse` | transactional `TryParse` | Void wrapper translates failed status |
| CLI file `Parse` | `TryParseFile` for migration only | New code uses configuration for files |
| fixed `char[5]` short names | owned, validated strings | Silent truncation removed |
| fixed 500-line help buffer | dynamic help/usage text | Existing print spelling remains |
| default output through `mout` | returned text or explicit stream | No-argument printing is compatibility-only |
| unsafe empty timer queries | defined zero values | Existing query names retained |
| fixed-capacity sample accounting | lossless total/last/average/count and prefixes | `Compress` is a compatibility no-op in M1 |
| timer `Print` documentation said total | `Print` is explicitly last interval | Method spelling retained |

## Deliberate behavior corrections

These changes are intentional even when accidental legacy behavior differs:

- a negative numeric token is consumed as the preceding option's value;
- unknown options and positional arguments are reported rather than ignored;
- a switch never consumes the following token;
- `--long=value` is accepted;
- repeated options are reset per invocation and the final occurrence wins;
- a failed command-line parse commits no partial values or bound-output writes;
- CLI declarations are not silently shortened to four characters;
- canonical CLI files use the implementation's actual `key = value` form;
- unknown canonical configuration keys fail unless additions are explicitly
  permitted;
- failed canonical configuration set/load operations roll back completely;
- every advertised configuration value has round-trippable canonical text;
- non-finite values, numeric overflow, invalid escaping, duplicate keys, and
  ragged matrices fail;
- `AsInt()` does not silently truncate a fractional or out-of-range double;
- empty timer queries return zero, while `Stop()` in the idle state is a
  programmer-contract violation;
- measurement count and average remain correct beyond 128 samples;
- timer printing consistently reports the last interval and one newline.

## Configuration text migration

Historical `ConfigParser` input primarily handled scalar, unquoted text. M1
continues to accept case-insensitive Booleans and unquoted non-reserved tokens,
but canonical output is typed and unambiguous:

```text
name = "experiment-a"
origin = vector[0.0, 0.0]
operator = matrix[[1.0, 0.0], [0.0, 1.0]]
samples = 128
step = 0.01
```

Keys are sorted in serialized output. Strings are quoted and escaped. Vectors
and matrices use explicit prefixes so an empty value retains its type.

Applications that intentionally accept input-defined keys must select the
addition policy explicitly. Schema-driven applications should keep the default
unknown-key rejection to catch spelling mistakes.

## CLI migration

Option names are declared without `-` or `--`:

```text
short name: n
long name: number
argv forms: -n -3, --number -3, --number=-3
```

Old comments and examples that declared `"-nx"` or described CLI files as
`-nx 100` were inaccurate. The compatibility file form is `name = value`.

New code calls `TryParse` and handles its returned status. Existing `Parse`
callers retain historical exception-or-abort translation during migration.
Because M1 does not support positional operands, programs that need them must
parse that outer application syntax separately rather than relying on the old
silent-ignore behavior.

Raw bound output pointers remain for source compatibility. They must outlive
every parser operation that can update them. `AddOption` returns the owned
option object for state inspection; `GetOption<T>` remains a compatibility
lookup and translates lookup/type failures through the historical error path.

## Timer migration

Existing timing loops continue to use `Timer`, `Start`, and `Stop`. The
observable corrections are:

- safe zero results before the first completed measurement;
- explicit `Reset`, `IsRunning`, and true measurement count;
- release-active failure for an idle `Stop`;
- no statistical loss after the old fixed buffer fills;
- exact and consistent unit/printing behavior.

Repeated `Start` deliberately restarts the active interval for compatibility.
It does not create a measurement until `Stop` succeeds.

## Compatibility window

The void/value wrappers and forwarding include are bounded pre-1.0 surfaces.
Removal requires:

1. equivalent status-oriented operations for ordinary uses;
2. dedicated tests for both canonical and compatibility paths;
3. migration of known downstream callers;
4. a documented breaking release and removal table.

M1 declares no removal release and does not claim a stable ABI for custom
option subclasses.

## Provenance

The original utilities were selected from MdeCpp as recorded in
[the migration inventory](inventory.md). M1 retains standard-container values
and familiar names, but its status, transaction, serialization, CLI grammar,
dependency, and timer contracts come from the approved asc-cpp architecture
and [`utilities_design.md`](../design/utilities_design.md).
