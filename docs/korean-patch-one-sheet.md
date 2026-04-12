# Korean Patch One-Sheet

## Purpose

This is the shortest reliable guide for rebuilding the Korean fork on top of a new upstream release.

Target reader:
- a new maintainer
- a weaker AI model with limited context retention
- anyone who needs the proven Korean patch points without re-learning the whole debugging history

Reference success baseline:
- Upstream base: `1.2.0`
- Working Korean result: `v1.2.0-ko-fix9`
- Working branch pattern: `rebuild/<upstream-version>-ko`

Do not treat this as a theory document.
Treat it as an execution checklist.

## What This Korean Build Actually Changes

Compared with upstream release firmware, the Korean build intentionally changes these areas:

1. Korean UI language support
- Add and enable Korean translation strings.
- Make Korean the default UI language.

2. Korean font stack
- UI uses Pretendard.
- Reader uses KoPub Batang.
- Built-in fonts are reduced to keep binary size under control.

3. Korean EPUB text layout
- Add Korean-friendly character-wrap behavior for justified text.
- Adjust paragraph indent fallback, line spacing, and final inter-word gap behavior.

4. Cache invalidation for reader layout changes
- EPUB and TXT caches must rebuild when Korean layout behavior changes.

5. Korean OTA/update path
- OTA points to the Korean repo releases, not upstream releases.

6. Build/flash capacity adjustments
- Partition sizes must accommodate the larger Korean firmware.

## Proven Files To Inspect On Every Rebase

These are the main patch points. If a future rebase breaks Korean behavior, inspect these first.

### Translation / defaults
- `lib/I18n/translations/korean.yaml`
- `lib/I18n/I18n.h`

### Font setup
- `lib/EpdFont/builtinFonts/all.h`
- `lib/EpdFont/builtinFonts/kopub_14_regular.h`
- `lib/EpdFont/builtinFonts/pretendard_10_regular.h`
- `src/fontIds.h`
- `src/main.cpp`
- `src/CrossPointSettings.h`
- `src/CrossPointSettings.cpp`

