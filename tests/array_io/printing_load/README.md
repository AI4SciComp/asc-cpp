# Compiler-observed printing loads

This maintains the first-party `printing-load-04` diagnostic unchanged: 24
CSR/CSC scalar/storage profiles and 288 Dense rank-three/four combinations,
plus the original strided Dense and COO controls. Selected offsets and load
budgets are independent of the production traversal. Live typed backing arrays
avoid fabricated capacities and object-lifetime overlaps.

Clang compiles the consumer at `-O1` with its documented
`-fsanitize-coverage=trace-pc,trace-loads` callbacks. The observer translation
unit is deliberately uninstrumented. The same uninstrumented consumer must
exit 1: otherwise the observation mechanism could report a false pass.
This negative control is a harness check, not a waived mathematical failure.

The installed package harness copies all sources away from the checkout and
runs this diagnostic on Clang/AppleClang configurations. Other compilers still
run ordinary printing tests; they receive no compiler-load evidence credit.
These are compiler-instruction observations for the recorded configuration,
not hardware counters, a performance claim or instrumentation of Core/stdlib
objects. Each test is synchronous; the test-only callback registry is empty
outside observation and is never a library or provider registry.
