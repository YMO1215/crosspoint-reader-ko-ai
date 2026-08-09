#!/usr/bin/env python3
"""Convert a Korean-build `.epdfont` file into upstream's `.cpfont` (v4).

Why this exists
---------------
The Korean fork used to ship its own SD font format (`.epdfont`).  Upstream
1.5.0 reads `.cpfont` instead, so fonts already on an SD card stop working
after the update.  If you still have the original TTF, **regenerate with
`fontconvert_sdcard.py` instead** — it produces kerning, ligatures and
sub-pixel advances that `.epdfont` never stored.  Use this converter only
when the TTF is gone and the `.epdfont` is all that is left.

What survives, what does not
----------------------------
  survives : glyph bitmaps, unicode intervals, per-glyph metrics,
             line height / ascender / descender
  lost     : nothing that was in the source file — but the result has
             no kerning, no ligatures, and integer-pixel advances,
             because `.epdfont` never carried those.

Format notes (the three things that actually differ)
----------------------------------------------------
  1. advanceX  .epdfont stores integer pixels (uint8).
               .cpfont stores 12.4 fixed-point (uint16) → value << 4.
  2. bitmaps   .cpfont is *always* 2-bit greyscale.  A 1-bit source is
               expanded here (bit set → level 3 = black).
  3. layout    .cpfont supports several styles per file behind a TOC.
               A converted file has exactly one (regular).

Usage
-----
    python epdfont_to_cpfont.py IN.epdfont -o OUT.cpfont
    python epdfont_to_cpfont.py *.epdfont --output-dir ./out/
"""

from __future__ import annotations

import argparse
import glob
import os
import struct
import sys

# ── .epdfont (source) ────────────────────────────────────────────────────
EPD_MAGIC = 0x46445045  # "EPDF"
EPD_HEADER = "<IHBBBbbBIIIII"          # 32 bytes
EPD_INTERVAL = "<III"                  # 12 bytes
EPD_GLYPH = "<BBBBhhII"                # 16 bytes

# ── .cpfont (target, v4) ─────────────────────────────────────────────────
CP_MAGIC = b"CPFONT\x00\x00"
CP_VERSION = 4
CP_HEADER = "<8sHHB19s"                # 32 bytes
CP_TOC = "<B3xIIBhhHHBBBI4x"           # 32 bytes
CP_GLYPH = "<BBHhhH2xI"                # 16 bytes
CP_STYLE_REGULAR = 0

for _fmt, _size in ((EPD_HEADER, 32), (EPD_INTERVAL, 12), (EPD_GLYPH, 16),
                    (CP_HEADER, 32), (CP_TOC, 32), (CP_GLYPH, 16)):
    assert struct.calcsize(_fmt) == _size, (_fmt, struct.calcsize(_fmt), _size)


class ConvertError(RuntimeError):
    pass


