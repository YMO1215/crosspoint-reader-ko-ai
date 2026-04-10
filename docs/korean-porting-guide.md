# Korean Porting Guide

## Purpose

This document defines a repeatable process for rebuilding and maintaining the Korean fork of `crosspoint-reader` on top of stable upstream releases.

The goal is not to maintain a permanently divergent codebase. The goal is to keep a small, well-understood Korean feature set that can be reapplied to each stable upstream release with minimal risk.

## Core Rules

- Do not merge `upstream/master` directly into the Korean branch.
- Always start from an upstream release tag.
- Treat Korean-specific behavior as a small patch set, not as a separate product line.
- Do not keep repairing a mixed branch after headers and implementations from different generations have already been combined.
- Port Korean features in small functional groups and build after each group.

## Current Proven Baselines

### Upstream

- Stable upstream release verified during this analysis: `1.2.0`
- Tag: `e6c6e72`

### Korean fork

- Last stable Korean release identified during this analysis: `1.1.1-ko.1`
- Tag / commit: `2937d8c`

## Observed Release Mapping Pattern

The Korean fork has historically been built on stable upstream releases, not on arbitrary upstream `master` snapshots.

| Korean version | Base upstream version | Notes |
| --- | --- | --- |
| `0.15.0-ko.0` | `0.15.0` | Early Korean patch baseline |
| `0.16.0-ko.0` | `0.16.0` | Upstream features merged, Korean changes preserved |
| `1.0.0-ko.0` | `1.0.0` | Major structural adaptation |
| `1.0.0-ko.1` | `1.0.0` | Stabilization after merge |
| `1.1.1-ko.0` | `1.1.1` | Upstream 1.1.1 merged |
| `1.1.1-ko.1` | `1.1.1` | CI / OTA / settings stabilization |

This pattern should continue.

## Why the Current Mixed Branch Fails

The broken `release/korean` branch failed because it mixed:

- newer upstream headers
- older Korean implementation files
- partially adapted font renderer changes
- EPUB layout extensions from Korean commits

Typical failure patterns that were observed:

- `ParsedText.h` and `ParsedText.cpp` had different function signatures
- `Section.h` and `Section.cpp` disagreed on `paragraphIndent` and `characterWrap`
- `TextBlock` constructor argument types no longer matched caller expectations
- `GfxRenderer` used mixed assumptions around `UnifiedFontFamily`

This means the problem is architectural mismatch, not a small compile bug.

## Korean Feature Packs

The Korean fork can be understood as five functional packs.

### Pack 1. UI / OTA / CI

Purpose:

- preserve Korean update source
- keep Korean-facing release behavior
- keep Korean UI additions stable

Main files:

- `.github/workflows/release.yml`
- `platformio.ini`
- `src/network/OtaUpdater.cpp`

Representative commits:

- `fed95ce` update OTA URL for org transfer
- `133a745` fix CI failures

### Pack 2. Custom Font Selection and Persistence

Purpose:

- allow selecting fonts from SD card
- persist selected font path across reboot/sleep

Main files:

- `src/activities/settings/FontSelectionActivity.cpp`
- `src/activities/settings/FontSelectionActivity.h`
- `src/activities/settings/CategorySettingsActivity.cpp`
- `src/activities/settings/CategorySettingsActivity.h`
- `src/JsonSettingsIO.cpp`
- `src/CrossPointSettings.cpp`
- `src/CrossPointSettings.h`

Representative commit:

- `58fbab9` persist `customFontPath`, `characterWrap`, `paragraphIndent`

### Pack 3. Korean Default EPUB Font

Purpose:

- replace default EPUB font with KoPub Batang
- register fallback/default font consistently

Main files:

- `lib/EpdFont/builtinFonts/all.h`
- `lib/EpdFont/builtinFonts/kopub_14_regular.h`
- `src/main.cpp`
- `src/fontIds.h`
- `src/CrossPointSettings.cpp`

Representative commit:

- `683e437` replace default font with KoPub Batang and add indent option

### Pack 4. Independent First-Line Indent (`paragraphIndent`)

Purpose:

- allow first-line indent independent of paragraph spacing

Main files:

- `lib/Epub/Epub/ParsedText.cpp`
- `lib/Epub/Epub/ParsedText.h`
- `lib/Epub/Epub/Section.cpp`
- `lib/Epub/Epub/Section.h`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h`
- `src/CrossPointSettings.cpp`
- `src/CrossPointSettings.h`
- `src/SettingsList.h`

Representative commit:

- `683e437`

### Pack 5. Korean Character-Level Wrapping (`characterWrap`)

Purpose:

- improve justified Korean text layout by allowing character-level wrapping when word-level spacing would become visually bad

Main files:

- `lib/Epub/Epub/ParsedText.cpp`
- `lib/Epub/Epub/ParsedText.h`
- `lib/Epub/Epub/Section.cpp`
- `lib/Epub/Epub/Section.h`
- `lib/Epub/Epub/blocks/TextBlock.cpp`
- `lib/Epub/Epub/blocks/TextBlock.h`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h`
- `lib/GfxRenderer/GfxRenderer.cpp`
- `lib/GfxRenderer/GfxRenderer.h`
- `src/CrossPointSettings.cpp`
- `src/CrossPointSettings.h`
- `src/activities/reader/EpubReaderActivity.cpp`
- `src/activities/reader/TxtReaderActivity.cpp`
- `src/SettingsList.h`

