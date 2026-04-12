#include "input.h"
#include "flash.h"
#include "oslist.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>

/* ================================================================
 * Button mapping
 * ================================================================ */
LogicalBtn input_map_controller(SDL_GameControllerButton b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return BTN_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return BTN_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return BTN_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return BTN_RIGHT;
    case SDL_CONTROLLER_BUTTON_A:          return BTN_A;
    case SDL_CONTROLLER_BUTTON_B:          return BTN_B;
    case SDL_CONTROLLER_BUTTON_Y:          return BTN_Y;
    case SDL_CONTROLLER_BUTTON_START:      return BTN_START;
    default:                               return BTN_NONE;
    }
}

LogicalBtn input_map_key(SDL_Keycode k)
{
    switch (k) {
    case SDLK_UP:      return BTN_UP;
    case SDLK_DOWN:    return BTN_DOWN;
    case SDLK_LEFT:    return BTN_LEFT;
    case SDLK_RIGHT:   return BTN_RIGHT;
    case SDLK_RETURN:
    case SDLK_z:       return BTN_A;
    case SDLK_ESCAPE:
    case SDLK_x:       return BTN_B;
    case SDLK_y:
    case SDLK_s:       return BTN_Y;
    case SDLK_F1:      return BTN_START;
    default:           return BTN_NONE;
    }
}

/* ================================================================
 * Internal helpers
 * ================================================================ */

/* Clamp-scroll a list selection by delta, keeping sel in [0, count-1]
   and adjusting scroll so sel is always visible. */
static void nav(int *sel, int *scroll, int count, int delta)
{
    if (count <= 0) return;
    *sel += delta;
    if (*sel < 0)       *sel = 0;
    if (*sel >= count)  *sel = count - 1;
    if (*sel < *scroll)                  *scroll = *sel;
    if (*sel >= *scroll + LIST_ROWS)     *scroll = *sel - LIST_ROWS + 1;
}

/* Accepted image file extensions for the file browser */
static int is_image_file(const char *name)
{
    const char *e = strrchr(name, '.');
    if (!e) return 0;
    return strcmp(e, ".img") == 0 || strcmp(e, ".iso") == 0 ||
           strcmp(e, ".zip") == 0 || strcmp(e, ".xz")  == 0 ||
           strcmp(e, ".gz")  == 0 || strcmp(e, ".bz2") == 0;
}

static int cmp_file(const void *a, const void *b)
{
    const FileEntry *fa = (const FileEntry *)a;
    const FileEntry *fb = (const FileEntry *)b;
    if (fa->is_dir != fb->is_dir) return fb->is_dir - fa->is_dir;
    return strcmp(fa->name, fb->name);
}

