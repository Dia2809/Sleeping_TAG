#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * Font loading
 * ================================================================ */
static const char *FONT_SEARCH[] = {
    /* Debian / Ubuntu / RPi OS */
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    /* Arch / Manjaro */
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    /* PortMaster bundled fallback */
    "./assets/font.ttf",
    "../rpiimager/assets/font.ttf",
    NULL
};

static TTF_Font *open_font(int size)
{
    for (int i = 0; FONT_SEARCH[i]; i++) {
        TTF_Font *f = TTF_OpenFont(FONT_SEARCH[i], size);
        if (f) return f;
    }
    return NULL;
}

int ui_load_fonts(void)
{
    G.font_lg = open_font(22);
    G.font_md = open_font(15);
    G.font_sm = open_font(11);
    return (G.font_lg && G.font_md && G.font_sm) ? 0 : -1;
}

void ui_free_fonts(void)
{
    if (G.font_lg) { TTF_CloseFont(G.font_lg); G.font_lg = NULL; }
    if (G.font_md) { TTF_CloseFont(G.font_md); G.font_md = NULL; }
    if (G.font_sm) { TTF_CloseFont(G.font_sm); G.font_sm = NULL; }
}

/* ================================================================
 * Drawing primitives
 * ================================================================ */
void ui_set_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(G.ren, r, g, b, a);
}

void ui_fill_rect(int x, int y, int w, int h)
{
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderFillRect(G.ren, &rc);
}

void ui_draw_rect(int x, int y, int w, int h)
{
    SDL_Rect rc = {x, y, w, h};
    SDL_RenderDrawRect(G.ren, &rc);
}

