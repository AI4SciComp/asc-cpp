#!/usr/bin/env python3
"""Independently validates bounded ASC v1 text/binary development fixtures.

This intentionally simple, whole-fixture Python oracle is not a production
stream parser, installed dependency, or evidence that the C++ I/O exists.
Values are opaque little-endian scalar bytes so binary NaNs retain their bits.
Use ``python3 tools/array_io/reference_codec.py --help`` for CLI usage.
"""

from __future__ import annotations

import argparse
import dataclasses
import decimal
import math
import pathlib
import re
import struct
import sys
import zlib

_SCALARS = (
    "i8",
    "u8",
    "i16",
    "u16",
    "i32",
    "u32",
    "i64",
    "u64",
    "f32",
    "f64",
    "c64",
    "c128",
)
_KINDS = ("dense", "coo", "csr", "csc")
_MAGIC = b"ASCARRB\n"
_MAX_INDEX = (1 << 63) - 1
_UNSIGNED = re.compile(r"[0-9]+", re.ASCII)
_INTEGER = re.compile(r"[+-]?[0-9]+", re.ASCII)
_REAL = re.compile(
    r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?",
    re.ASCII,
)


class FormatError(ValueError):
    """An input fixture violates the v1 format or an explicit size limit."""


@dataclasses.dataclass(frozen=True)
class Limits:  # pylint: disable=too-many-instance-attributes
    """Reference-fixture input and decoded representation budgets.

    This Python oracle allocates bounded complete frames. It does not model
    C++ caller resources, staging aliases, scratch or allocation accounting.
    Independent wire budgets intentionally remain separately named fields.
    """

    max_input_bytes: int = 67_108_864
    max_header_bytes: int = 65_536
    max_token_bytes: int = 256
    max_rank: int = 32
    max_extent: int = 2_147_483_647
    max_logical_elements: int = 16_777_216
    max_stored_elements: int = 16_777_216
    max_structure_bytes: int = 67_108_864
    max_decoded_bytes: int = 67_108_864


@dataclasses.dataclass(frozen=True)
class Frame:
    """An array frame with native-order structure and scalar value bytes.

    Attributes:
      kind: dense, coo, csr or csc.
      scalar: One exact v1 scalar code.
      shape: Extents in dimension order.
      structure: Flattened coordinates or compressed offsets then indices.
      values: One little-endian byte string per logical/stored scalar.
    """

    kind: str
    scalar: str
    shape: tuple[int, ...]
    structure: tuple[int, ...]
    values: tuple[bytes, ...]


def _width(scalar: str) -> int:
    if scalar not in _SCALARS:
        raise FormatError("unknown scalar")
    return int(scalar[1:]) // 8


def _limits(limits: Limits) -> None:
    for field in dataclasses.fields(limits):
        value = getattr(limits, field.name)
        if not _integer(value) or value < 0 or value > _MAX_INDEX:
            raise FormatError(f"invalid limit: {field.name}")


def _integer(value: object) -> bool:
    # Preserve strict fixture validation: bool and int subclasses with
    # user-defined arithmetic are not plain metadata integers.
    return type(value) is int  # pylint: disable=unidiomatic-typecheck


def _number(token: str, maximum: int, limits: Limits) -> int:
    if (len(token) > limits.max_token_bytes or not _UNSIGNED.fullmatch(token) or
            len(token.lstrip("0")) > len(str(maximum))):
        raise FormatError("invalid or overlong unsigned integer")
    result = int(token)
    if result > maximum:
        raise FormatError("unsigned integer exceeds limit")
    return result


