#ifndef UI_H_
#define UI_H_

#include "types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

/* ----------------------------------------------------------------
 * Colour palette (R, G, B)
 * ---------------------------------------------------------------- */
#define C_BG       18,  18,  28   /* main background          */
#define C_PANEL    38,  38,  58   /* card / list background   */
#define C_SEL      45,  95, 195   /* selected row             */
#define C_TITLE    90, 170, 255   /* header text              */
#define C_TEXT    210, 210, 220   /* primary text             */
#define C_SUBTLE  120, 120, 140   /* secondary / hint text    */
#define C_ACCENT  255, 110,  40   /* warning / accent         */
#define C_OK       65, 185,  65   /* success green            */
#define C_ERR     210,  50,  50   /* error red                */
#define C_BAR      55, 145, 255   /* progress bar fill        */

/* ----------------------------------------------------------------
 * Layout constants
 * ---------------------------------------------------------------- */
#define HEADER_H        52
#define HINT_H          28
#define LIST_TOP        (HEADER_H + 6)
#define LIST_BOT        (WIN_H - HINT_H - 4)
#define ROW_H           46
#define LIST_ROWS       ((LIST_BOT - LIST_TOP) / ROW_H)

/* Menu item counts (used by both ui.c and input.c) */
#define MAIN_ITEM_COUNT   4
#define CUSTOM_ITEM_COUNT 2

/* ----------------------------------------------------------------
 * Font loading
 * ---------------------------------------------------------------- */
int  ui_load_fonts(void);
void ui_free_fonts(void);

/* ----------------------------------------------------------------
 * Drawing primitives
 * ---------------------------------------------------------------- */
void ui_set_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a);
void ui_fill_rect(int x, int y, int w, int h);
void ui_draw_rect(int x, int y, int w, int h);

/* Render UTF-8 text; returns rendered width in pixels. */
int  ui_text(TTF_Font *font, const char *text,
             int x, int y, Uint8 r, Uint8 g, Uint8 b);

/* Same, but horizontally centred on cx. */
void ui_text_center(TTF_Font *font, const char *text,
                    int cx, int y, Uint8 r, Uint8 g, Uint8 b);

/* Truncate `text` so it fits in `max_px` pixels; returns static buffer. */
const char *ui_truncate(TTF_Font *font, const char *text, int max_px);

/* Horizontal progress bar (filled from left to right). */
void ui_progress_bar(int x, int y, int w, int h, double pct,
                     Uint8 r, Uint8 g, Uint8 b);

/* ----------------------------------------------------------------
 * Composite UI elements
 * ---------------------------------------------------------------- */
void ui_header(const char *title, const char *subtitle);
void ui_hints(const char *text);

/*
 * ui_list – draw a scrollable list.
 *   names[]  – primary label for each row (required)
 *   descs[]  – secondary label (may be NULL to skip)
 *   count    – total entry count
 *   sel      – currently selected index
 *   scroll   – first visible index
 */
void ui_list(const char **names, const char **descs,
             int count, int sel, int scroll);

/* ----------------------------------------------------------------
 * Full-screen renderers (one per Screen enum value)
 * ---------------------------------------------------------------- */
void ui_render_main(void);
void ui_render_fetching(void);
void ui_render_os_list(void);
void ui_render_custom(void);
void ui_render_file_browser(void);
void ui_render_device_select(void);
void ui_render_confirm(void);
void ui_render_flashing(void);
void ui_render_done(void);
void ui_render_error(void);
void ui_render_about(void);

/* Top-level: clear, dispatch, present. */
void ui_render_frame(void);

#endif /* UI_H_ */
