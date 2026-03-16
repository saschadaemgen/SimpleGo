/**
 * @file ui_screensaver.c
 * @brief Matrix Rain Screensaver - SEC-04 Display Lock (Matrix Mode)
 *
 * Animated Matrix-style digital rain effect on LVGL Canvas.
 * Uses an embedded 8x8 bitmap font for self-contained rendering
 * with no dependency on LVGL font configuration.
 *
 * Three neon color palettes (green 60%, cyan 25%, yellow 15%)
 * are randomly distributed across 40 columns. Each column has
 * independent speed, trail length, and timing.
 *
 * Canvas buffer lives in PSRAM (~153 KB for 320x240 RGB565).
 * Animation runs at 20 FPS via LVGL timer on Core 1.
 *
 * Reference: 0015/Arduino_DigitalRain_Matrix "Matrix Prism" mode
 * Ported to pure LVGL v9 Canvas with direct pixel rendering.
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha Daemgen, IT and More Systems
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_screensaver.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"           /* ui_chat_get_keyboard_indev() */
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "UI_MATRIX";

/* ================================================================
 * Constants
 * ================================================================ */

#define SCREEN_W        320
#define SCREEN_H        240
#define CELL_W          8       /* pixels per character column */
#define CELL_H          8       /* pixels per character row */
#define NUM_COLS        (SCREEN_W / CELL_W)   /* 40 */
#define NUM_ROWS        (SCREEN_H / CELL_H)   /* 30 */

#define MATRIX_FPS      20
#define MATRIX_PERIOD   (1000 / MATRIX_FPS)   /* 50 ms */

#define TAIL_MIN        6       /* minimum trail length in cells */
#define TAIL_MAX        22      /* maximum trail length in cells */
#define SPEED_MIN       1       /* fastest: advance every frame */
#define SPEED_MAX       3       /* slowest: advance every 3rd frame */
#define WAIT_MIN        3       /* minimum restart delay in frames */
#define WAIT_MAX        35      /* maximum restart delay in frames */
#define SHIMMER_PCT     12      /* percent chance per column per frame */

/* Canvas buffer size: 320 * 240 * 2 bytes (RGB565) = 153,600 */
#define CANVAS_BUF_SIZE (SCREEN_W * SCREEN_H * 2)

/* ================================================================
 * Character set for the rain effect
 * ================================================================ */