static void browse(const char *path)
{
    strncpy(G.browser_dir, path, MAX_PATH_LEN - 1);
    G.file_count = 0;
    G.file_sel   = 0;
    G.file_scroll = 0;

    DIR *d = opendir(path);
    if (!d) return;

    /* Parent directory link */
    if (strcmp(path, "/") != 0 && G.file_count < MAX_FILES) {
        FileEntry *e = &G.files[G.file_count++];
        strncpy(e->name, "..", sizeof(e->name) - 1);
        snprintf(e->path, sizeof(e->path), "%s/..", path);
        e->is_dir     = 1;
        e->size_bytes = 0;
    }

    struct dirent *de;
    while ((de = readdir(d)) && G.file_count < MAX_FILES) {
        if (de->d_name[0] == '.') continue;

        char full[MAX_PATH_LEN];
        snprintf(full, sizeof(full), "%s/%s", path, de->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        int is_dir = S_ISDIR(st.st_mode);
        if (!is_dir && !is_image_file(de->d_name)) continue;

        FileEntry *e = &G.files[G.file_count++];
        strncpy(e->name, de->d_name, sizeof(e->name) - 1);
        strncpy(e->path, full,       sizeof(e->path)  - 1);
        e->is_dir     = is_dir;
        e->size_bytes = is_dir ? 0 : (long)st.st_size;
    }
    closedir(d);
    qsort(G.files, (size_t)G.file_count, sizeof(FileEntry), cmp_file);
}

/* Enumerate writable block devices from /sys/block */
static int enum_devices(void)
{
    G.dev_count  = 0;
    G.dev_sel    = 0;
    G.dev_scroll = 0;

    DIR *d = opendir("/sys/block");
    if (!d) return 0;

    struct dirent *de;
    while ((de = readdir(d)) && G.dev_count < MAX_DEVICES) {
        const char *n = de->d_name;
        if (n[0] == '.') continue;
        if (strncmp(n, "sd",     2) != 0 &&
            strncmp(n, "mmcblk", 6) != 0 &&
            strncmp(n, "nvme",   4) != 0) continue;

        /* Read sector count */
        char spath[256];
        snprintf(spath, sizeof(spath), "/sys/block/%s/size", n);
        FILE *f = fopen(spath, "r");
        if (!f) continue;
        unsigned long long sectors = 0;
        fscanf(f, "%llu", &sectors);
        fclose(f);
        if (sectors == 0) continue;

        unsigned long long bytes = sectors * 512ULL;

        /* Try to read device model */
        char model[MAX_NAME_LEN] = "";
        const char *model_paths[] = {
            "/sys/block/%s/device/model",
            "/sys/block/%s/device/name",
            NULL
        };
        for (int i = 0; model_paths[i]; i++) {
            char mp[256];
            snprintf(mp, sizeof(mp), model_paths[i], n);
            FILE *mf = fopen(mp, "r");
            if (mf) {
                fgets(model, sizeof(model), mf);
                fclose(mf);
                /* Strip trailing whitespace */
                int l = (int)strlen(model);
                while (l > 0 && (model[l-1] == '\n' || model[l-1] == '\r' ||
                                  model[l-1] == ' '))
                    model[--l] = '\0';
                if (model[0]) break;
            }
        }

        DeviceEntry *e = &G.devices[G.dev_count++];
        snprintf(e->path, sizeof(e->path), "/dev/%s", n);
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        if (model[0])
            snprintf(e->display, sizeof(e->display),
                     "%s  (%.1f GB)  %s", n, gb, model);
        else
            snprintf(e->display, sizeof(e->display),
                     "%s  (%.1f GB)", n, gb);
        e->size_bytes = bytes;
    }
    closedir(d);
    return G.dev_count;
}

/* ================================================================
 * Main input handler – state machine
 * ================================================================ */
void input_handle(LogicalBtn btn)
{
    switch (G.screen) {

    /* ---- Main menu ---- */
    case SCR_MAIN:
        if (btn == BTN_DOWN)  G.main_sel = (G.main_sel + 1) % MAIN_ITEM_COUNT;
        if (btn == BTN_UP)    G.main_sel = (G.main_sel + MAIN_ITEM_COUNT - 1)
                                           % MAIN_ITEM_COUNT;
        if (btn == BTN_START) { G.running = 0; return; }

        if (btn == BTN_A || btn == BTN_RIGHT) {
            switch (G.main_sel) {
            case 0: /* Choose OS → trigger fetch */
                G.os_count  = 0;
                G.os_sel    = 0;
                G.os_scroll = 0;
                snprintf(G.status_msg, sizeof(G.status_msg), "Initialising…");
                G.fetch.done  = 0;
                G.fetch.error = 0;
                G.fetch.error_msg[0] = '\0';
                G.screen = SCR_FETCHING;
                pthread_create(&G.fetch_thread, NULL, oslist_fetch_thread, NULL);
                break;
            case 1: /* Custom image */
                G.custom_sel = 0;
                G.screen     = SCR_CUSTOM;
                break;
            case 2: /* About */
                G.screen = SCR_ABOUT;
                break;
            case 3: /* Quit */
                G.running = 0;
                break;
            }
        }
        break;

    /* ---- Fetching ---- */
    case SCR_FETCHING:
        /* B cancels the fetch (we can't truly cancel the thread,
           but we can navigate away; the thread will finish quietly) */
        if (btn == BTN_B) {
            G.screen = SCR_MAIN;
        }
        break;

    /* ---- OS list ---- */
    case SCR_OS_LIST:
        if (btn == BTN_DOWN)  nav(&G.os_sel, &G.os_scroll, G.os_count, +1);
        if (btn == BTN_UP)    nav(&G.os_sel, &G.os_scroll, G.os_count, -1);
        if (btn == BTN_B || btn == BTN_LEFT) { G.screen = SCR_MAIN; break; }

        if (btn == BTN_A || btn == BTN_RIGHT) {
            if (G.os_count > 0) {
                strncpy(G.image_url,  G.oses[G.os_sel].url,
                        sizeof(G.image_url)  - 1);
                strncpy(G.image_name, G.oses[G.os_sel].name,
                        sizeof(G.image_name) - 1);
                G.image_is_local = 0;
                enum_devices();
                G.screen = SCR_DEVICE_SELECT;
            }
        }
        break;

    /* ---- Custom image sub-menu ---- */
    case SCR_CUSTOM:
        if (btn == BTN_DOWN || btn == BTN_UP)
            G.custom_sel ^= 1;
        if (btn == BTN_B || btn == BTN_LEFT) { G.screen = SCR_MAIN; break; }

        if (btn == BTN_A || btn == BTN_RIGHT) {
            if (G.custom_sel == 0) {
                browse("/");
                G.screen = SCR_FILE_BROWSER;
            } else {
                G.screen = SCR_MAIN;
            }
        }
        break;

    /* ---- File browser ---- */
    case SCR_FILE_BROWSER:
        if (btn == BTN_DOWN)  nav(&G.file_sel, &G.file_scroll, G.file_count, +1);
        if (btn == BTN_UP)    nav(&G.file_sel, &G.file_scroll, G.file_count, -1);
        if (btn == BTN_B || btn == BTN_LEFT) { G.screen = SCR_CUSTOM; break; }

        if (btn == BTN_A || btn == BTN_RIGHT) {
            if (G.file_count > 0) {
                FileEntry *fe = &G.files[G.file_sel];
                if (fe->is_dir) {
                    char resolved[MAX_PATH_LEN];
                    if (!realpath(fe->path, resolved))
                        strncpy(resolved, fe->path, sizeof(resolved) - 1);
                    browse(resolved);
                } else {
                    strncpy(G.image_url,  fe->path, sizeof(G.image_url)  - 1);
                    strncpy(G.image_name, fe->name, sizeof(G.image_name) - 1);
                    G.image_is_local = 1;
                    enum_devices();
                    G.screen = SCR_DEVICE_SELECT;
                }
            }
        }
        break;

    /* ---- Device select ---- */
    case SCR_DEVICE_SELECT:
        if (btn == BTN_DOWN)  nav(&G.dev_sel, &G.dev_scroll, G.dev_count, +1);
        if (btn == BTN_UP)    nav(&G.dev_sel, &G.dev_scroll, G.dev_count, -1);
        if (btn == BTN_Y)     { enum_devices(); break; }
        if (btn == BTN_B || btn == BTN_LEFT) {
            G.screen = G.image_is_local ? SCR_FILE_BROWSER : SCR_OS_LIST;
            break;
        }
        if ((btn == BTN_A || btn == BTN_RIGHT) && G.dev_count > 0)
            G.screen = SCR_CONFIRM;
        break;

    /* ---- Confirm ---- */
    case SCR_CONFIRM:
        if (btn == BTN_B) { G.screen = SCR_DEVICE_SELECT; break; }
        if (btn == BTN_A) {
            flash_start();
            G.screen = SCR_FLASHING;
        }
        break;

    /* ---- Flashing – no input accepted ---- */
    case SCR_FLASHING:
        break;

    /* ---- Done ---- */
    case SCR_DONE:
        if (btn == BTN_A || btn == BTN_B) {
            G.screen   = SCR_MAIN;
            G.main_sel = 0;
        }
        break;

    /* ---- Error ---- */
    case SCR_ERROR:
        if (btn == BTN_A || btn == BTN_B) {
            G.screen   = SCR_MAIN;
            G.main_sel = 0;
        }
        break;

    /* ---- About ---- */
    case SCR_ABOUT:
        if (btn == BTN_B || btn == BTN_A) G.screen = SCR_MAIN;
        break;

    default:
        break;
    }
}

/* ================================================================
 * Controller management
 * ================================================================ */
void input_open_controller(int joystick_idx)
{
    if (G.ctrl) return; /* already have one */
    if (!SDL_IsGameController(joystick_idx)) return;
    G.ctrl = SDL_GameControllerOpen(joystick_idx);
    if (G.ctrl)
        fprintf(stderr, "[input] Opened controller: %s\n",
                SDL_GameControllerName(G.ctrl));
}

void input_close_controller(SDL_JoystickID instance_id)
{
    if (!G.ctrl) return;
    SDL_Joystick *j = SDL_GameControllerGetJoystick(G.ctrl);
    if (SDL_JoystickInstanceID(j) == instance_id) {
        SDL_GameControllerClose(G.ctrl);
        G.ctrl = NULL;
    }
}