def _metadata(
    kind: str,
    scalar: str,
    shape: tuple[int, ...],
    count: int,
    limits: Limits,
) -> int:
    if kind not in _KINDS:
        raise FormatError("unknown kind")
    width = _width(scalar)
    if len(shape) > limits.max_rank or len(shape) > (1 << 32) - 1:
        raise FormatError("rank exceeds limit")
    for extent in shape:
        if (not _integer(extent) or extent < 0 or
                extent > min(_MAX_INDEX, limits.max_extent)):
            raise FormatError("extent exceeds limit")
    logical = 0 if 0 in shape else math.prod(shape)
    if logical > min(_MAX_INDEX, limits.max_logical_elements):
        raise FormatError("logical size exceeds limit")
    if not _integer(count) or count < 0 or count > logical:
        raise FormatError("count is incompatible with shape")
    structure_count = _structure_count(kind, shape, count, logical, limits)
    if 8 * structure_count > limits.max_structure_bytes:
        raise FormatError("structure exceeds decoded budget")
    if 8 * structure_count + width * count > limits.max_decoded_bytes:
        raise FormatError("frame exceeds decoded budget")
    return structure_count


def _structure_count(
    kind: str,
    shape: tuple[int, ...],
    count: int,
    logical: int,
    limits: Limits,
) -> int:
    if kind == "dense":
        if count != logical:
            raise FormatError("dense count differs from logical size")
        return 0
    if count > limits.max_stored_elements:
        raise FormatError("stored count exceeds limit")
    if kind == "coo":
        return count * len(shape)
    if len(shape) != 2:
        raise FormatError("compressed kind requires rank two")
    return shape[0 if kind == "csr" else 1] + 1 + count


def _validate_coo(frame: Frame) -> None:
    rank = len(frame.shape)
    previous = None
    for position in range(len(frame.values)):
        coordinate = frame.structure[position * rank:(position + 1) * rank]
        if any(index >= extent
               for index, extent in zip(coordinate, frame.shape)):
            raise FormatError("coordinate outside shape")
        if previous is not None and coordinate <= previous:
            raise FormatError("noncanonical or duplicate COO coordinate")
        previous = coordinate


def _validate_compressed(frame: Frame) -> None:
    outer_dimension = 0 if frame.kind == "csr" else 1
    outer = frame.shape[outer_dimension]
    inner = frame.shape[1 - outer_dimension]
    offsets = frame.structure[:outer + 1]
    indices = frame.structure[outer + 1:]
    count = len(frame.values)
    if offsets[0] != 0 or offsets[-1] != count:
        raise FormatError("offset endpoints invalid")
    for begin, end in zip(offsets, offsets[1:]):
        if not 0 <= begin <= end <= count:
            raise FormatError("offset monotonicity invalid")
        segment = indices[begin:end]
        if any(index >= inner for index in segment):
            raise FormatError("inner index outside shape")
        if any(left >= right for left, right in zip(segment, segment[1:])):
            raise FormatError("noncanonical compressed segment")


def validate_frame(frame: Frame, limits: Limits = Limits()) -> None:
    """Checks exact scalar widths, shape, count and canonical structure.

    Args:
      frame: Bounded in-memory fixture to validate.
      limits: Input/metadata/decoded representation limits.

    Raises:
      FormatError: Metadata, structure, scalar bytes or limits are invalid.
    """
    _limits(limits)
    count = len(frame.values)
    structure_count = _metadata(
        frame.kind,
        frame.scalar,
        frame.shape,
        count,
        limits,
    )
    if len(frame.structure) != structure_count:
        raise FormatError("structure count mismatch")
    if any(len(value) != _width(frame.scalar) for value in frame.values):
        raise FormatError("scalar byte width mismatch")
    if any(not _integer(value) or not 0 <= value <= _MAX_INDEX
           for value in frame.structure):
        raise FormatError("structure integer outside ASC range")
    if frame.kind == "coo":
        _validate_coo(frame)
    elif frame.kind in ("csr", "csc"):
        _validate_compressed(frame)


def _round_ratio(numerator: int, denominator: int) -> int:
    quotient, remainder = divmod(numerator, denominator)
    return quotient + int(2 * remainder > denominator or
                          (2 * remainder == denominator and quotient % 2 == 1))