Representative commit:

- `62c3b3f` add character-level line wrapping for Korean text

## Recommended Rebuild Strategy for `1.2.0-ko`

### Step 1. Start from upstream stable tag

```bash
git fetch upstream --tags
git checkout -b rebuild/1.2.0-ko e6c6e72
```

Do not start from current `release/korean`.

### Step 2. Use `1.1.1-ko.1` as the Korean reference

Reference points:

- Korean stable reference: `2937d8c`
- Feature commits to inspect carefully:
  - `683e437`
  - `62c3b3f`
  - `58fbab9`
  - `fed95ce`
  - `133a745`

### Step 3. Port in this exact order

1. CI / OTA / Korean release settings
2. new settings fields and persistence
3. font selection UI and custom font path
4. KoPub Batang default font wiring
5. `paragraphIndent`
6. `characterWrap`
7. only then reconcile EPUB layout internals and `GfxRenderer`

This order matters because it keeps failure scope narrow.

## Build / Verification Gates

Run a build after each feature pack.

```bash
pio run
```

### After settings persistence pack

Verify:

- `customFontPath` survives reboot/sleep
- `paragraphIndent` survives reboot/sleep
- `characterWrap` survives reboot/sleep

### After font selection pack

Verify:

- default font is KoPub Batang
- SD font selection works
- chosen font name is displayed correctly

### After EPUB layout packs

Verify:

- `paragraphIndent` toggles visually
- `characterWrap` toggles visually
- justified Korean text does not create oversized inter-word spacing
- no broken line starts/ends in EPUB reader
- TXT reader behavior still works

## Files That Need Extra Care

These files are high risk during future ports. Never patch only one side of the interface.

- `lib/Epub/Epub/ParsedText.cpp`
- `lib/Epub/Epub/ParsedText.h`
- `lib/Epub/Epub/Section.cpp`
- `lib/Epub/Epub/Section.h`
- `lib/Epub/Epub/blocks/TextBlock.cpp`
- `lib/Epub/Epub/blocks/TextBlock.h`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h`
- `lib/GfxRenderer/GfxRenderer.cpp`
- `lib/GfxRenderer/GfxRenderer.h`
- `src/CrossPointSettings.cpp`
- `src/CrossPointSettings.h`
- `src/main.cpp`

## Rules for Future Upstream Updates

When a new upstream stable release appears:

1. identify the new upstream release tag
2. branch from that tag
3. reapply Korean packs in the same order
4. build after each pack
5. only after all packs pass, update `release/korean`
6. create a new Korean tag

### Tag naming

Examples:

- first Korean release on upstream `1.2.0`: `1.2.0-ko.0`
- follow-up fixes: `1.2.0-ko.1`, `1.2.0-ko.2`

### Branch naming

Recommended:

- active port branch: `rebuild/1.2.0-ko`
- stable delivery branch: `release/korean`

## What Not To Do

- do not merge `upstream/master` directly into Korean work
- do not copy entire old Korean folders over a new upstream release
- do not patch compile errors one-by-one on a mixed branch after interfaces have diverged
- do not modify `ParsedText`, `Section`, `TextBlock`, and `GfxRenderer` all at once without intermediate builds

## Practical Working Method

Recommended commit style:

- one feature pack per commit or small sequence of commits
- commit messages should identify the Korean pack being ported
- if a build fails, isolate failure to the last feature pack only

Example sequence:

```bash
git checkout -b rebuild/1.2.0-ko e6c6e72
# port OTA / CI
git commit -m "port(ko): carry OTA and release workflow changes"
# port settings persistence
git commit -m "port(ko): persist Korean reader settings"
# port font selection
git commit -m "port(ko): restore SD font selection UI"
# port default font
git commit -m "port(ko): restore KoPub Batang default font"
# port indent + character wrap
git commit -m "port(ko): restore Korean paragraph indent and character wrapping"
```

## Quick Checklist

Before starting a new port:

- [ ] confirm upstream stable tag
- [ ] confirm last Korean stable reference tag
- [ ] branch from upstream tag, not from an old mixed branch
- [ ] define feature-pack order

Before shipping:

- [ ] `pio run` passes
- [ ] OTA URL points to Korean fork
- [ ] custom font selection persists
- [ ] KoPub Batang is wired correctly
- [ ] `paragraphIndent` works
- [ ] `characterWrap` works
- [ ] new `-ko.x` tag is prepared

## Reference Commits

- `67c8e3f` merge upstream `0.16.0`
- `49bdd58` merge upstream `1.0.0`
- `9610b31` merge upstream `1.1.1`
- `683e437` default font + indent option
- `62c3b3f` character-level wrapping
- `58fbab9` persist Korean-specific settings
- `fed95ce` OTA URL update
- `133a745` CI stabilization

## Final Maintenance Principle

The Korean fork should be maintained as a disciplined patch set on top of upstream stable releases.

That is the only approach that keeps future updates repeatable, debuggable, and low-risk.