def expand_1bit_to_2bit(data: bytes, width: int, height: int) -> bytes:
    """1-bit → 2-bit.  A set bit becomes level 3 (darkest).

    ⚠️ Both formats pack the glyph as **one continuous bit stream**, MSB first,
    with no per-row byte padding — only the very last byte is padded.  Treating
    rows as byte-aligned (the obvious guess) shears the glyph and reads past the
    end of short bitmaps.  Verified against the genuine files: a 3x18 glyph is
    14 bytes at 2-bit (3*18*2 = 108 bits), not 18.

    Polarity matches the generators: 1-bit set = ink, 2-bit 3 = darkest.
    """
    count = width * height
    need = (count + 7) // 8
    if len(data) < need:
        raise ConvertError("1-bit bitmap is %d bytes, need %d for %dx%d" % (len(data), need, width, height))
    out = bytearray((count * 2 + 7) // 8)
    for i in range(count):
        if (data[i >> 3] >> (7 - (i & 7))) & 1:
            out[i >> 2] |= 3 << (6 - 2 * (i & 3))
    return bytes(out)


def convert(src_path: str, dst_path: str, *, quiet: bool = False) -> int:
    raw = open(src_path, "rb").read()
    if len(raw) < 32:
        raise ConvertError("file is too short to hold a header")

    (magic, version, is_2bit, _r1, advance_y, ascender, descender, _r2,
     interval_count, glyph_count,
     intervals_off, glyphs_off, bitmap_off) = struct.unpack_from(EPD_HEADER, raw, 0)

    if magic != EPD_MAGIC:
        raise ConvertError("not an .epdfont file (magic 0x%08X)" % magic)
    if version != 1:
        raise ConvertError("unsupported .epdfont version %d" % version)

    # ── intervals: same 12-byte layout, copy straight through ──
    need = intervals_off + interval_count * 12
    if need > len(raw):
        raise ConvertError("intervals run past end of file")
    intervals_blob = raw[intervals_off:need]

    # ── glyphs: repack, and rebuild the bitmap section as we go ──
    glyphs_blob = bytearray()
    bitmap_blob = bytearray()
    max_advance = 0
    for i in range(glyph_count):
        off = glyphs_off + i * 16
        if off + 16 > len(raw):
            raise ConvertError("glyph %d runs past end of file" % i)
        (w, h, advance_x, _pad, left, top, data_len, data_off) = struct.unpack_from(EPD_GLYPH, raw, off)

        start = bitmap_off + data_off
        if start + data_len > len(raw):
            raise ConvertError("glyph %d bitmap runs past end of file" % i)
        bits = raw[start:start + data_len]
        if not is_2bit and data_len:
            bits = expand_1bit_to_2bit(bits, w, h)

        # ⚠️ .cpfont stores dataLength in a uint16. A single glyph over 64 KB
        #    cannot be represented — refuse instead of silently truncating.
        if len(bits) > 0xFFFF:
            raise ConvertError("glyph %d is %d bytes; .cpfont caps a glyph at 65535" % (i, len(bits)))

        # ⚠️ integer pixels → 12.4 fixed-point
        advance_fp = advance_x << 4
        max_advance = max(max_advance, advance_fp)

        glyphs_blob += struct.pack(CP_GLYPH, w, h, advance_fp, left, top, len(bits), len(bitmap_blob))
        bitmap_blob += bits

    # ── assemble ──
    # Section order per style: intervals, glyphs, kernL, kernR, kernMatrix,
    # ligatures, bitmaps. This file has no kerning/ligatures, so those are empty.
    style_payload = bytes(intervals_blob) + bytes(glyphs_blob) + bytes(bitmap_blob)
    data_offset = 32 + 32  # header + one TOC entry

    header = struct.pack(CP_HEADER, CP_MAGIC, CP_VERSION, 1, 1, bytes(19))
    toc = struct.pack(CP_TOC,
                      CP_STYLE_REGULAR,
                      interval_count, glyph_count,
                      advance_y, ascender, descender,
                      0, 0,      # kerning class entry counts
                      0, 0,      # kerning class counts
                      0,         # ligature pairs
                      data_offset)

    os.makedirs(os.path.dirname(dst_path) or ".", exist_ok=True)
    with open(dst_path, "wb") as f:
        f.write(header)
        f.write(toc)
        f.write(style_payload)

    if not quiet:
        print("%s -> %s" % (os.path.basename(src_path), os.path.basename(dst_path)))
        print("  %d intervals, %d glyphs, %s source"
              % (interval_count, glyph_count, "2-bit" if is_2bit else "1-bit (expanded)"))
        print("  advanceY=%d ascender=%d descender=%d  max advanceX=%.2f px"
              % (advance_y, ascender, descender, max_advance / 16))
        print("  %d bytes" % (32 + 32 + len(style_payload)))
    return 32 + 32 + len(style_payload)


def main() -> None:
    ap = argparse.ArgumentParser(description="Convert .epdfont (Korean fork) to .cpfont (upstream v4).")
    ap.add_argument("inputs", nargs="+", help=".epdfont file(s); shell globs are expanded")
    ap.add_argument("-o", "--output", help="output path (single input only)")
    ap.add_argument("--output-dir", help="write <name>.cpfont into this directory")
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args()

    paths: list[str] = []
    for pattern in args.inputs:
        hits = glob.glob(pattern)
        paths.extend(hits if hits else [pattern])

    if args.output and len(paths) > 1:
        sys.exit("-o takes a single input; use --output-dir for several")

    failures = 0
    for src in paths:
        if args.output:
            dst = args.output
        else:
            base = os.path.splitext(os.path.basename(src))[0] + ".cpfont"
            dst = os.path.join(args.output_dir or os.path.dirname(src) or ".", base)
        try:
            convert(src, dst, quiet=args.quiet)
        except (ConvertError, OSError, struct.error) as exc:
            print("FAILED %s: %s" % (src, exc), file=sys.stderr)
            failures += 1

    if failures:
        sys.exit("%d file(s) failed" % failures)


if __name__ == "__main__":
    main()
