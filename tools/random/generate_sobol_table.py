#!/usr/bin/env python3
"""Generate asc-cpp's compact Sobol parameter table from pinned Joe/Kuo data.

This is an original offline build-maintenance tool. It is deliberately absent
from the configure, build, install, and consumer paths.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


EXPECTED_INPUT_SHA256 = (
    "68eedd2a4e3b659b9695e7aff0f8ac68718bcf620730fc3d3a8c65df2a067441"
)
EXPECTED_FIRST_DIMENSION = 2
EXPECTED_LAST_DIMENSION = 21201
WORD_COUNT = 64
FNV_OFFSET_BASIS = 14695981039346656037
FNV_PRIME = 1099511628211
UINT64_MASK = (1 << 64) - 1


class InputError(ValueError):
    """The pinned input does not satisfy the frozen grammar."""


def parse_rows(path: Path) -> list[tuple[int, int, tuple[int, ...]]]:
    payload = path.read_bytes()
    observed = hashlib.sha256(payload).hexdigest()
    if observed != EXPECTED_INPUT_SHA256:
        raise InputError(
            f"input SHA-256 is {observed}, expected {EXPECTED_INPUT_SHA256}"
        )
    try:
        lines = payload.decode("ascii").splitlines()
    except UnicodeDecodeError as error:
        raise InputError("input is not ASCII") from error
    if not lines or lines[0].split() != ["d", "s", "a", "m_i"]:
        raise InputError("input header is not 'd s a m_i'")

    rows: list[tuple[int, int, tuple[int, ...]]] = []
    for line_number, line in enumerate(lines[1:], start=2):
        fields = line.split()
        if len(fields) < 4 or any(not field.isdecimal() for field in fields):
            raise InputError(f"line {line_number} has invalid unsigned fields")
        values = [int(field) for field in fields]
        dimension, degree, coefficient = values[:3]
        initial = tuple(values[3:])
        expected_dimension = EXPECTED_FIRST_DIMENSION + len(rows)
        if dimension != expected_dimension:
            raise InputError(
                f"line {line_number} has dimension {dimension}, "
                f"expected {expected_dimension}"
            )
        if not 1 <= degree <= 31:
            raise InputError(f"line {line_number} has invalid degree {degree}")
        if coefficient >= (1 << (degree - 1)):
            raise InputError(
                f"line {line_number} coefficient does not fit degree {degree}"
            )
        if len(initial) != degree:
            raise InputError(
                f"line {line_number} has {len(initial)} initial values, "
                f"expected {degree}"
            )
        for index, value in enumerate(initial, start=1):
            if value == 0 or value % 2 == 0 or value >= (1 << index):
                raise InputError(
                    f"line {line_number} has invalid m_{index} value {value}"
                )
        rows.append((degree, coefficient, initial))

    expected_count = EXPECTED_LAST_DIMENSION - EXPECTED_FIRST_DIMENSION + 1
    if len(rows) != expected_count:
        raise InputError(f"input has {len(rows)} rows, expected {expected_count}")
    return rows


def direction_words(
    dimension: int, rows: list[tuple[int, int, tuple[int, ...]]]
) -> list[int]:
    if dimension == 0:
        return [1 << (63 - index) for index in range(WORD_COUNT)]
    degree, coefficient, initial = rows[dimension - 1]
    words = [0] * WORD_COUNT
    for index, value in enumerate(initial):
        words[index] = value << (63 - index)
    for index in range(degree, WORD_COUNT):
        word = words[index - degree] ^ (words[index - degree] >> degree)
        for offset in range(1, degree):
            if (coefficient >> (degree - 1 - offset)) & 1:
                word ^= words[index - offset]
        words[index] = word
    return words


def table_checksum(rows: list[tuple[int, int, tuple[int, ...]]]) -> int:
    checksum = FNV_OFFSET_BASIS
    for dimension in range(EXPECTED_LAST_DIMENSION):
        for word in direction_words(dimension, rows):
            for shift in range(0, 64, 8):
                checksum ^= (word >> shift) & 0xFF
                checksum = (checksum * FNV_PRIME) & UINT64_MASK
    return checksum


def format_array(name: str, cpp_type: str, values: list[int]) -> str:
    lines = [
        f"constexpr std::array<{cpp_type}, {len(values)}> {name} = {{{{"
    ]
    width = 12
    for start in range(0, len(values), width):
        suffix = "," if start + width < len(values) else ""
        lines.append(
            "    " + ", ".join(str(value) for value in values[start : start + width]) + suffix
        )
    lines.append("}};")
    return "\n".join(lines)


def render(rows: list[tuple[int, int, tuple[int, ...]]]) -> str:
    degrees = [row[0] for row in rows]
    coefficients = [row[1] for row in rows]
    offsets = [0]
    initial_values: list[int] = []
    for _, _, initial in rows:
        initial_values.extend(initial)
        offsets.append(len(initial_values))
    checksum = table_checksum(rows)
    arrays = "\n\n".join(
        [
            format_array("kDegrees", "std::uint8_t", degrees),
            format_array("kCoefficients", "std::uint32_t", coefficients),
            format_array("kInitialOffsets", "std::uint32_t", offsets),
            format_array("kInitialValues", "std::uint32_t", initial_values),
        ]
    )
    return f'''// clang-format off
// Generated by tools/random/generate_sobol_table.py. Do not edit.
// Source: Joe and Kuo D(6), dimensions 2 through 21201.
// Source SHA-256: {EXPECTED_INPUT_SHA256}

#include "asc/random/quasi.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "asc/core/status.h"

namespace asc {{
namespace {{

{arrays}

}}  // namespace

Status InitializeSobolDirectionNumbers(
    std::size_t dimension,
    std::span<std::uint64_t> direction_words) {{
  if (dimension >= kSobolDimensionCount) {{
    return Status(ErrorCode::kIndex, "Sobol dimension is out of range");
  }}
  if (direction_words.size() != kSobolDirectionWordCount) {{
    return Status(ErrorCode::kShape,
                  "Sobol direction workspace must contain 64 words");
  }}
  if (dimension == 0) {{
    for (std::size_t index = 0; index < kSobolDirectionWordCount; ++index) {{
      direction_words[index] = std::uint64_t{{1}} << (63U - index);
    }}
    return Status::Ok();
  }}

  const std::size_t row = dimension - 1;
  const std::size_t degree = kDegrees[row];
  const std::uint32_t coefficient = kCoefficients[row];
  const std::size_t initial_offset = kInitialOffsets[row];
  for (std::size_t index = 0; index < degree; ++index) {{
    direction_words[index] =
        static_cast<std::uint64_t>(kInitialValues[initial_offset + index])
        << (63U - index);
  }}
  for (std::size_t index = degree; index < kSobolDirectionWordCount; ++index) {{
    std::uint64_t word = direction_words[index - degree] ^
                         (direction_words[index - degree] >> degree);
    for (std::size_t offset = 1; offset < degree; ++offset) {{
      const std::size_t bit = degree - 1 - offset;
      if (((coefficient >> bit) & 1U) != 0U) {{
        word ^= direction_words[index - offset];
      }}
    }}
    direction_words[index] = word;
  }}
  return Status::Ok();
}}

std::uint64_t SobolDirectionTableChecksum() noexcept {{
  return 0x{checksum:016X}ULL;
}}

}}  // namespace asc
// clang-format on
'''


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--verify", action="store_true", help="fail instead of replacing drift"
    )
    arguments = parser.parse_args()
    rendered = render(parse_rows(arguments.input))
    rendered_bytes = rendered.encode("utf-8")
    if arguments.verify:
        if (
            not arguments.output.exists()
            or arguments.output.read_bytes() != rendered_bytes
        ):
            raise SystemExit(f"generated output differs: {arguments.output}")
        return 0
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(rendered_bytes)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
