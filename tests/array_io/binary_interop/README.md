# Installed independent binary checks

The package harness copies this consumer and its three first-party Python
fixture/oracle tools outside the checkout. Dense and Sparse configure
independently against relocated exported targets. Ordinary consumers do not
acquire a Python dependency.

`check_binary_interop.py` retains the original 60 profiles: 12 scalar codes
with Dense left/right, COO, CSR and CSC. Both directions use those same
profiles. Another 20 real/complex profiles cover quiet-NaN component payloads,
signed zeros and infinities; signaling NaNs are outside the frozen guarantee.
All profiles have nonempty rank-two shape `(2,2)`. Other ranks and empty/padded
classes remain covered by their existing tests and acceptance mappings.

The writer receives independently prepared scalar component bits and canonical
structure. Python checks the complete binary bytes and independently decodes
metadata, logical ordering, structure and values. The reader receives a frame
encoded entirely by the Python oracle. Its public view is observed directly as
shape, structure integers and component-bit hexadecimal strings; this diagnostic
is not an ASC wire format and never calls an ASC formatter or decoder.

For every profile, checksum corruption, truncation and trailing bytes must fail
before an existing destination commits. Complete values and structure are
compared before/after rejection; input consumption is required and is not
rolled back. All owner allocations must be released. Existing full byte-boundary,
resource and padding regressions remain independently required.

Each CTest invocation creates fresh random-suffix scratch with exclusive Python
creation, so reruns preserve earlier observations. Missing files, mismatched
bytes, failed consumers and empty/wrong profile selections are errors.
