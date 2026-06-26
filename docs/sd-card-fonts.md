# SD Card Fonts

The Korean AI build uses the Korean fork's `.epdfont` SD-card font engine.
This is intentional: large Korean/CJK fonts are more stable with the
on-demand interval lookup and glyph cache isolation implemented in
`crosspoint-reader-ko`.

## Supported Format

- Supported: `.epdfont`
- Not used in this build: upstream `.cpfont`

`.epdfont` files can be selected separately for:

- Reader body font
- System/UI font

When a system/UI SD font does not contain a glyph, the renderer falls back to
the built-in Pretendard UI font on a per-glyph basis.

## Installing Fonts

Copy `.epdfont` files to one of these SD-card locations:

```text
/.crosspoint/fonts/
/.fonts/
/fonts/
/fonts/<Family>/
```

The font picker scans the roots above, including one level of per-family
folders under `/fonts`.

Example:

```text
SD Card Root/
├── .fonts/
│   └── RidiBatang_14.epdfont
└── fonts/
    └── MyFamily/
        ├── MyFamily_12.epdfont
        └── MyFamily_14.epdfont
```

After copying fonts, insert the SD card and open the device font settings.

## Converting Fonts

Use the `.epdfont` converter scripts kept with this firmware:

```bash
python3 lib/EpdFont/scripts/ttf_to_epdfont.py MyFont 14 MyFont-Regular.ttf -o MyFont_14.epdfont
```

For details about the binary format and fixed-point metric compatibility, see
[sd-font-format.md](./sd-font-format.md).

## Stability Notes

- Large CJK fonts must not allocate the full interval table in RAM.
- Glyph bitmap cache keys include the font identity to prevent cross-font Hanja corruption.
- XTC rendering must remain usable while large SD fonts are selected.
