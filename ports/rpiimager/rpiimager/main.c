/*
 * main.c – RPI Imager Port (PortMaster)
 *
 * Entry point, SDL2 initialisation, and the main event loop.
 * All domain logic lives in the other translation units:
 *
 *   oslist.c  – fetch + parse remote OS catalogs
 *   flash.c   – download image + write to block device
 *   ui.c      – SDL2 rendering
 *   input.c   – controller / keyboard input
 *   json.c    – jsmn wrapper
 *   net.c     – libcurl HTTP client
 */

#include "types.h"
#include "ui.h"
#include "input.h"
#include "flash.h"
#include "oslist.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <curl/curl.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Global application state – single instance, defined here.
 * All other TUs access it via `extern App G` (declared in types.h).
 * ---------------------------------------------------------------- */
App G;

/* ----------------------------------------------------------------
 * Catalog JSON search paths (tried in order)
 * ---------------------------------------------------------------- */
static const char *OSES_JSON_PATHS[] = {
    "oses.json",                    /* run from the rpiimager/ dir   */
    "rpiimager/oses.json",          /* run from the port root        */
    "../rpiimager/oses.json",
    NULL
};

/* ----------------------------------------------------------------
 * SDL2 initialisation
 * ---------------------------------------------------------------- */
static int sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO |
                 SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "[main] SDL_Init: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() != 0) {
        fprintf(stderr, "[main] TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    G.win = SDL_CreateWindow(
        "RPI Imager",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!G.win) {
        fprintf(stderr, "[main] SDL_CreateWindow: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return -1;
    }

    /* Try hardware-accelerated renderer first, fall back to software */
    G.ren = SDL_CreateRenderer(
        G.win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!G.ren)
        G.ren = SDL_CreateRenderer(G.win, -1, SDL_RENDERER_SOFTWARE);

    if (!G.ren) {
        fprintf(stderr, "[main] SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(G.win);
        TTF_Quit(); SDL_Quit();
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderSetLogicalSize(G.ren, WIN_W, WIN_H);

    return 0;
}

static void sdl_quit(void)
{
    ui_free_fonts();
    if (G.ctrl) SDL_GameControllerClose(G.ctrl);
    if (G.ren)  SDL_DestroyRenderer(G.ren);
    if (G.win)  SDL_DestroyWindow(G.win);
    TTF_Quit();
    SDL_Quit();
}

/* ----------------------------------------------------------------
 * Poll the fetch-thread state and transition screens when done
 * ---------------------------------------------------------------- */
static void poll_fetch(void)
{
    if (G.screen != SCR_FETCHING) return;

    pthread_mutex_lock(&G.fetch.lock);
    int done  = G.fetch.done;
    int error = G.fetch.error;
    char emsg[256];
    strncpy(emsg, G.fetch.error_msg, sizeof(emsg));
    pthread_mutex_unlock(&G.fetch.lock);

    if (!done) return;

    pthread_join(G.fetch_thread, NULL);

    if (error && G.os_count == 0) {
        /* All sources failed */
        strncpy(G.error_msg, emsg[0] ? emsg : "Failed to fetch any catalog.",
                sizeof(G.error_msg) - 1);
        G.screen = SCR_ERROR;
    } else {
        /* At least some OSes loaded – show the list */
        G.os_sel    = 0;
        G.os_scroll = 0;
        G.screen    = SCR_OS_LIST;
    }
}

/* ----------------------------------------------------------------
 * Poll the flash-thread state and transition screens when done
 * ---------------------------------------------------------------- */
static void poll_flash(void)
{
    if (G.screen != SCR_FLASHING) return;

    pthread_mutex_lock(&G.flash.lock);
    int done  = G.flash.done;
    int error = G.flash.error;
    char emsg[256];
    strncpy(emsg, G.flash.error_msg, sizeof(emsg));
    pthread_mutex_unlock(&G.flash.lock);

    if (!done) return;

    flash_join();

    if (error) {
        strncpy(G.error_msg, emsg[0] ? emsg : "Unknown flash error.",
                sizeof(G.error_msg) - 1);
        G.screen = SCR_ERROR;
    } else {
        G.screen = SCR_DONE;
    }
}

/* ----------------------------------------------------------------
 * Process one SDL event
 * ---------------------------------------------------------------- */
static void handle_event(const SDL_Event *ev)
{
    switch (ev->type) {

    case SDL_QUIT:
        G.running = 0;
        break;

    case SDL_KEYDOWN: {
        LogicalBtn btn = input_map_key(ev->key.keysym.sym);
        if (btn != BTN_NONE) input_handle(btn);
        break;
    }

    case SDL_CONTROLLERBUTTONDOWN: {
        LogicalBtn btn = input_map_controller(
            (SDL_GameControllerButton)ev->cbutton.button);
        if (btn != BTN_NONE) input_handle(btn);
        break;
    }

    case SDL_CONTROLLERDEVICEADDED:
        input_open_controller(ev->cdevice.which);
        break;

    case SDL_CONTROLLERDEVICEREMOVED:
        input_close_controller(ev->cdevice.which);
        break;

    default:
        break;
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    /* --- Zero-init global state --- */
    memset(&G, 0, sizeof(G));
    G.running  = 1;
    G.screen   = SCR_MAIN;
    G.main_sel = 0;
    strncpy(G.browser_dir, "/", sizeof(G.browser_dir) - 1);

    pthread_mutex_init(&G.fetch.lock, NULL);
    pthread_mutex_init(&G.flash.lock, NULL);

    /* --- Load local catalog source list --- */
    int sources_loaded = 0;
    for (int i = 0; OSES_JSON_PATHS[i]; i++) {
        if (oslist_load_sources(OSES_JSON_PATHS[i]) > 0) {
            fprintf(stderr, "[main] Loaded sources from %s (%d sources)\n",
                    OSES_JSON_PATHS[i], G.source_count);
            sources_loaded = 1;
            break;
        }
    }
    if (!sources_loaded)
        fprintf(stderr, "[main] Warning: oses.json not found or has no sources\n");

    /* --- Init SDL2 + fonts --- */
    if (sdl_init() != 0) return 1;

    if (ui_load_fonts() != 0)
        fprintf(stderr, "[main] Warning: fonts not found – text may be missing\n");

    /* --- Init libcurl --- */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* --- Open any connected controller --- */
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        input_open_controller(i);

    /* ================================================================
     * Main loop
     * ================================================================ */
    SDL_Event ev;
    while (G.running) {

        /* Poll background threads */
        poll_fetch();
        poll_flash();

        /* Drain SDL event queue */
        while (SDL_PollEvent(&ev))
            handle_event(&ev);

        /* Render */
        ui_render_frame();

        /* Cap at ~60 fps when vsync is not available */
        SDL_Delay(16);
    }

    /* ================================================================
     * Cleanup
     * ================================================================ */
    /* If threads are still running (e.g. user quit during fetch),
       they will finish naturally – we detach rather than block forever. */
    if (!G.fetch.done) pthread_detach(G.fetch_thread);
    if (!G.flash.done) pthread_detach(G.flash_thread);

    curl_global_cleanup();
    sdl_quit();

    pthread_mutex_destroy(&G.fetch.lock);
    pthread_mutex_destroy(&G.flash.lock);

    return 0;
}
