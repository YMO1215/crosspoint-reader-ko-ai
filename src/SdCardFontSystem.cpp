#include "SdCardFontSystem.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <iterator>

#include "CrossPointSettings.h"
#include "ReaderFontSizes.h"
#include "fontIds.h"

namespace {

// Point the reader font size at a size the given family actually ships, and
// persist the change so the settings UI and the loaded font never disagree.
// Guarded by the value-change check: a no-op snap must not write SPIFFS.
void snapFontPointSizeTo(const uint8_t availablePointSize) {
  if (availablePointSize == 0 || availablePointSize == SETTINGS.fontPointSize) return;
  LOG_DBG("SDFS", "Font size %u unavailable, snapping to %u", SETTINGS.fontPointSize, availablePointSize);
  SETTINGS.fontPointSize = availablePointSize;
  SETTINGS.saveToFile();
}

// Built-in UI fonts and their physical point sizes (at 150 DPI, matching the
// SD-font converter). Each is paired with a same-size SD fallback so CJK UI
// text matches the surrounding Latin. See SdCardFontSystem::setupUiFallbacks.
struct UiFontSize {
  int fontId;
  uint8_t pointSize;
};
constexpr UiFontSize kUiFontSizes[] = {
    {SMALL_FONT_ID, 8},
    {UI_10_FONT_ID, 10},
    {UI_12_FONT_ID, 12},
};

}  // namespace

void SdCardFontSystem::begin(GfxRenderer& renderer) {
  registry_.discover();

  // Register this system as the SD font ID resolver in settings.
  // Uses a static trampoline since CrossPointSettings stores a plain function pointer.
  SETTINGS.sdFontIdResolver = [](void* ctx, const char* familyName, uint8_t pointSize) -> int {
    return static_cast<SdCardFontSystem*>(ctx)->resolveFontId(familyName, pointSize);
  };
  SETTINGS.sdFontResolverCtx = this;

  // If user has a saved SD font selection, load it
  if (SETTINGS.sdFontFamilyName[0] != '\0') {
    const auto* family = registry_.findFamily(SETTINGS.sdFontFamilyName);
    if (family) {
      if (manager_.loadFamily(*family, renderer, SETTINGS.fontPointSize)) {
        snapFontPointSizeTo(manager_.currentPointSize());
        LOG_DBG("SDFS", "Loaded SD card font family: %s", SETTINGS.sdFontFamilyName);
      } else {
        LOG_ERR("SDFS", "Failed to load SD font family: %s (clearing)", SETTINGS.sdFontFamilyName);
        SETTINGS.clearSdFontFamily();
      }
    } else {
      LOG_DBG("SDFS", "SD font family not found on card: %s (clearing)", SETTINGS.sdFontFamilyName);
      SETTINGS.clearSdFontFamily();
    }
  }

  // Unconditional: with a separate menu font the UI needs hangul even when the
  // book font is a built-in (or failed to load). Runs after loadFamily so the
  // follow-the-book-font default sees the family that was actually loaded.
  setupUiFallbacks(renderer);

  LOG_DBG("SDFS", "SD font system ready (%d families discovered)", registry_.getFamilyCount());
}

