# API conventions {#api_conventions}

All public declarations are in namespace `asc` and are available through the
header and CMake component named in their module documentation. Consumers must
compile as C++20. Public headers are self-contained and do not require
exceptions.

## Failure and contracts

Recoverable failures use `asc::Status` or `asc::Result<T>`. An OK status has no
provider/native failure; a failed result contains a non-OK status. Accessing a
failed result's value is a fatal contract violation. Preconditions documented
as caller contracts may terminate through `asc::FatalContract`; they are not
converted into exceptions.

## Ownership and memory

Owners are move-only unless stated otherwise. Views never extend storage
lifetime. Callers keep buffers, memory resources, contexts, and provider
objects alive through the documented completion boundary. A memory space is
part of a view's contract; CPU operations require host accessibility and CUDA
operations require the documented device/pinned accessibility.

## Execution and completion

Provider-free CPU calls are synchronous with `ExecutionContext::Serial()`.
CUDA operations may enqueue work and return a completion event; success of the
enqueue is not completion. Callers must wait or otherwise order the returned
event before reading results or releasing referenced resources.

## Numerical behavior

Shapes, indices, strides, layouts, transpose/conjugation modes, aliasing,
precision, and workspace requirements are part of each operation's contract.
Reference BLAS implements the documented mathematical operation and is not a
performance claim. Random reproducibility holds only for the documented
engine/distribution/address mapping and version boundary.
