#ifndef TYPES_H_
#define TYPES_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <pthread.h>

/* ----------------------------------------------------------------
 * Tuneable limits
 * ---------------------------------------------------------------- */
#define WIN_W          640
#define WIN_H          480

#define MAX_SOURCES     16   /* catalog providers in oses.json        */
#define MAX_OSES       256   /* OS entries after flattening all feeds  */
#define MAX_DEVICES     16   /* block devices for flashing             */
#define MAX_FILES      256   /* entries shown in the file browser      */

#define MAX_PATH_LEN   512
#define MAX_NAME_LEN   128
#define MAX_DESC_LEN   512
#define MAX_URL_LEN   1024

/* ----------------------------------------------------------------
 * Screen / navigation state
 * ---------------------------------------------------------------- */
typedef enum {
    SCR_MAIN = 0,
    SCR_FETCHING,      /* background catalog fetch in progress */
    SCR_OS_LIST,       /* scrollable OS picker                 */
    SCR_CUSTOM,        /* "use custom image" sub-menu          */
    SCR_FILE_BROWSER,  /* navigate the filesystem              */
    SCR_DEVICE_SELECT, /* pick target block device             */
    SCR_CONFIRM,       /* final warning before write           */
    SCR_FLASHING,      /* download → write progress            */
    SCR_DONE,          /* success                              */
    SCR_ERROR,         /* unrecoverable error                  */
    SCR_ABOUT,
    SCR_COUNT
} Screen;

/* ----------------------------------------------------------------
 * Domain types
 * ---------------------------------------------------------------- */

/* One OS catalog provider, loaded from oses.json */
typedef struct {
    char id[64];
    char name[MAX_NAME_LEN];
    char catalog_url[MAX_URL_LEN];
    char format[32];     /* "rpi" | "generic" */
} OSSource;

/* One flashable OS entry – populated from a remote catalog */
typedef struct {
    char name[MAX_NAME_LEN];
    char description[MAX_DESC_LEN];
    char url[MAX_URL_LEN];
    char sha256[65];           /* hex SHA-256, may be empty */
    long long download_size;   /* bytes, 0 = unknown        */
    long long extract_size;    /* bytes, 0 = unknown        */
    char source_name[MAX_NAME_LEN]; /* which provider       */
} OSEntry;

/* A writable block device */
typedef struct {
    char path[MAX_PATH_LEN];
    char display[MAX_NAME_LEN];      /* e.g. "sdb  (32.0 GB)  SanDisk Ultra" */
    unsigned long long size_bytes;
} DeviceEntry;

/* File-browser entry */
typedef struct {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    int  is_dir;
    long size_bytes;
} FileEntry;

/* ----------------------------------------------------------------
 * Thread-shared state structures
 * ---------------------------------------------------------------- */

/* Catalog fetch progress (written by fetch thread, read by UI) */
typedef struct {
    volatile int done;
    volatile int error;
    char         error_msg[256];
    pthread_mutex_t lock;
} FetchState;

/* Flash / write progress (written by flash thread, read by UI) */
typedef struct {
    volatile double progress; /* 0.0 – 1.0                        */
    volatile int    phase;    /* 0 = downloading, 1 = writing      */
    volatile int    done;
    volatile int    error;
    char            error_msg[256];
    pthread_mutex_t lock;
} FlashState;

/* ----------------------------------------------------------------
 * Central application state  (lives in main.c as `App G`)
 * ---------------------------------------------------------------- */
typedef struct {
    Screen screen;
    int    running;

    /* Catalog sources (from local oses.json) */
    OSSource sources[MAX_SOURCES];
    int      source_count;

    /* OS entries populated after remote fetch */
    OSEntry  oses[MAX_OSES];
    int      os_count;
    int      os_sel;
    int      os_scroll;

    /* Block devices */
    DeviceEntry devices[MAX_DEVICES];
    int         dev_count;
    int         dev_sel;
    int         dev_scroll;

    /* File browser */
    FileEntry  files[MAX_FILES];
    int        file_count;
    int        file_sel;
    int        file_scroll;
    char       browser_dir[MAX_PATH_LEN];

    /* Currently selected image (URL or local path) */
    char image_url[MAX_URL_LEN];
    char image_name[MAX_NAME_LEN];
    int  image_is_local; /* 1 = local file, 0 = remote URL */

    /* Main-menu cursor (4 items) */
    int main_sel;

    /* Custom-image sub-menu cursor (2 items) */
    int custom_sel;

    /* SDL2 handles */
    SDL_Window   *win;
    SDL_Renderer *ren;
    TTF_Font     *font_lg; /* ~22 pt */
    TTF_Font     *font_md; /* ~15 pt */
    TTF_Font     *font_sm; /* ~11 pt */

    /* Controller */
    SDL_GameController *ctrl;

    /* Worker threads */
    FetchState fetch;
    pthread_t  fetch_thread;

    FlashState flash;
    pthread_t  flash_thread;

    /* Error / status messages shown on-screen */
    char error_msg[256];
    char status_msg[256];
} App;

extern App G;

#endif /* TYPES_H_ */
