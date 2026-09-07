# LAPACK and array API notes

P00 is in progress. No new public C++ declarations are implemented yet.
Provider interfaces use explicit Dense-owned overloads, typed arguments, ASC
64-bit dimensions and caller-owned conversion/workspace storage. Reports remain
accessible after numerical failure. Base Dense has no foreign symbol dependency.

Storage-neutral codecs belong to Core; Dense and Sparse each own their array
formats. Printing, ASC text, ASC binary and Matrix Market are distinct APIs.
No dense/sparse sibling dependency or implicit device transfer is permitted.

Concrete P01 descriptor/workspace signatures and per-routine contracts must be
frozen and tested before generating bindings. See the complete runbook, not
these notes, for requirements.