def _real_bytes(token: str, bits: int) -> bytes:
    precision, exponent_bits, bias = (24, 8, 127) if bits == 32 else (53, 11,
                                                                      1023)
    sign = int(token.startswith("-")) << (bits - 1)
    all_exponents = ((1 << exponent_bits) - 1) << (precision - 1)
    if token in ("inf", "-inf", "nan"):
        payload = (1 << (precision - 2)) if token == "nan" else 0
        return (sign | all_exponents | payload).to_bytes(bits // 8, "little")
    return (sign | _finite_bits(token, precision, bias)).to_bytes(
        bits // 8, "little")


def _finite_bits(token: str, precision: int, bias: int) -> int:
    if not _REAL.fullmatch(token):
        raise FormatError("invalid real token")
    try:
        value = decimal.Decimal(token)
    except decimal.InvalidOperation as error:
        raise FormatError("invalid decimal") from error
    if value.is_zero():
        return 0
    if not -400 <= value.adjusted() <= 400:
        raise FormatError("real token overflows or underflows")
    numerator, denominator = value.copy_abs().as_integer_ratio()
    exponent = _ratio_exponent(numerator, denominator)
    minimum = 1 - bias
    scale = precision - 1 - max(exponent, minimum)
    if scale >= 0:
        significand = _round_ratio(numerator << scale, denominator)
    else:
        significand = _round_ratio(numerator, denominator << -scale)
    if not significand:
        raise FormatError("nonzero real underflows to zero")
    if significand == 1 << precision:
        significand >>= 1
        exponent += 1
    if exponent > bias:
        raise FormatError("finite real overflows")
    if exponent < minimum and significand < 1 << (precision - 1):
        encoded = significand
    else:
        encoded = ((max(exponent, minimum) + bias) << (precision - 1))
        encoded |= significand - (1 << (precision - 1))
    return encoded


def _ratio_exponent(numerator: int, denominator: int) -> int:
    exponent = numerator.bit_length() - denominator.bit_length()
    if exponent >= 0:
        return exponent - int(numerator < denominator << exponent)
    return exponent - int(numerator << -exponent < denominator)


def scalar_bytes(token: str, scalar: str, limits: Limits = Limits()) -> bytes:
    """Converts one exact v1 text token to independently rounded scalar bytes.

    Args:
      token: One ASCII scalar token, without surrounding whitespace.
      scalar: Exact v1 scalar code.
      limits: Supplies the maximum complete token size.

    Returns:
      Little-endian scalar bytes with signed zero retained.

    Raises:
      FormatError: Syntax, type, token budget or representable range is invalid.
    """
    width = _width(scalar)
    if len(token) > limits.max_token_bytes:
        raise FormatError("overlong scalar token")
    if scalar[0] in "iu":
        if not _INTEGER.fullmatch(token) or (scalar[0] == "u" and
                                             token.startswith("-")):
            raise FormatError("invalid integer token")
        try:
            return int(token).to_bytes(width, "little", signed=scalar[0] == "i")
        except (OverflowError, ValueError) as error:
            raise FormatError("integer out of range") from error
    if scalar[0] == "f":
        return _real_bytes(token, width * 8)
    if not token.startswith("(") or not token.endswith(")"):
        raise FormatError("invalid complex delimiters")
    parts = token[1:-1].split(",")
    if len(parts) != 2:
        raise FormatError("complex requires two components")
    return _real_bytes(parts[0], width * 4) + _real_bytes(parts[1], width * 4)


def _scalar_text(value: bytes, scalar: str) -> str:
    if scalar[0] in "iu":
        return str(int.from_bytes(value, "little", signed=scalar[0] == "i"))
    if scalar[0] == "c":
        half = len(value) // 2
        real_scalar = "f32" if half == 4 else "f64"
        return (f"({_scalar_text(value[:half], real_scalar)},"
                f"{_scalar_text(value[half:], real_scalar)})")
    number = struct.unpack("<f" if scalar == "f32" else "<d", value)[0]
    return format(number, ".9g" if scalar == "f32" else ".17g")


class _TextCursor:
    """A bounded whole-fixture cursor, not a production streaming parser."""

    def __init__(self, data: bytes, limits: Limits):
        self.data = data
        self.limits = limits
        self.position = 0

    def line(self) -> str:
        """Returns exactly one ASCII record with its required EOL removed."""
        end = self.data.find(b"\n", self.position, self.limits.max_input_bytes)
        if end < 0:
            raise FormatError("missing line ending or input byte limit")
        raw = self.data[self.position:end]
        self.position = end + 1
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if any(byte < 32 or byte > 126 for byte in raw):
            raise FormatError("non-ASCII or control byte within frame")
        return raw.decode("ascii")

    def field(self, name: str) -> str:
        """Returns the payload of an exact named SP-separated header record."""
        raw = self.line()
        if not raw.startswith(name + " "):
            raise FormatError(f"expected {name} field")
        return raw[len(name) + 1:]


def _text_metadata(
        cursor: _TextCursor) -> tuple[str, str, tuple[int, ...], int]:
    limits = cursor.limits
    if cursor.line() != "ASCARRAY 1":
        raise FormatError("bad text magic or version")
    kind = cursor.field("kind")
    scalar = cursor.field("scalar")
    rank = _number(cursor.field("rank"), min(limits.max_rank, (1 << 32) - 1),
                   limits)
    raw_shape = cursor.line()
    if raw_shape != "shape" and not raw_shape.startswith("shape "):
        raise FormatError("expected shape field")
    tokens = [] if raw_shape == "shape" else raw_shape[6:].split(" ")
    if len(tokens) != rank:
        raise FormatError("rank/shape mismatch")
    shape = tuple(_number(token, limits.max_extent, limits) for token in tokens)
    if cursor.field("order") != ("dim0" if kind == "dense" else kind):
        raise FormatError("kind/order mismatch")
    count = _number(cursor.field("count"), _MAX_INDEX, limits)
    if cursor.position > limits.max_header_bytes:
        raise FormatError("header byte limit")
    _metadata(kind, scalar, shape, count, limits)
    return kind, scalar, shape, count


def _text_structure(
    cursor: _TextCursor,
    kind: str,
    shape: tuple[int, ...],
    count: int,
) -> tuple[int, ...]:
    limits = cursor.limits
    structure = []
    if kind == "dense":
        if cursor.line() != "data":
            raise FormatError("expected data section")
    elif kind == "coo":
        if cursor.line() != "coordinates":
            raise FormatError("expected coordinates section")
        for _ in range(count):
            raw = cursor.line()
            if not raw.startswith("(") or not raw.endswith(")"):
                raise FormatError("invalid coordinate delimiters")
            parts = [] if raw == "()" else raw[1:-1].split(",")
            if len(parts) != len(shape):
                raise FormatError("coordinate rank mismatch")
            structure.extend(
                _number(part, _MAX_INDEX, limits) for part in parts)
    else:
        if cursor.line() != "offsets":
            raise FormatError("expected offsets section")
        outer = shape[0 if kind == "csr" else 1]
        structure.extend(
            _number(cursor.line(), _MAX_INDEX, limits)
            for _ in range(outer + 1))
        if cursor.line() != "indices":
            raise FormatError("expected indices section")
        structure.extend(
            _number(cursor.line(), _MAX_INDEX, limits) for _ in range(count))
    if kind != "dense" and cursor.line() != "values":
        raise FormatError("expected values section")
    return tuple(structure)


def decode_text_frame(
        data: bytes,
        limits: Limits = Limits(),
) -> tuple[Frame, int]:
    """Decodes one bounded text frame and returns its exact consumed-byte count.

    The returned count ends after the mandatory end-record LF/CRLF. Bytes
    belonging to a following frame remain with the caller. This oracle takes
    complete bounded input rather than implementing a streaming ByteSource.

    Raises:
      FormatError: The first frame is malformed, truncated or exceeds limits.
    """
    _limits(limits)
    cursor = _TextCursor(data, limits)
    kind, scalar, shape, count = _text_metadata(cursor)
    structure = _text_structure(cursor, kind, shape, count)
    values = tuple(
        scalar_bytes(cursor.line(), scalar, limits) for _ in range(count))
    if cursor.line() != "end":
        raise FormatError("expected end record")
    frame = Frame(kind, scalar, shape, structure, values)
    validate_frame(frame, limits)
    return frame, cursor.position


def decode_text(data: bytes, limits: Limits = Limits()) -> Frame:
    """Decodes one text file, rejecting non-whitespace trailing data.

    Raises:
      FormatError: Frame, input byte budget or trailing-file policy fails.
    """
    if len(data) > limits.max_input_bytes:
        raise FormatError("input byte limit")
    frame, consumed = decode_text_frame(data, limits)
    trailing = data[consumed:].replace(b"\r\n", b"\n")
    if trailing.strip(b" \t\n"):
        raise FormatError("trailing text data")
    return frame


def encode_text(frame: Frame, limits: Limits = Limits()) -> bytes:
    """Encodes one validated fixture using canonical v1 text spelling.

    Raises:
      FormatError: Frame metadata/structure or output byte budget fails.
    """
    validate_frame(frame, limits)
    count = len(frame.values)
    shape = "".join(f" {extent}" for extent in frame.shape)
    lines = [
        "ASCARRAY 1", f"kind {frame.kind}", f"scalar {frame.scalar}",
        f"rank {len(frame.shape)}", f"shape{shape}",
        f"order {'dim0' if frame.kind == 'dense' else frame.kind}",
        f"count {count}"
    ]
    if len(("\n".join(lines) + "\n").encode("ascii")) > limits.max_header_bytes:
        raise FormatError("header byte limit")
    if frame.kind == "dense":
        lines.append("data")
    elif frame.kind == "coo":
        lines.append("coordinates")
        rank = len(frame.shape)
        for position in range(count):
            coordinate = frame.structure[position * rank:(position + 1) * rank]
            lines.append("(" + ",".join(map(str, coordinate)) + ")")
        lines.append("values")
    else:
        outer = frame.shape[0 if frame.kind == "csr" else 1]
        lines.append("offsets")
        lines.extend(map(str, frame.structure[:outer + 1]))
        lines.append("indices")
        lines.extend(map(str, frame.structure[outer + 1:]))
        lines.append("values")
    for value in frame.values:
        token = _scalar_text(value, frame.scalar)
        if len(token) > limits.max_token_bytes:
            raise FormatError("output scalar token budget")
        lines.append(token)
    lines.append("end")
    result = ("\n".join(lines) + "\n").encode("ascii")
    if len(result) > limits.max_input_bytes:
        raise FormatError("output byte limit")
    return result


@dataclasses.dataclass(frozen=True)
class _BinaryMetadata:
    """Validated fixed envelope plus bounded rank/shape for fixture decoding."""

    kind: str
    scalar: str
    shape: tuple[int, ...]
    count: int
    structure_count: int
    payload_bytes: int
    header_bytes: int


def _binary_metadata(data: bytes, limits: Limits) -> _BinaryMetadata:
    if len(data) < 56 or data[:8] != _MAGIC:
        raise FormatError("truncated header or bad binary magic")
    fields = struct.unpack_from("<HHBBHIIQQQQ", data, 8)
    kind_code, scalar_code, rank = fields[2], fields[3], fields[5]
    count, structure_count, payload_bytes, header_bytes = fields[7:]
    if fields[:2] != (1, 0) or fields[4] or fields[6]:
        raise FormatError("unsupported version, flags or reserved field")
    if kind_code not in range(1, 5) or scalar_code not in range(1, 13):
        raise FormatError("unsupported binary header field")
    if rank > limits.max_rank or header_bytes != 56 + 8 * rank:
        raise FormatError("rank/header length mismatch")
    if header_bytes > limits.max_header_bytes or len(data) < header_bytes:
        raise FormatError("header budget or truncated extents")
    shape = tuple(
        struct.unpack_from("<Q", data, 56 + 8 * i)[0] for i in range(rank))
    kind, scalar = _KINDS[kind_code - 1], _SCALARS[scalar_code - 1]
    expected = _metadata(kind, scalar, shape, count, limits)
    if structure_count != expected:
        raise FormatError("binary structure count mismatch")
    if payload_bytes != 8 * structure_count + _width(scalar) * count:
        raise FormatError("binary payload length mismatch")
    return _BinaryMetadata(kind, scalar, shape, count, structure_count,
                           payload_bytes, header_bytes)


def decode_binary_frame(
        data: bytes,
        limits: Limits = Limits(),
) -> tuple[Frame, int]:
    """Decodes one binary frame, validating length, structure and CRC-32.

    Returns:
      The validated frame and byte count through its four checksum bytes.

    Raises:
      FormatError: Header, size, structure, checksum or input limits fail.
    """
    _limits(limits)
    header = _binary_metadata(data, limits)
    end = header.header_bytes + header.payload_bytes
    if end + 4 > limits.max_input_bytes or len(data) < end + 4:
        raise FormatError("frame byte limit or truncated payload/checksum")
    if zlib.crc32(data[:end]) != struct.unpack_from("<I", data, end)[0]:
        raise FormatError("checksum mismatch")
    structure = tuple(
        struct.unpack_from("<Q", data, header.header_bytes + 8 * i)[0]
        for i in range(header.structure_count))
    start = header.header_bytes + 8 * header.structure_count
    width = _width(header.scalar)
    values = tuple(data[start + width * i:start + width * (i + 1)]
                   for i in range(header.count))
    frame = Frame(header.kind, header.scalar, header.shape, structure, values)
    validate_frame(frame, limits)
    return frame, end + 4


def decode_binary(data: bytes, limits: Limits = Limits()) -> Frame:
    """Decodes a single binary file with exact EOF after its checksum.

    Raises:
      FormatError: The frame is invalid or any trailing byte is present.
    """
    frame, consumed = decode_binary_frame(data, limits)
    if consumed != len(data):
        raise FormatError("trailing binary data")
    return frame


def encode_binary(frame: Frame, limits: Limits = Limits()) -> bytes:
    """Encodes validated scalar bytes without native object-layout assumptions.

    Raises:
      FormatError: Frame validation or output header/frame budget fails.
    """
    validate_frame(frame, limits)
    header_bytes = 56 + 8 * len(frame.shape)
    payload_bytes = (8 * len(frame.structure) +
                     _width(frame.scalar) * len(frame.values))
    if header_bytes > limits.max_header_bytes:
        raise FormatError("header byte limit")
    if header_bytes + payload_bytes + 4 > limits.max_input_bytes:
        raise FormatError("output byte limit")
    result = bytearray(_MAGIC)
    result.extend(
        struct.pack(
            "<HHBBHIIQQQQ",
            1,
            0,
            _KINDS.index(frame.kind) + 1,
            _SCALARS.index(frame.scalar) + 1,
            0,
            len(frame.shape),
            0,
            len(frame.values),
            len(frame.structure),
            payload_bytes,
            header_bytes,
        ))
    for extent in frame.shape:
        result.extend(struct.pack("<Q", extent))
    for integer in frame.structure:
        result.extend(struct.pack("<Q", integer))
    for value in frame.values:
        result.extend(value)
    result.extend(struct.pack("<I", zlib.crc32(result)))
    return bytes(result)


def main(argv: list[str] | None = None) -> int:
    """Validates a fixture with an explicit format, without autodetection.

    Returns:
      Zero for a valid fixture or one for a file/format failure. Argument
      errors use argparse's conventional nonzero exit status.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("format", choices=("text", "binary", "binary-hex"))
    parser.add_argument("fixture", type=pathlib.Path)
    args = parser.parse_args(argv)
    try:
        with args.fixture.open("rb") as source:
            data = source.read(Limits().max_input_bytes + 1)
        if len(data) > Limits().max_input_bytes:
            raise FormatError("fixture exceeds input byte limit")
        if args.format == "binary-hex":
            data = bytes.fromhex(data.decode("ascii"))
        frame = decode_text(data) if args.format == "text" else decode_binary(
            data)
    except (OSError, ValueError, UnicodeError) as error:
        print(f"invalid fixture: {error}", file=sys.stderr)
        return 1
    print(f"valid {args.format}: {frame.kind} {frame.scalar} "
          f"shape={frame.shape} values={len(frame.values)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
