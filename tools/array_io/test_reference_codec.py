"""Tests independent array fixtures and strict reference-codec failures."""

from __future__ import annotations

import dataclasses
import pathlib
import struct
import subprocess
import sys
import unittest
import zlib

from tools.array_io import reference_codec

_ROOT = pathlib.Path(__file__).resolve().parents[2]
_FIXTURES = _ROOT / "tests" / "array_io" / "fixtures"


def _recurrence(data: bytes) -> int:
    """Computes the contract bit recurrence independently of codec's zlib."""
    state = 0xffffffff
    for value in data:
        state ^= value
        for _ in range(8):
            state = (state >> 1) ^ (0xedb88320 if state & 1 else 0)
    return state ^ 0xffffffff


def _replace_header(data: bytes, offset: int, encoding: str,
                    value: int) -> bytes:
    result = bytearray(data)
    struct.pack_into(encoding, result, offset, value)
    result[-4:] = _recurrence(result[:-4]).to_bytes(4, "little")
    return bytes(result)


class ReferenceCodecTest(unittest.TestCase):
    """Independent fixtures, exact framing and adversarial mutations."""

    def setUp(self):
        self.text = (_FIXTURES / "dense-f64.asc").read_bytes()
        self.binary = bytes.fromhex(
            (_FIXTURES / "dense-i16.hex").read_text(encoding="ascii"))

    def test_hand_derived_dense_text(self):
        """Checks independently listed dimension-zero-fastest values."""
        frame = reference_codec.decode_text(self.text)
        self.assertEqual((frame.kind, frame.scalar, frame.shape),
                         ("dense", "f64", (2, 3)))
        self.assertEqual(frame.structure, ())
        self.assertEqual(
            frame.values,
            tuple(struct.pack("<d", value) for value in (1, 4, 2, 5, 3, 6)))
        self.assertEqual(reference_codec.encode_text(frame), self.text)

    def test_hand_derived_binary_and_crc(self):
        """Checks envelope bytes and independent CRC recurrence/check values."""
        self.assertEqual(_recurrence(b"123456789"), 0xcbf43926)
        self.assertEqual(zlib.crc32(b"123456789"), 0xcbf43926)
        self.assertEqual(_recurrence(self.binary[:-4]), 0xcc139fd1)
        frame = reference_codec.decode_binary(self.binary)
        self.assertEqual(
            frame,
            reference_codec.Frame("dense", "i16", (2,), (),
                                  (b"\xfe\xff", b"\x01\x02")))
        self.assertEqual(reference_codec.encode_binary(frame), self.binary)

    def test_rank_zero_and_stored_zero(self):
        """Retains rank-zero scalar and stored-zero distinctions."""
        frame = reference_codec.decode_text(
            (_FIXTURES / "coo-rank-zero.asc").read_bytes())
        self.assertEqual(frame.shape, ())
        self.assertEqual(frame.values, (b"\0",))
        self.assertEqual(frame.structure, ())
        empty = dataclasses.replace(frame, values=())
        self.assertEqual(
            reference_codec.decode_binary(reference_codec.encode_binary(empty)),
            empty)
        with self.assertRaises(reference_codec.FormatError):
            reference_codec.validate_frame(
                dataclasses.replace(frame, values=(b"\0", b"\0")))
        dense = dataclasses.replace(frame, kind="dense")
        self.assertEqual(
            reference_codec.decode_text(reference_codec.encode_text(dense)),
            dense)

    def test_all_wire_scalars_and_complex_bits(self):
        """Exercises all scalar codes, extrema and complex NaN payloads."""
        samples = {
            "i8": ("-128", "127"),
            "u8": ("0", "255"),
            "i16": ("-32768", "32767"),
            "u16": ("0", "65535"),
            "i32": ("-2147483648", "2147483647"),
            "u32": ("0", "4294967295"),
            "i64": ("-9223372036854775808", "9223372036854775807"),
            "u64": ("0", "18446744073709551615"),
            "f32": ("-0", "1.40129846e-45", "3.40282347e+38", "inf", "nan"),
            "f64": ("-0", "4.9406564584124654e-324", "1.7976931348623157e+308"),
            "c64": ("(-0,inf)", "(nan,-inf)", "(1.25,-2.5)"),
            "c128": ("(-0,1e-300)", "(1.25,-2.5)"),
        }
        for scalar, tokens in samples.items():
            with self.subTest(scalar=scalar):
                values = tuple(
                    reference_codec.scalar_bytes(token, scalar)
                    for token in tokens)
                frame = reference_codec.Frame("dense", scalar, (len(tokens),),
                                              (), values)
                self.assertEqual(
                    reference_codec.decode_binary(
                        reference_codec.encode_binary(frame)), frame)
                self.assertEqual(
                    reference_codec.decode_text(
                        reference_codec.encode_text(frame)), frame)
        raw = reference_codec.Frame(
            "dense", "c128", (), (),
            (bytes.fromhex("420000000000f8ff230100000000f87f"),))
        self.assertEqual(
            reference_codec.decode_binary(reference_codec.encode_binary(raw)),
            raw)

    def test_independent_ieee_rounding(self):
        """Distinguishes ties-even rounding from double-rounding errors."""
        # 1 + 2^-24 is halfway between binary32 1 and its next neighbor.
        halfway = "1.000000059604644775390625"
        above = "1.000000059604644775390625000000000000000000000000001"
        self.assertEqual(reference_codec.scalar_bytes(halfway, "f32"),
                         bytes.fromhex("0000803f"))
        self.assertEqual(reference_codec.scalar_bytes(above, "f32"),
                         bytes.fromhex("0100803f"))
        self.assertEqual(reference_codec.scalar_bytes("-0.0", "f64"),
                         bytes.fromhex("0000000000000080"))

    def test_invalid_scalar_syntax_range_and_length(self):
        """Rejects invalid grammar, narrowing, overflow and token exhaustion."""
        bad = (("u8", "-0"), ("u64", "18446744073709551616"), ("i8", "128"),
               ("i8", "-129"), ("i32", "1.0"), ("f64", "infinity"),
               ("f64", "+inf"), ("f64", "-nan"), ("f64", "0x1p0"),
               ("f32", "1e39"), ("f32", "1e-46"), ("f64", "1e999999999999"),
               ("f64", "1e-999999999999"), ("c64", "(1, 2)"),
               ("c128", "(1,2,3)"), ("f64", "1" * 257), ("bool", "1"))
        for scalar, token in bad:
            with self.subTest(scalar=scalar, token=token):
                with self.assertRaises(reference_codec.FormatError):
                    reference_codec.scalar_bytes(token, scalar)

    def test_empty_shape_checks_every_extent(self):
        """An empty dimension never exempts later extents from validation."""
        frame = reference_codec.Frame("dense", "f64", (0, 3), (), ())
        self.assertEqual(
            reference_codec.decode_binary(reference_codec.encode_binary(frame)),
            frame)
        with self.assertRaises(reference_codec.FormatError):
            reference_codec.validate_frame(
                dataclasses.replace(frame, shape=(0, 1 << 63)))
        with self.assertRaises(reference_codec.FormatError):
            reference_codec.validate_frame(
                dataclasses.replace(frame, shape=(0,) * 33))

    def test_coo_comparator_is_dimension_zero_first(self):
        """Checks tuple lexicographic ordering, distinct from Dense order."""
        frame = reference_codec.Frame("coo", "i8", (2, 3), (0, 2, 1, 0),
                                      (b"\0", b"\x07"))
        self.assertEqual(
            reference_codec.decode_text(reference_codec.encode_text(frame)),
            frame)
        for structure in ((1, 0, 0, 2), (0, 2, 0, 2), (0, 3, 1, 0)):
            with self.subTest(structure=structure):
                with self.assertRaises(reference_codec.FormatError):
                    reference_codec.validate_frame(
                        dataclasses.replace(frame, structure=structure))

    def test_csr_csc_canonical_offsets(self):
        """Validates compressed endpoints, monotonicity and inner ordering."""
        for kind in ("csr", "csc"):
            frame = reference_codec.Frame(kind, "i8", (2, 2), (0, 1, 2, 1, 0),
                                          (b"\0", b"\x07"))
            self.assertEqual(
                reference_codec.decode_binary(
                    reference_codec.encode_binary(frame)), frame)
            self.assertEqual(
                reference_codec.decode_text(reference_codec.encode_text(frame)),
                frame)
            for structure in ((1, 1, 2, 1, 0), (0, 2, 1, 1, 0), (0, 1, 3, 1, 0),
                              (0, 2, 2, 1, 1), (0, 1, 2, 2, 0)):
                with self.subTest(kind=kind, structure=structure):
                    with self.assertRaises(reference_codec.FormatError):
                        reference_codec.validate_frame(
                            dataclasses.replace(frame, structure=structure))
            empty = reference_codec.Frame(kind, "f32", (0, 0), (0,), ())
            self.assertEqual(
                reference_codec.decode_text(reference_codec.encode_text(empty)),
                empty)

    def test_every_truncation_boundary(self):
        """Rejects every incomplete prefix of each independent tiny fixture."""
        for data, decoder in ((self.text, reference_codec.decode_text),
                              (self.binary, reference_codec.decode_binary)):
            for boundary in range(len(data)):
                with self.subTest(boundary=boundary, binary=data
                                  is self.binary):
                    with self.assertRaises(reference_codec.FormatError):
                        decoder(data[:boundary])

    def test_every_binary_byte_corruption(self):
        """Rejects a bit flip at every byte including metadata and checksum."""
        for offset in range(len(self.binary)):
            damaged = bytearray(self.binary)
            damaged[offset] ^= 1
            with self.subTest(offset=offset):
                with self.assertRaises(reference_codec.FormatError):
                    reference_codec.decode_binary(bytes(damaged))

    def test_header_corruption_with_valid_checksum(self):
        """Semantic header rejection remains required after recomputing CRC."""
        changes = ((8, "<H", 2), (10, "<H", 1), (12, "<B", 0), (13, "<B", 13),
                   (14, "<H", 1), (16, "<I", 0xffffffff), (20, "<I", 1),
                   (24, "<Q", 1), (32, "<Q", 1), (40, "<Q", 5), (48, "<Q", 65),
                   (56, "<Q", 1 << 63))
        for offset, encoding, value in changes:
            with self.subTest(offset=offset):
                with self.assertRaises(reference_codec.FormatError):
                    reference_codec.decode_binary(
                        _replace_header(self.binary, offset, encoding, value))

    def test_text_header_and_extra_record_rejection(self):
        """Rejects mismatched metadata, invalid separators and extra records."""
        changes = ((b"ASCARRAY 1", b"ASCARRAY 2"), (b"dense", b"Dense"),
                   (b"rank 2", b"rank 33"), (b"shape 2 3", b"shape 2 3 4"),
                   (b"shape 2 3",
                    b"shape 2  3"), (b"dim0", b"coo"), (b"count 6", b"count 7"),
                   (b"count 6", b"count -6"), (b"data\n", b"data\n#comment\n"),
                   (b"end\n", b"end\nextra\n"), (b"scalar f64",
                                                 b"scalar i32\nextra value"))
        for old, new in changes:
            with self.subTest(new=new):
                with self.assertRaises(reference_codec.FormatError):
                    reference_codec.decode_text(self.text.replace(old, new))

    def test_framing_crlf_and_trailing_policies(self):
        """Checks adjacent-frame boundaries and text/binary EOF policies."""
        frame, consumed = reference_codec.decode_text_frame(self.text * 2)
        self.assertEqual(consumed, len(self.text))
        self.assertEqual(
            reference_codec.decode_text_frame((self.text * 2)[consumed:])[0],
            frame)
        self.assertEqual(reference_codec.decode_text(self.text + b" \t\r\n"),
                         frame)
        self.assertEqual(
            reference_codec.decode_text(self.text.replace(b"\n", b"\r\n")),
            frame)
        self.assertEqual(
            reference_codec.decode_binary_frame(self.binary * 2)[1],
            len(self.binary))
        for bad, decoder in ((self.text * 2, reference_codec.decode_text),
                             (self.text + b"\r", reference_codec.decode_text),
                             (self.binary + b"\n",
                              reference_codec.decode_binary),
                             (self.binary * 2, reference_codec.decode_binary)):
            with self.assertRaises(reference_codec.FormatError):
                decoder(bad)

    def test_resource_limits(self):
        """Each bounded-fixture resource cap independently causes rejection."""
        for name, maximum in (("max_input_bytes", 10), ("max_header_bytes", 20),
                              ("max_rank", 1), ("max_extent",
                                                2), ("max_logical_elements", 5),
                              ("max_decoded_bytes", 47), ("max_token_bytes",
                                                          0)):
            with self.subTest(name=name):
                with self.assertRaises(reference_codec.FormatError):
                    reference_codec.decode_text(
                        self.text,
                        dataclasses.replace(reference_codec.Limits(),
                                            **{name: maximum}))
        with self.assertRaises(reference_codec.FormatError):
            reference_codec.decode_text(self.text,
                                        reference_codec.Limits(max_rank=-1))

    def test_cli_help_valid_and_nonzero_failure(self):
        """The CLI distinguishes help, valid fixtures and nonzero errors."""
        script = _ROOT / "tools" / "array_io" / "reference_codec.py"
        for arguments, expected in ((["--help"], 0), ([
                "text", str(_FIXTURES / "dense-f64.asc")
        ], 0), (["binary-hex", str(_FIXTURES / "dense-i16.hex")],
                0), (["binary", str(_FIXTURES / "dense-f64.asc")], 1)):
            process = subprocess.run(
                [sys.executable, str(script), *arguments],
                check=False,
                capture_output=True,
                text=True)
            self.assertEqual(process.returncode, expected, process.stderr)


if __name__ == "__main__":
    unittest.main()