### EPUB layout
- `lib/Epub/Epub/ParsedText.h`
- `lib/Epub/Epub/ParsedText.cpp`
- `lib/Epub/Epub/Section.h`
- `lib/Epub/Epub/Section.cpp`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.h`
- `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp`
- `src/activities/reader/EpubReaderActivity.cpp`

### TXT reader cache/layout
- `src/activities/reader/TxtReaderActivity.h`
- `src/activities/reader/TxtReaderActivity.cpp`

### Settings persistence / UI toggles
- `src/JsonSettingsIO.cpp`
- `src/SettingsList.h`
- `src/CrossPointSettings.h`
- `src/CrossPointSettings.cpp`

### Build / OTA
- `partitions.csv`
- `platformio.ini`
- `src/network/OtaUpdater.cpp`

## Non-Negotiable Rules

1. Start from an upstream release tag, not upstream `master`.
- Proven base for the current work was tag `1.2.0`.

2. Do not mix old Korean implementation files with newer upstream headers.
- This caused signature mismatches and fake progress during earlier attempts.

3. Build the Korean fork as a small patch set on top of upstream.
- Do not grow a permanent fork full of unrelated divergence.

4. When Korean text layout changes, invalidate caches.
- Otherwise the device may look unchanged even when the code is different.

5. Do not assume a fix worked just because the code changed.
- Confirm the runtime code path actually uses that logic.

## The Actual Failure History Behind fix9

These are the important mistakes that cost time.
Future work should check these immediately.

### 1. Korean text rendered incorrectly because the font pipeline was not actually aligned
Symptoms:
- Korean UI text missing or broken
- body text looked like black bars or unreadable glyphs

What fixed it:
- add `korean.yaml`
- set default language to Korean
- add Pretendard and KoPub built-in fonts
- register those fonts in `main.cpp`
- correct Pretendard glyph `advanceX` scaling so the renderer reads widths correctly

Important note:
- Korean font problems are not just "missing glyph" problems.
- Wrong advance metrics also break spacing and layout.

### 2. Firmware size became a real issue after adding Korean fonts
Symptoms:
- build too large
- OTA/app partition pressure
- unstable release artifact expectations

What fixed it:
- remove unused built-in fonts from `lib/EpdFont/builtinFonts/all.h`
- enlarge app partitions in `partitions.csv`

Important note:
- Korean fonts are large enough that upstream partition assumptions may no longer be safe.

### 3. We fixed spacing code but the screen still looked unchanged
Symptoms:
- user reflashed but EPUB page still looked the same
- looked like the code fix had no effect

Actual cause:
- cached page/section data was being reused

What fixed it:
- bump `SECTION_FILE_VERSION` in `lib/Epub/Epub/Section.cpp`
- ensure reader cache keys include the Korean layout-affecting settings

Important note:
- if a user says "it still looks identical", suspect cache before assuming the patch failed.

### 4. We changed `extractLine()` but still did not fix the final Korean spacing bug
Symptoms:
- some Korean words still had huge spaces around them
- changes looked logically correct in code but runtime output still looked wrong

Actual cause:
- `layoutCharacterWrap()` existed, but the live EPUB rendering path was not using it
- the code still fell through the generic hyphenation line breaker

What fixed it:
- in `ParsedText::layoutAndExtractLines()`, if `characterWrap` is on and alignment is `Justify`, route directly to `layoutCharacterWrap()` and return early
- do not let `characterWrap` silently share the generic `computeHyphenatedLineBreaks()` path

Important note:
- this was the final fix in `v1.2.0-ko-fix9`
- if Korean justified spacing looks wrong again on a future rebase, inspect this path first

### 5. Some spacing fixes were only partial because the Korean reference logic had two separate layers
Symptoms:
- fixing width measurement alone helped only a little
- fixing gap distribution alone helped only a little

What fixed it:
- restore the Korean character-wrap algorithm
- switch key width measurements to match the reference behavior
- simplify justified gap calculation in `extractLine()` so it uses one resolved final gap width instead of natural space plus extra justify width

Important note:
- Korean layout behavior is a combination of line-breaking logic and final placement logic
- checking only one layer is not enough

## fix9 Minimum Patch Set

If you are porting to a new upstream release, these are the minimum Korean changes that must survive.

### A. Korean translation and defaults
- add `lib/I18n/translations/korean.yaml`
- set Korean as default in `lib/I18n/I18n.h`

### B. Korean font registration
- keep Pretendard UI font available
- keep KoPub reader font available
- keep `KOPUB_14_FONT_ID`
- make reader font resolve to KoPub in `CrossPointSettings::getReaderFontId()`
- register fonts in `src/main.cpp`

### C. Korean settings must exist and persist
These settings must exist in memory, JSON, settings UI, and reader cache comparisons:
- `paragraphIndent`
- `characterWrap`
- `customFontPath` if still supported

Files to verify:
- `src/CrossPointSettings.h`
- `src/CrossPointSettings.cpp`
- `src/JsonSettingsIO.cpp`
- `src/SettingsList.h`

### D. EPUB parser and cache signatures must stay in sync
If upstream changes constructor signatures, re-check all of these together:
- `ParsedText.h`
- `ParsedText.cpp`
- `Section.h`
- `Section.cpp`
- `ChapterHtmlSlimParser.h`
- `ChapterHtmlSlimParser.cpp`
- `EpubReaderActivity.cpp`

If one side still uses the old arguments, the build may fail or, worse, compile but behave incorrectly.

### E. Korean EPUB layout behavior must remain wired correctly
In `ParsedText.cpp`, verify all of the following:

1. `splitUtf8Chars()` exists.
2. `layoutCharacterWrap()` exists.
3. `layoutAndExtractLines()` routes `characterWrap + Justify` directly into `layoutCharacterWrap()`.
4. `extractLine()` does not over-inflate Korean gaps.
5. paragraph indent fallback uses ideographic space (`U+3000`) for Korean compatibility.
6. width measurement changes that were required for Korean output are still present.

### F. Cache invalidation must survive
- EPUB cache header/version changes must include Korean layout-affecting state.
- TXT page cache must include `characterWrap`.
- `SECTION_FILE_VERSION` should be bumped whenever incompatible EPUB layout changes are introduced.

### G. OTA must point to the Korean release source
- `src/network/OtaUpdater.cpp` must query the Korean repo releases if you want OTA updates for the Korean branch.

### H. Partitions must still fit the Korean build
- confirm `partitions.csv` still gives enough OTA app space for the larger Korean firmware

## Runtime Validation Checklist

Do not stop at compile success.
Check these manually after every major rebase.

### UI / basic boot
- device boots normally
- settings screen appears in Korean by default
- no missing Korean glyphs in menus

### Reader / EPUB
- Korean EPUB opens without broken glyphs
- justified Korean paragraphs do not show extreme word gaps
- paragraph indent behaves as expected
- changing line spacing visibly changes output
- reopening the same book after a layout code change reflects the new layout, not the old cached one

### Reader / TXT
- TXT wraps correctly with and without `characterWrap`
- cache rebuild happens when wrap mode changes

### OTA / versioning
- version screen shows the expected Korean build tag/version
- OTA checks the Korean repo if OTA is expected to work for Korean users

### Flashing
- manual flash for current device family uses the correct chip and offsets
- for the current verified hardware, working command was:

```cmd
python -m esptool --chip esp32c3 --port COM5 --baud 460800 write-flash -z 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