int ui_text(TTF_Font *font, const char *text,
            int x, int y, Uint8 r, Uint8 g, Uint8 b)
{
    if (!font || !text || !text[0]) return 0;
    SDL_Color col = {r, g, b, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return 0;

    int tw = surf->w;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(G.ren, surf);
    SDL_FreeSurface(surf);
    if (!tex) return 0;

    SDL_Rect dst = {x, y, tw, surf ? 0 : 0}; /* surf freed – recalc */
    /* Re-query texture size (surf is freed) */
    int th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    dst.w = tw; dst.h = th;

    SDL_RenderCopy(G.ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    return tw;
}

void ui_text_center(TTF_Font *font, const char *text,
                    int cx, int y, Uint8 r, Uint8 g, Uint8 b)
{
    if (!font || !text || !text[0]) return;
    int w, h;
    TTF_SizeUTF8(font, text, &w, &h);
    ui_text(font, text, cx - w / 2, y, r, g, b);
}

const char *ui_truncate(TTF_Font *font, const char *text, int max_px)
{
    static char buf[512];
    if (!font) return text;

    int w, h;
    TTF_SizeUTF8(font, text, &w, &h);
    if (w <= max_px) return text;

    /* Binary-search for the longest prefix that fits with "…" appended */
    int lo = 0, hi = (int)strlen(text);
    while (lo < hi) {
        int  mid = (lo + hi + 1) / 2;
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%.*s\xe2\x80\xa6", mid, text); /* UTF-8 ellipsis */
        TTF_SizeUTF8(font, tmp, &w, &h);
        if (w <= max_px) lo = mid;
        else             hi = mid - 1;
    }
    snprintf(buf, sizeof(buf), "%.*s\xe2\x80\xa6", lo, text);
    return buf;
}

void ui_progress_bar(int x, int y, int w, int h, double pct,
                     Uint8 r, Uint8 g, Uint8 b)
{
    /* Track */
    ui_set_color(C_PANEL, 255);
    ui_fill_rect(x, y, w, h);

    /* Fill */
    int fill = (int)(pct * w);
    if (fill > 0) {
        ui_set_color(r, g, b, 255);
        ui_fill_rect(x, y, fill, h);
    }

    /* Border */
    ui_set_color(C_SUBTLE, 255);
    ui_draw_rect(x, y, w, h);
}

/* ================================================================
 * Composite elements
 * ================================================================ */
void ui_header(const char *title, const char *subtitle)
{
    ui_set_color(C_PANEL, 255);
    ui_fill_rect(0, 0, WIN_W, HEADER_H);

    if (subtitle) {
        ui_text_center(G.font_lg, title, WIN_W / 2, 6,    C_TITLE);
        ui_text_center(G.font_sm, subtitle, WIN_W / 2, 32, C_SUBTLE);
    } else {
        ui_text_center(G.font_lg, title, WIN_W / 2, 14, C_TITLE);
    }
}

void ui_hints(const char *text)
{
    ui_set_color(C_PANEL, 255);
    ui_fill_rect(0, WIN_H - HINT_H, WIN_W, HINT_H);
    ui_text_center(G.font_sm, text, WIN_W / 2, WIN_H - HINT_H + 7, C_SUBTLE);
}

void ui_list(const char **names, const char **descs,
             int count, int sel, int scroll)
{
    int rows = LIST_ROWS;
    for (int i = 0; i < rows && (i + scroll) < count; i++) {
        int idx   = i + scroll;
        int y     = LIST_TOP + i * ROW_H;
        int is_sel = (idx == sel);

        if (is_sel) {
            ui_set_color(C_SEL, 255);
        } else {
            Uint8 shade = (Uint8)(idx % 2 == 0 ? 42 : 38);
            ui_set_color(shade, shade, shade + 18, 255);
        }
        ui_fill_rect(8, y, WIN_W - 16, ROW_H - 3);

        Uint8 tr = is_sel ? 255 : 210;
        Uint8 tg = is_sel ? 255 : 210;
        Uint8 tb = is_sel ? 255 : 220;

        int tx  = 18;
        int ty1 = y + (descs ? 4  : 14);
        int ty2 = y + 24;

        ui_text(G.font_md,
                ui_truncate(G.font_md, names[idx], WIN_W - 36),
                tx, ty1, tr, tg, tb);

        if (descs && descs[idx] && descs[idx][0]) {
            Uint8 sr = is_sel ? 190 : 120;
            Uint8 sg = is_sel ? 200 : 120;
            Uint8 sb = is_sel ? 210 : 140;
            ui_text(G.font_sm,
                    ui_truncate(G.font_sm, descs[idx], WIN_W - 36),
                    tx, ty2, sr, sg, sb);
        }
    }

    /* Scroll thumb */
    if (count > rows) {
        int track = LIST_BOT - LIST_TOP;
        int thumb = track * rows / count;
        if (thumb < 16) thumb = 16;
        int thumb_y = LIST_TOP + (track - thumb) * scroll / (count - rows);
        ui_set_color(C_SEL, 200);
        ui_fill_rect(WIN_W - 5, thumb_y, 3, thumb);
    }
}

/* ================================================================
 * Screen renderers
 * ================================================================ */

/* ---- Main menu ---- */
static const char *MAIN_ITEMS[] = {
    "  Choose OS from catalog",
    "  Use a custom image file",
    "  About",
    "  Quit"
};
#define MAIN_ITEM_H 56

void ui_render_main(void)
{
    ui_header("RPI Imager", "Flash OS images to your SD card");

    for (int i = 0; i < MAIN_ITEM_COUNT; i++) {
        int y      = LIST_TOP + i * MAIN_ITEM_H;
        int is_sel = (i == G.main_sel);

        if (is_sel) { ui_set_color(C_SEL,   255); }
        else        { ui_set_color(42, 42, 62, 255); }
        ui_fill_rect(40, y, WIN_W - 80, MAIN_ITEM_H - 6);

        Uint8 tr = is_sel ? 255 : 210;
        Uint8 tg = is_sel ? 255 : 210;
        Uint8 tb = is_sel ? 255 : 220;
        ui_text(G.font_md, MAIN_ITEMS[i], 58, y + 16, tr, tg, tb);
    }

    ui_hints("[D-PAD] Navigate   [A] Select   [START] Quit");
}

/* ---- Fetching screen ---- */
void ui_render_fetching(void)
{
    ui_header("Fetching Catalogs", NULL);
    ui_text_center(G.font_md, G.status_msg[0] ? G.status_msg : "Please wait…",
                   WIN_W / 2, WIN_H / 2 - 20, C_TEXT);

    /* Animated dots based on SDL tick */
    int dots = (SDL_GetTicks() / 400) % 4;
    char buf[8] = "";
    for (int i = 0; i < dots; i++) strcat(buf, ".");
    ui_text_center(G.font_lg, buf, WIN_W / 2, WIN_H / 2 + 14, C_TITLE);

    ui_hints("[B] Cancel");
}

/* ---- OS list ---- */
void ui_render_os_list(void)
{
    char subtitle[64];
    snprintf(subtitle, sizeof(subtitle), "%d images available", G.os_count);
    ui_header("Choose OS", subtitle);

    if (G.os_count == 0) {
        ui_text_center(G.font_md, "No images found.",
                       WIN_W / 2, WIN_H / 2 - 10, C_SUBTLE);
        ui_text_center(G.font_sm, "Check your network connection.",
                       WIN_W / 2, WIN_H / 2 + 16, C_SUBTLE);
    } else {
        static const char *names[MAX_OSES];
        static const char *descs[MAX_OSES];
        for (int i = 0; i < G.os_count; i++) {
            names[i] = G.oses[i].name;
            descs[i] = G.oses[i].description[0] ? G.oses[i].description
                                                 : G.oses[i].source_name;
        }
        ui_list(names, descs, G.os_count, G.os_sel, G.os_scroll);
    }

    ui_hints("[D-PAD] Navigate   [A] Select   [B] Back");
}

/* ---- Custom image sub-menu ---- */
static const char *CUSTOM_ITEMS[] = {
    "  Browse filesystem for image file",
    "  Back to main menu"
};

void ui_render_custom(void)
{
    ui_header("Custom Image", "Select an image from your storage");
    for (int i = 0; i < CUSTOM_ITEM_COUNT; i++) {
        int y      = LIST_TOP + i * MAIN_ITEM_H;
        int is_sel = (i == G.custom_sel);
        if (is_sel) { ui_set_color(C_SEL,   255); }
        else        { ui_set_color(42, 42, 62, 255); }
        ui_fill_rect(40, y, WIN_W - 80, MAIN_ITEM_H - 6);
        Uint8 tr = is_sel ? 255 : 210;
        ui_text(G.font_md, CUSTOM_ITEMS[i], 58, y + 16, tr, tr, tr + 10);
    }
    ui_hints("[D-PAD] Navigate   [A] Select   [B] Back");
}

/* ---- File browser ---- */
void ui_render_file_browser(void)
{
    ui_header("Browse for Image",
              ui_truncate(G.font_sm, G.browser_dir, WIN_W - 20));

    if (G.file_count == 0) {
        ui_text_center(G.font_md, "No compatible images here.",
                       WIN_W / 2, WIN_H / 2 - 10, C_SUBTLE);
    } else {
        static const char *names[MAX_FILES];
        static const char *descs[MAX_FILES];
        for (int i = 0; i < G.file_count; i++) {
            names[i] = G.files[i].name;
            descs[i] = G.files[i].is_dir ? "[folder]" : "";
        }
        ui_list(names, descs, G.file_count, G.file_sel, G.file_scroll);
    }

    ui_hints("[D-PAD] Navigate   [A] Open/Select   [B] Back");
}

/* ---- Device select ---- */
void ui_render_device_select(void)
{
    ui_header("Choose Storage Device", "Target for the image write");

    if (G.dev_count == 0) {
        ui_text_center(G.font_md, "No block devices found.",
                       WIN_W / 2, WIN_H / 2 - 14, C_SUBTLE);
        ui_text_center(G.font_sm, "Insert an SD card, then press [Y] to refresh.",
                       WIN_W / 2, WIN_H / 2 + 10, C_SUBTLE);
    } else {
        static const char *names[MAX_DEVICES];
        static const char *descs[MAX_DEVICES];
        static const char  WARN[] = "All data will be erased!";
        for (int i = 0; i < G.dev_count; i++) {
            names[i] = G.devices[i].display;
            descs[i] = WARN;
        }
        ui_list(names, descs, G.dev_count, G.dev_sel, G.dev_scroll);
    }

    ui_hints("[D-PAD] Navigate   [A] Select   [Y] Refresh   [B] Back");
}

/* ---- Confirm ---- */
void ui_render_confirm(void)
{
    ui_header("Confirm Flash", NULL);

    int y = LIST_TOP + 10;
    ui_text(G.font_sm, "Image:", 20, y, C_SUBTLE);
    y += 20;
    ui_text(G.font_md,
            ui_truncate(G.font_md, G.image_name, WIN_W - 30),
            20, y, C_TEXT);
    y += 32;
    ui_text(G.font_sm, "Destination:", 20, y, C_SUBTLE);
    y += 20;
    if (G.dev_count > 0)
        ui_text(G.font_md, G.devices[G.dev_sel].display,
                20, y, C_ACCENT);
    y += 42;
    ui_text_center(G.font_md,
                   "WARNING: ALL data on the device will be ERASED!",
                   WIN_W / 2, y, C_ERR);

    ui_hints("[A] Flash Now   [B] Cancel");
}

/* ---- Flashing ---- */
void ui_render_flashing(void)
{
    ui_header("Flashing…", "Do not remove the device");

    pthread_mutex_lock(&G.flash.lock);
    double   prog  = G.flash.progress;
    int      phase = G.flash.phase;
    pthread_mutex_unlock(&G.flash.lock);

    const char *phase_str = (phase == 0) ? "Downloading" : "Writing";
    char buf[64];
    snprintf(buf, sizeof(buf), "%s  %.0f%%", phase_str, prog * 100.0);

    ui_text_center(G.font_md, buf, WIN_W / 2, WIN_H / 2 - 40, C_TEXT);
    ui_progress_bar(40, WIN_H / 2 - 12, WIN_W - 80, 24, prog, C_BAR);

    ui_hints("Please wait…");
}

/* ---- Done ---- */
void ui_render_done(void)
{
    ui_header("Done!", NULL);
    ui_text_center(G.font_md, "Image written successfully.",
                   WIN_W / 2, WIN_H / 2 - 28, C_OK);
    ui_text_center(G.font_sm, "You may safely remove the device.",
                   WIN_W / 2, WIN_H / 2 + 6, C_TEXT);
    ui_hints("[A] Return to main menu");
}

/* ---- Error ---- */
void ui_render_error(void)
{
    ui_header("Error", NULL);
    ui_text_center(G.font_md,
                   ui_truncate(G.font_md, G.error_msg, WIN_W - 40),
                   WIN_W / 2, WIN_H / 2 - 20, C_ERR);
    ui_hints("[A] or [B]  Return to main menu");
}

/* ---- About ---- */
void ui_render_about(void)
{
    ui_header("About", "RPI Imager Port for PortMaster");
    static const char *lines[] = {
        "A Raspberry Pi Imager-style tool",
        "with gamepad / controller support.",
        "",
        "OS catalogs are fetched live from",
        "the URLs listed in oses.json.",
        "Add your own sources to that file!",
        "",
        "Built with SDL2, SDL2_ttf, libcurl.",
        NULL
    };
    int y = LIST_TOP + 6;
    for (int i = 0; lines[i]; i++, y += 22)
        ui_text_center(G.font_sm, lines[i], WIN_W / 2, y, C_TEXT);
    ui_hints("[B] Back");
}

/* ================================================================
 * Top-level frame render
 * ================================================================ */
void ui_render_frame(void)
{
    ui_set_color(C_BG, 255);
    SDL_RenderClear(G.ren);

    switch (G.screen) {
    case SCR_MAIN:          ui_render_main();          break;
    case SCR_FETCHING:      ui_render_fetching();      break;
    case SCR_OS_LIST:       ui_render_os_list();       break;
    case SCR_CUSTOM:        ui_render_custom();        break;
    case SCR_FILE_BROWSER:  ui_render_file_browser();  break;
    case SCR_DEVICE_SELECT: ui_render_device_select(); break;
    case SCR_CONFIRM:       ui_render_confirm();       break;
    case SCR_FLASHING:      ui_render_flashing();      break;
    case SCR_DONE:          ui_render_done();          break;
    case SCR_ERROR:         ui_render_error();         break;
    case SCR_ABOUT:         ui_render_about();         break;
    default: break;
    }

    SDL_RenderPresent(G.ren);
}
