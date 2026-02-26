/**
 * @file simplego_fonts.h
 * @brief German umlaut support via fallback fonts
 *
 * ESP32 stores const LVGL fonts in flash (read-only). We cannot modify
 * their fallback pointer. Instead, simplego_font_14/10 are RAM copies
 * of Montserrat with the umlaut fallback already set.
 *
 * Usage in UI code: use &simplego_font_14 instead of &lv_font_montserrat_14
 *
 * Glyphs: Ä(U+00C4) Ö(U+00D6) Ü(U+00DC) ä(U+00E4) ö(U+00F6) ü(U+00FC) ß(U+00DF)
 */
#ifndef SIMPLEGO_FONTS_H
#define SIMPLEGO_FONTS_H

#include "lvgl.h"

// Fallback fonts containing only German umlauts (7 glyphs each, ~500 bytes)
extern const lv_font_t simplego_umlauts_14;
extern const lv_font_t simplego_umlauts_12;
extern const lv_font_t simplego_umlauts_10;

// RAM copies of Montserrat with umlaut fallback — use these in UI code
// Initialized in app_main() via memcpy + fallback assignment
extern lv_font_t simplego_font_14;
extern lv_font_t simplego_font_12;
extern lv_font_t simplego_font_10;

#endif /* SIMPLEGO_FONTS_H */
