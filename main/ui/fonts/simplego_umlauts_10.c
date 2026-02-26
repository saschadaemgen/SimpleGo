/*******************************************************************************
 * Size: 10 px
 * Bpp: 4
 * Opts: --no-compress --no-prefilter --bpp 4 --size 10 --font node_modules/@fontsource/montserrat/files/montserrat-latin-500-normal.woff -r 0xC4,0xD6,0xDC,0xE4,0xF6,0xFC,0xDF --format lvgl -o output/fonts/simplego_umlauts_10.c --force-fast-kern-format
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef SIMPLEGO_UMLAUTS_10
#define SIMPLEGO_UMLAUTS_10 1
#endif

#if SIMPLEGO_UMLAUTS_10

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+00C4 "Ä" */
    0x0, 0x9, 0x1a, 0x0, 0x0, 0x0, 0x1f, 0x60,
    0x0, 0x0, 0x8, 0x8c, 0x0, 0x0, 0x0, 0xd0,
    0x94, 0x0, 0x0, 0x67, 0x2, 0xb0, 0x0, 0xd,
    0xcc, 0xce, 0x30, 0x5, 0x90, 0x0, 0x4a, 0x0,
    0xc3, 0x0, 0x0, 0xd1,

    /* U+00D6 "Ö" */
    0x0, 0x19, 0x37, 0x0, 0x1, 0x9d, 0xdc, 0x30,
    0xc, 0x70, 0x3, 0xd3, 0x5b, 0x0, 0x0, 0x5c,
    0x78, 0x0, 0x0, 0x2e, 0x5b, 0x0, 0x0, 0x5c,
    0xc, 0x70, 0x3, 0xd3, 0x1, 0x9e, 0xdc, 0x30,

    /* U+00DC "Ü" */
    0x0, 0x56, 0x74, 0x0, 0xf0, 0x0, 0x1e, 0xf,
    0x0, 0x1, 0xe0, 0xf0, 0x0, 0x1e, 0xf, 0x0,
    0x1, 0xe0, 0xe0, 0x0, 0x2d, 0xa, 0x70, 0x9,
    0x80, 0x1a, 0xdd, 0x90,

    /* U+00DF "ß" */
    0x4, 0xcc, 0xb2, 0x0, 0xe2, 0x5, 0xa0, 0x1e,
    0x0, 0x78, 0x1, 0xe0, 0xaf, 0x50, 0x1e, 0x0,
    0xd, 0x31, 0xe0, 0x0, 0xc3, 0x1e, 0x2c, 0xc7,
    0x0,

    /* U+00E4 "ä" */
    0x5, 0x57, 0x30, 0x0, 0x0, 0x0, 0x1b, 0xcd,
    0x60, 0x1, 0x0, 0xe0, 0x1a, 0xaa, 0xf1, 0x68,
    0x1, 0xe1, 0x2c, 0xba, 0xd1,

    /* U+00F6 "ö" */
    0x2, 0x84, 0x60, 0x0, 0x0, 0x0, 0x7, 0xdd,
    0xb1, 0x5c, 0x0, 0x7b, 0x87, 0x0, 0x1e, 0x5c,
    0x0, 0x7b, 0x6, 0xdd, 0xb1,

    /* U+00FC "ü" */
    0x0, 0xa1, 0x90, 0x0, 0x0, 0x0, 0x2d, 0x0,
    0x1d, 0x2d, 0x0, 0x1d, 0x2d, 0x0, 0x2d, 0xe,
    0x10, 0x7d, 0x5, 0xdd, 0x9d
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 117, .box_w = 9, .box_h = 8, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 36, .adv_w = 134, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 127, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 108, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 96, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 102, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 163, .adv_w = 108, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x12, 0x18, 0x1b, 0x20, 0x32, 0x38
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 196, .range_length = 57, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 7, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t simplego_umlauts_10 = {
#else
lv_font_t simplego_umlauts_10 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 8,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SIMPLEGO_UMLAUTS_10*/