static const char RAIN_CHARS[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-*/<>=@#$%&";
#define NUM_RAIN_CHARS  ((int)(sizeof(RAIN_CHARS) - 1))   /* 48 */

/* ================================================================
 * Embedded 8x8 Bitmap Font (public domain CP437/font8x8_basic)
 *
 * Format: 8 bytes per character, one byte per row.
 * Bit 0 (LSB) = leftmost pixel, Bit 7 (MSB) = rightmost pixel.
 * Indexed to match RAIN_CHARS[] order.
 * ================================================================ */

static const uint8_t FONT_DATA[NUM_RAIN_CHARS][8] = {
    /* '0' */ {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00},
    /* '1' */ {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00},
    /* '2' */ {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00},
    /* '3' */ {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00},
    /* '4' */ {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00},
    /* '5' */ {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00},
    /* '6' */ {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00},
    /* '7' */ {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00},
    /* '8' */ {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00},
    /* '9' */ {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00},
    /* 'A' */ {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00},
    /* 'B' */ {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00},
    /* 'C' */ {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00},
    /* 'D' */ {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00},
    /* 'E' */ {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00},
    /* 'F' */ {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00},
    /* 'G' */ {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00},
    /* 'H' */ {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00},
    /* 'I' */ {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    /* 'J' */ {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00},
    /* 'K' */ {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00},
    /* 'L' */ {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00},
    /* 'M' */ {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00},
    /* 'N' */ {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00},
    /* 'O' */ {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00},
    /* 'P' */ {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00},
    /* 'Q' */ {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00},
    /* 'R' */ {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00},
    /* 'S' */ {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00},
    /* 'T' */ {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00},
    /* 'U' */ {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00},
    /* 'V' */ {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00},
    /* 'W' */ {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    /* 'X' */ {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00},
    /* 'Y' */ {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00},
    /* 'Z' */ {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00},
    /* '+' */ {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00},
    /* '-' */ {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00},
    /* '*' */ {0x00,0x0C,0x2D,0x1E,0x2D,0x0C,0x00,0x00},
    /* '/' */ {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00},
    /* '<' */ {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00},
    /* '>' */ {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},
    /* '=' */ {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00},
    /* '@' */ {0x3C,0x42,0x5A,0x5A,0x5C,0x42,0x3C,0x00},
    /* '#' */ {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00},
    /* '$' */ {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00},
    /* '%' */ {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00},
    /* '&' */ {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00},
};

/* ================================================================
 * Color Palettes - Neon Multi (green / cyan / yellow)
 *
 * Five brightness levels per palette:
 *   [0] = head (near white), [1] = bright, [2] = mid,
 *   [3] = dim, [4] = very dim (near black)
 *
 * Palette distribution: green ~60%, cyan ~25%, yellow ~15%
 * ================================================================ */

#define NUM_PALETTES     3
#define NUM_SHADES       5

/* Pre-computed lv_color_t values, initialized in screensaver_init_colors() */
static lv_color_t s_palettes[NUM_PALETTES][NUM_SHADES];

static void screensaver_init_colors(void)
{
    /* Green palette (dominant) */
    s_palettes[0][0] = lv_color_make(220, 255, 220);   /* head glow */
    s_palettes[0][1] = lv_color_make(0,   255, 65);    /* bright neon */
    s_palettes[0][2] = lv_color_make(0,   180, 45);    /* medium */
    s_palettes[0][3] = lv_color_make(0,   100, 25);    /* dim */
    s_palettes[0][4] = lv_color_make(0,   40,  10);    /* very dim */

    /* Cyan palette */
    s_palettes[1][0] = lv_color_make(200, 255, 255);   /* head glow */
    s_palettes[1][1] = lv_color_make(0,   200, 255);   /* bright neon */
    s_palettes[1][2] = lv_color_make(0,   140, 180);   /* medium */
    s_palettes[1][3] = lv_color_make(0,   70,  100);   /* dim */
    s_palettes[1][4] = lv_color_make(0,   25,  40);    /* very dim */

    /* Yellow/Amber palette */
    s_palettes[2][0] = lv_color_make(255, 255, 200);   /* head glow */
    s_palettes[2][1] = lv_color_make(255, 220, 0);     /* bright neon */
    s_palettes[2][2] = lv_color_make(180, 155, 0);     /* medium */
    s_palettes[2][3] = lv_color_make(100, 85,  0);     /* dim */
    s_palettes[2][4] = lv_color_make(40,  35,  0);     /* very dim */
}

/* ================================================================
 * Drop state - one per column
 * ================================================================ */

typedef struct {
    int16_t  head_y;          /* current row of drop head (can be negative) */
    uint8_t  tail_len;        /* trail length in cells */
    uint8_t  speed;           /* 1=fast, 2=medium, 3=slow */
    uint8_t  tick;            /* frame counter for speed control */
    uint8_t  palette_idx;     /* 0=green, 1=cyan, 2=yellow */
    int16_t  wait;            /* frames to wait before (re)starting */
    uint8_t  chars[NUM_ROWS]; /* character index per row position */
} matrix_drop_t;

/* ================================================================
 * Static state
 * ================================================================ */

static lv_obj_t   *s_screen       = NULL;
static lv_obj_t   *s_canvas       = NULL;
static uint8_t    *s_canvas_buf   = NULL;
static lv_obj_t   *s_hidden_ta    = NULL;
static lv_group_t *s_group        = NULL;
static lv_timer_t *s_anim_timer   = NULL;

static matrix_drop_t s_drops[NUM_COLS];

/* ================================================================
 * Random helpers using hardware RNG
 * ================================================================ */

/* Random integer in [min, max] inclusive */
static int rand_range(int min, int max)
{
    if (min >= max) return min;
    return min + (int)(esp_random() % (uint32_t)(max - min + 1));
}

/* Weighted random palette: green=60%, cyan=25%, yellow=15% */
static uint8_t rand_palette(void)
{
    int r = rand_range(0, 99);
    if (r < 60) return 0;      /* green */
    if (r < 85) return 1;      /* cyan */
    return 2;                   /* yellow */
}

/* Random character index into RAIN_CHARS / FONT_DATA */
static uint8_t rand_char(void)
{
    return (uint8_t)(esp_random() % NUM_RAIN_CHARS);
}

/* ================================================================
 * Drop initialization
 * ================================================================ */

static void init_drop(int col, bool first_time)
{
    matrix_drop_t *d = &s_drops[col];

    d->tail_len    = (uint8_t)rand_range(TAIL_MIN, TAIL_MAX);
    d->speed       = (uint8_t)rand_range(SPEED_MIN, SPEED_MAX);
    d->tick        = 0;
    d->palette_idx = rand_palette();

    if (first_time) {
        /* Stagger initial positions for visual variety */
        d->head_y = (int16_t)rand_range(-NUM_ROWS, NUM_ROWS);
        d->wait   = 0;
    } else {
        /* Restart from top after finishing */
        d->head_y = (int16_t)rand_range(-10, -1);
        d->wait   = (int16_t)rand_range(WAIT_MIN, WAIT_MAX);
    }

    /* Fill character slots with random characters */
    for (int r = 0; r < NUM_ROWS; r++) {
        d->chars[r] = rand_char();
    }
}

/* ================================================================
 * Pixel-level drawing helpers
 * ================================================================ */

/**
 * Draw a single 8x8 character at grid position (col, row) with color.
 * Only lit pixels are drawn; unlit pixels are left as-is (assumed black
 * from prior clear_cell call).
 */
static void draw_char_at(int col, int row, uint8_t char_idx, lv_color_t color)
{
    if (row < 0 || row >= NUM_ROWS || col < 0 || col >= NUM_COLS) return;
    if (char_idx >= NUM_RAIN_CHARS) return;

    const uint8_t *glyph = FONT_DATA[char_idx];
    int x0 = col * CELL_W;
    int y0 = row * CELL_H;

    for (int gy = 0; gy < 8; gy++) {
        int py = y0 + gy;
        if (py >= SCREEN_H) break;

        uint8_t bits = glyph[gy];
        for (int gx = 0; gx < 8; gx++) {
            if (bits & (1 << gx)) {     /* font8x8: LSB = leftmost */
                int px = x0 + gx;
                if (px < SCREEN_W) {
                    lv_canvas_set_px(s_canvas, px, py, color, LV_OPA_COVER);
                }
            }
        }
    }
}

/**
 * Clear a single 8x8 cell to black by writing zeros directly
 * to the canvas buffer. Black = 0x0000 in any RGB565 variant.
 */
static void clear_cell(int col, int row)
{
    if (row < 0 || row >= NUM_ROWS || col < 0 || col >= NUM_COLS) return;

    int x0 = col * CELL_W;
    int y0 = row * CELL_H;
    uint16_t *buf = (uint16_t *)s_canvas_buf;

    for (int gy = 0; gy < CELL_H; gy++) {
        int py = y0 + gy;
        if (py >= SCREEN_H) break;
        /* Zero 8 pixels (16 bytes) in one memset call */
        memset(&buf[py * SCREEN_W + x0], 0, CELL_W * sizeof(uint16_t));
    }
}

/**
 * Get the color for a trail character based on distance from head.
 * distance=0 is the head, distance=tail_len is the oldest character.
 */
static lv_color_t get_trail_color(int distance, int tail_len, uint8_t palette_idx)
{
    if (palette_idx >= NUM_PALETTES) palette_idx = 0;

    if (distance <= 0) {
        return s_palettes[palette_idx][0];   /* head: bright glow */
    }

    /* Map distance to shade index 1-4 based on position in trail */
    int quarter = tail_len / 4;
    if (quarter < 1) quarter = 1;

    if (distance <= quarter) {
        return s_palettes[palette_idx][1];   /* bright */
    } else if (distance <= quarter * 2) {
        return s_palettes[palette_idx][2];   /* medium */
    } else if (distance <= quarter * 3) {
        return s_palettes[palette_idx][3];   /* dim */
    } else {
        return s_palettes[palette_idx][4];   /* very dim */
    }
}

/* ================================================================
 * Animation timer callback - runs at 20 FPS on LVGL task (Core 1)
 * ================================================================ */

static void matrix_animation_cb(lv_timer_t *timer)
{
    (void)timer;

    for (int col = 0; col < NUM_COLS; col++) {
        matrix_drop_t *d = &s_drops[col];

        /* ---- Waiting phase ---- */
        if (d->wait > 0) {
            d->wait--;
            continue;
        }

        /* ---- Speed gating ---- */
        d->tick++;
        if (d->tick < d->speed) {
            /* Still waiting for next advance, but do shimmer */
            goto shimmer;
        }
        d->tick = 0;

        /* ---- Clear the cell that just fell off the trail ---- */
        {
            int erase_row = d->head_y - d->tail_len - 1;
            if (erase_row >= 0 && erase_row < NUM_ROWS) {
                clear_cell(col, erase_row);
            }
        }

        /* ---- Advance head ---- */
        d->head_y++;

        /* ---- Check if drop has fully exited the screen ---- */
        if (d->head_y - d->tail_len > NUM_ROWS + 2) {
            init_drop(col, false);
            continue;
        }

        /* ---- Draw the visible trail ---- */
        for (int dist = 0; dist <= d->tail_len; dist++) {
            int row = d->head_y - dist;
            if (row < 0) continue;
            if (row >= NUM_ROWS) continue;

            /* Clear cell first (removes old character pixels) */
            clear_cell(col, row);

            /* Draw new character with trail color */
            lv_color_t color = get_trail_color(dist, d->tail_len, d->palette_idx);
            draw_char_at(col, row, d->chars[row], color);
        }

        /* ---- Erase cells beyond the very dim zone ---- */
        for (int extra = 1; extra <= 3; extra++) {
            int row = d->head_y - d->tail_len - extra;
            if (row >= 0 && row < NUM_ROWS) {
                clear_cell(col, row);
            }
        }

shimmer:
        /* ---- Shimmer: randomly change trail characters ---- */
        if (rand_range(0, 99) < SHIMMER_PCT) {
            /* Pick a random visible trail position */
            int dist = rand_range(1, d->tail_len);
            int row = d->head_y - dist;
            if (row >= 0 && row < NUM_ROWS) {
                d->chars[row] = rand_char();
                /* Redraw the changed character */
                clear_cell(col, row);
                lv_color_t color = get_trail_color(dist, d->tail_len,
                                                   d->palette_idx);
                draw_char_at(col, row, d->chars[row], color);
            }
        }
    }

    /* Trigger LVGL to redraw the canvas (single invalidate per frame) */
    lv_obj_invalidate(s_canvas);
}

/* ================================================================
 * Keyboard event handler - any key unlocks
 * ================================================================ */

static void on_any_key(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Key press detected, stopping screensaver");
    ui_manager_unlock();
}

/* ================================================================
 * Public API
 * ================================================================ */

lv_obj_t *ui_screensaver_create(void)
{
    ESP_LOGI(TAG, "Creating Matrix Rain screensaver");

    /* Initialize color palettes */
    screensaver_init_colors();

    /* ---- Create screen ---- */
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Allocate canvas buffer in PSRAM ---- */
    s_canvas_buf = (uint8_t *)heap_caps_malloc(CANVAS_BUF_SIZE,
                                                MALLOC_CAP_SPIRAM);
    if (!s_canvas_buf) {
        ESP_LOGE(TAG, "[FAIL] Cannot allocate canvas buffer (%d bytes)",
                 CANVAS_BUF_SIZE);
        return s_screen;
    }
    memset(s_canvas_buf, 0, CANVAS_BUF_SIZE);

    ESP_LOGI(TAG, "Canvas buffer allocated: %d bytes in PSRAM", CANVAS_BUF_SIZE);

    /* ---- Create LVGL canvas ---- */
    s_canvas = lv_canvas_create(s_screen);
    lv_canvas_set_buffer(s_canvas, s_canvas_buf,
                         SCREEN_W, SCREEN_H, LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(s_canvas, lv_color_black(), LV_OPA_COVER);
    lv_obj_center(s_canvas);

    /* ---- Initialize all 40 drops ---- */
    for (int col = 0; col < NUM_COLS; col++) {
        init_drop(col, true);
    }

    /* ---- Hidden textarea for keyboard capture ----
     * Same technique as ui_lock.c: invisible textarea captures
     * all keyboard events from the T-Deck physical keyboard. */
    s_hidden_ta = lv_textarea_create(s_screen);
    lv_obj_set_size(s_hidden_ta, 1, 1);
    lv_obj_set_pos(s_hidden_ta, -10, -10);
    lv_obj_set_style_opa(s_hidden_ta, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(s_hidden_ta, LV_OBJ_FLAG_SCROLLABLE);

    /* Character key -> VALUE_CHANGED, Enter -> READY, special -> KEY */
    lv_obj_add_event_cb(s_hidden_ta, on_any_key, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_hidden_ta, on_any_key, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_hidden_ta, on_any_key, LV_EVENT_KEY, NULL);

    /* Create input group and assign keyboard */
    s_group = lv_group_create();
    lv_group_add_obj(s_group, s_hidden_ta);

    lv_indev_t *kb = ui_chat_get_keyboard_indev();
    if (kb) {
        lv_indev_set_group(kb, s_group);
        ESP_LOGI(TAG, "Keyboard assigned to screensaver");
    }

    /* ---- Start animation timer ---- */
    s_anim_timer = lv_timer_create(matrix_animation_cb, MATRIX_PERIOD, NULL);

    ESP_LOGI(TAG, "Matrix Rain screensaver active (%d FPS, %d columns)",
             MATRIX_FPS, NUM_COLS);

    return s_screen;
}

void ui_screensaver_cleanup(void)
{
    /* Stop animation first */
    if (s_anim_timer) {
        lv_timer_delete(s_anim_timer);
        s_anim_timer = NULL;
    }

    /* Delete keyboard group */
    if (s_group) {
        lv_group_del(s_group);
        s_group = NULL;
    }

    /* Free canvas buffer from PSRAM */
    if (s_canvas_buf) {
        heap_caps_free(s_canvas_buf);
        s_canvas_buf = NULL;
    }

    /* Null all static pointers */
    s_screen    = NULL;
    s_canvas    = NULL;
    s_hidden_ta = NULL;

    ESP_LOGI(TAG, "Screensaver cleanup complete, canvas buffer freed");
}