## Fast Triage Guide

If the next maintainer sees this symptom, check here first.

### Symptom: Korean UI is missing or broken
Check:
- `korean.yaml`
- `I18n.h`
- `main.cpp`
- built-in font registration

### Symptom: Korean body text shows unreadable glyphs or black blocks
Check:
- KoPub and Pretendard font headers exist
- `main.cpp` font registration
- font metrics / advance scaling assumptions

### Symptom: spacing is still bad even after changing layout code
Check in this order:
1. cache invalidation
2. `layoutAndExtractLines()` actually routing to `layoutCharacterWrap()`
3. `extractLine()` final gap logic
4. width measurement method used by Korean layout logic

### Symptom: code looks correct but output is unchanged
Check:
- EPUB/TXT cache mismatch handling
- section cache version
- whether the changed function is on the live runtime path

### Symptom: build suddenly becomes too large
Check:
- built-in fonts list
- partition sizes
- release build flags

### Symptom: OTA finds the wrong release line
Check:
- `src/network/OtaUpdater.cpp`

## Recommended Rebase Procedure

1. Create a fresh branch from the new upstream release tag.
2. Reapply the Korean patches in this order:
- translation/default language
- fonts/font registration
- settings persistence
- EPUB parser/cache signature alignment
- Korean EPUB layout logic
- cache invalidation
- OTA repo target
- partition/build adjustments
3. Build after each group.
4. Flash and test a Korean EPUB page known to expose spacing issues.
5. Do not trust screenshots until cache behavior is ruled out.
6. Tag only after runtime validation, not after compile success.

## Known Good Commit Story for 1.2.0-ko

This was the working progression for the current baseline:

- `9f0e45d` port korean 1.2.0 changes
- `c7a2228` reduce builtin fonts for korean build
- `9a7b5ac` fix: restore korean font rendering and add korean ui translation
- `c9da294` chore: remove unused ui font includes for ko build
- `cd7e0e3` build: enlarge ota partitions for ko release
- `e6b33a2` fix: correct pretendard glyph advance metrics
- `058dbb8` fix: restore korean character wrap layout
- `18dcd1f` fix: align korean reader layout with reference rendering
- `49abb28` fix: reduce excessive korean justification gaps
- `6f12d56` fix: rebuild epub cache for korean layout changes
- `8174639` fix: route korean justified text through character-wrap layout

The last commit above is the one that finally fixed the "spacing still looks the same" issue by restoring the actual runtime code path.

## Final Reminder

The main danger is not one broken line of code.
The main danger is fixing the wrong layer:
- font data instead of font registration
- layout math instead of runtime routing
- code instead of cache invalidation
- build success instead of actual device output

If future Korean spacing looks wrong again, start with these three files first:
- `lib/Epub/Epub/ParsedText.cpp`
- `lib/Epub/Epub/Section.cpp`
- `src/activities/reader/EpubReaderActivity.cpp`