void SdCardFontSystem::ensureLoaded(GfxRenderer& renderer) {
  // If the web server (or another task) installed/deleted fonts, re-discover.
  // Track whether we just re-discovered so we can force a reload below even
  // when the wanted family/size still maps to the same point size — the file
  // contents on disk may have changed (e.g. user re-uploaded a new build).
  const bool registryWasDirty = registryDirty_.exchange(false, std::memory_order_acquire);
  if (registryWasDirty) {
    LOG_DBG("SDFS", "Registry dirty — re-discovering fonts");
    registry_.discover();
  }

  const char* wantedFamily = SETTINGS.sdFontFamilyName;
  const std::string& currentFamily = manager_.currentFamilyName();

  // The menu font can change while the book font stays put. Its UI sizes are
  // already resident, so re-running setupUiFallbacks alone would stack a second
  // family on top of the old one; take the full unload+reload path instead.
  const bool uiFamilyChanged = (appliedUiFamily_ != SETTINGS.sdUiFontFamilyName);
  if (uiFamilyChanged) {
    LOG_DBG("SDFS", "Menu font changed: '%s' -> '%s'", appliedUiFamily_.c_str(), SETTINGS.sdUiFontFamilyName);
  }

  if (wantedFamily[0] == '\0') {
    if (!currentFamily.empty() || uiFamilyChanged) {
      manager_.unloadAll(renderer);
      // A built-in book font still leaves the menus needing hangul, so the menu
      // family is reloaded on its own — unloadAll just dropped it along with
      // the reader font.
      setupUiFallbacks(renderer);
    }
    // Back on a built-in family, which exists only at BUILTIN_READER_POINT_SIZES:
    // a size inherited from an SD family has to come back into that set.
    snapFontPointSizeTo(snapToNearestPointSize(BUILTIN_READER_POINT_SIZES, std::size(BUILTIN_READER_POINT_SIZES),
                                               SETTINGS.fontPointSize));
    return;
  }

  // Reload if family changed OR if the user-selected size maps to a
  // different file than what's currently loaded OR if the registry was
  // just rediscovered (file may have been replaced on disk).
  bool familyMatches = (currentFamily == wantedFamily);
  if (familyMatches) {
    const auto* family = registry_.findFamily(wantedFamily);
    if (!family) {
      LOG_DBG("SDFS", "SD font family disappeared: %s (clearing)", wantedFamily);
      manager_.unloadAll(renderer);
      SETTINGS.clearSdFontFamily();
      return;
    }
    const auto* selected = family->findNearestSize(SETTINGS.fontPointSize);
    const uint8_t wantedPt = selected ? selected->pointSize : 0;
    // Snap before the early return: the wanted size can already be loaded while
    // the setting still names a size this family does not ship.
    snapFontPointSizeTo(wantedPt);
    if (!registryWasDirty && !uiFamilyChanged && wantedPt == manager_.currentPointSize()) return;
    LOG_DBG("SDFS", "Reloading %s: size %u -> %u%s", wantedFamily, manager_.currentPointSize(), wantedPt,
            registryWasDirty ? " [registry dirty]" : "");
  }

  if (!currentFamily.empty()) {
    manager_.unloadAll(renderer);
  }

  const auto* family = registry_.findFamily(wantedFamily);
  if (family) {
    if (manager_.loadFamily(*family, renderer, SETTINGS.fontPointSize)) {
      snapFontPointSizeTo(manager_.currentPointSize());
      setupUiFallbacks(renderer);
      LOG_DBG("SDFS", "Loaded SD font family: %s", wantedFamily);
    } else {
      LOG_ERR("SDFS", "Failed to load SD font family: %s (clearing)", wantedFamily);
      SETTINGS.clearSdFontFamily();
    }
  } else {
    LOG_DBG("SDFS", "SD font family not found: %s (clearing)", wantedFamily);
    SETTINGS.clearSdFontFamily();
  }
}

void SdCardFontSystem::setupUiFallbacks(GfxRenderer& renderer) {
  // The menus may be set in a different family from the book: on a Korean UI
  // the built-in fonts draw no hangul at all, so this family *is* the menu
  // typeface, and a gothic reads better there than the serif one might want
  // for the book. Empty means follow the book font — upstream's behaviour.
  const std::string& readerFamilyName = manager_.currentFamilyName();
  const std::string familyName =
      SETTINGS.sdUiFontFamilyName[0] != '\0' ? SETTINGS.sdUiFontFamilyName : readerFamilyName;
  // Record what we acted on even when we bail: ensureLoaded compares against
  // this to spot a menu-font change, and a family that failed to load must not
  // re-trigger a full reload on every frame.
  appliedUiFamily_ = SETTINGS.sdUiFontFamilyName;
  if (familyName.empty()) return;  // no SD family loaded — nothing to fall back to

  const auto* family = registry_.findFamily(familyName);
  if (!family) {
    LOG_DBG("SDFS", "UI font family not on card: %s", familyName.c_str());
    return;
  }

  // One representative codepoint per script: Han, Hiragana, Katakana, Hangul.
  static constexpr uint32_t kCjkProbes[] = {0x4E00, 0x3042, 0x30A2, 0xAC00};

  // Probe the first UI size that loads before paying for the rest:
  // resolveTextFontId only redirects on CJK codepoints, so a Latin-only family
  // can never act as a fallback and its UI sizes would be dead weight in RAM.
  bool probed = false;
  for (const auto& ui : kUiFontSizes) {
    const int sdFontId = manager_.loadFamilyExtraSize(*family, renderer, ui.pointSize);
    if (sdFontId == 0) {
      LOG_DBG("SDFS", "No %u pt SD glyphs for UI fallback in %s", ui.pointSize, familyName.c_str());
      continue;
    }
    if (!probed) {
      const auto it = renderer.getFontMap().find(sdFontId);
      if (it == renderer.getFontMap().end()) continue;
      bool hasCjk = false;
      for (const uint32_t cp : kCjkProbes) {
        if (it->second.hasCodepoint(cp)) {
          hasCjk = true;
          break;
        }
      }
      if (!hasCjk) {
        LOG_DBG("SDFS", "%s has no CJK coverage - skipping UI fallback sizes", familyName.c_str());
        return;
      }
      probed = true;
    }
    renderer.setFallbackFont(ui.fontId, sdFontId);
  }
}

int SdCardFontSystem::resolveFontId(const char* familyName, uint8_t /*pointSize*/) const {
  // The manager holds exactly one reader-size font, already selected for
  // SETTINGS.fontPointSize, so the size argument is implicit — always return
  // that font's ID. ensureLoaded() must have run for the current settings first.
  return manager_.getFontId(familyName);
}
