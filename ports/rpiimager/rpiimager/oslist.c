#include "oslist.h"
#include "json.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Internal: safely append one OSEntry if there is space
 * ---------------------------------------------------------------- */
static void append_os(OSEntry *e)
{
    pthread_mutex_lock(&G.fetch.lock);
    if (G.os_count < MAX_OSES) {
        G.oses[G.os_count++] = *e;
    }
    pthread_mutex_unlock(&G.fetch.lock);
}

/* ----------------------------------------------------------------
 * oslist_load_sources
 *
 * Parses the top-level oses.json:
 *   { "sources": [ { "id":…, "name":…, "catalog_url":…, "format":… } ] }
 * ---------------------------------------------------------------- */
int oslist_load_sources(const char *json_path)
{
    size_t len;
    char *js = json_load_file(json_path, &len);
    if (!js) return -1;

    jsmntok_t *toks = NULL;
    int n = json_parse(js, len, &toks);
    if (n < 0) { free(js); return -1; }

    G.source_count = 0;

    /* Walk tokens looking for the "sources" array */
    for (int i = 0; i < n - 1; i++) {
        if (jsoneq(js, &toks[i], "sources") != 0) continue;
        if (toks[i + 1].type != JSMN_ARRAY)       continue;

        int arr_size = toks[i + 1].size;
        int j        = i + 2;

        for (int k = 0; k < arr_size && G.source_count < MAX_SOURCES; k++) {
            if (toks[j].type != JSMN_OBJECT) { j++; continue; }

            int obj_size = toks[j].size;
            j++;

            OSSource *s = &G.sources[G.source_count];
            memset(s, 0, sizeof(*s));

            for (int m = 0; m < obj_size; m++) {
                if      (jsoneq(js, &toks[j], "id")          == 0)
                    json_str(js, &toks[j + 1], s->id,          sizeof(s->id));
                else if (jsoneq(js, &toks[j], "name")        == 0)
                    json_str(js, &toks[j + 1], s->name,        sizeof(s->name));
                else if (jsoneq(js, &toks[j], "catalog_url") == 0)
                    json_str(js, &toks[j + 1], s->catalog_url, sizeof(s->catalog_url));
                else if (jsoneq(js, &toks[j], "format")      == 0)
                    json_str(js, &toks[j + 1], s->format,      sizeof(s->format));
                j += 2;
            }

            if (s->catalog_url[0]) G.source_count++;
        }
        break;
    }

    free(toks);
    free(js);
    return G.source_count;
}

/* ================================================================
 * RPi Imager v4 catalog parser
 *
 * Format (simplified):
 * {
 *   "os_list": [
 *     {
 *       "name": "Raspberry Pi OS",
 *       "subitems": [
 *         {
 *           "name": "Raspberry Pi OS (64-bit)",
 *           "description": "…",
 *           "url": "https://…",
 *           "image_download_sha256": "abc…",
 *           "image_download_size": 1234567890,
 *           "extract_size": 9876543210
 *         }
 *       ]
 *     },
 *     {
 *       "name": "Raspberry Pi OS Lite (64-bit)",
 *       "url": "https://…",
 *       …
 *     }
 *   ]
 * }
 *
 * Categories (objects without "url" but with "subitems") are not
 * added themselves; we recurse into their subitems.
 * ================================================================ */

/*
 * Parse one object token (index `obj_idx`) as a potential OS entry.
 * If it has "subitems" we recurse; if it has "url" we emit an OSEntry.
 * Returns the index of the token AFTER this object (so the caller can
 * continue iterating the parent array).
 */
static int rpi_parse_object(const char *js, const jsmntok_t *toks, int n,
                             int obj_idx, int depth,
                             const char *source_name);

static int rpi_parse_array(const char *js, const jsmntok_t *toks, int n,
                            int arr_idx, int depth,
                            const char *source_name)
{
    int count = toks[arr_idx].size;
    int i     = arr_idx + 1;
    for (int k = 0; k < count && i < n; k++) {
        if (toks[i].type == JSMN_OBJECT)
            i = rpi_parse_object(js, toks, n, i, depth, source_name);
        else
            i++; /* skip unexpected token */
    }
    return i;
}

static int rpi_parse_object(const char *js, const jsmntok_t *toks, int n,
                             int obj_idx, int depth,
                             const char *source_name)
{
    int obj_size = toks[obj_idx].size;
    int i        = obj_idx + 1;

    OSEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.source_name, source_name, sizeof(entry.source_name) - 1);

    /* First pass: collect fields */
    int  subitems_idx = -1;
    for (int m = 0; m < obj_size && i < n; m++) {
        const jsmntok_t *key = &toks[i];
        const jsmntok_t *val = &toks[i + 1];

        if (jsoneq(js, key, "name") == 0) {
            json_str(js, val, entry.name, sizeof(entry.name));
            i += 2;
        } else if (jsoneq(js, key, "description") == 0) {
            json_str(js, val, entry.description, sizeof(entry.description));
            i += 2;
        } else if (jsoneq(js, key, "url") == 0) {
            json_str(js, val, entry.url, sizeof(entry.url));
            i += 2;
        } else if (jsoneq(js, key, "image_download_sha256") == 0) {
            json_str(js, val, entry.sha256, sizeof(entry.sha256));
            i += 2;
        } else if (jsoneq(js, key, "image_download_size") == 0) {
            entry.download_size = json_ll(js, val);
            i += 2;
        } else if (jsoneq(js, key, "extract_size") == 0) {
            entry.extract_size = json_ll(js, val);
            i += 2;
        } else if (jsoneq(js, key, "subitems") == 0) {
            /* Remember the subitems array; skip it for now */
            subitems_idx = i + 1;
            /* Skip past the array and all its children */
            i += 2;
            if (subitems_idx < n && toks[subitems_idx].type == JSMN_ARRAY)
                i = rpi_parse_array(js, toks, n, subitems_idx, depth + 1, source_name);
        } else {
            /* Unknown key → skip key + value (including any nested tokens) */
            i++;
            if (i < n) {
                /* Skip nested object/array by walking past its children */
                int skip_size = toks[i].size;
                i++;
                for (int s = 0; s < skip_size && i < n; s++) {
                    if (toks[i].type == JSMN_OBJECT || toks[i].type == JSMN_ARRAY) {
                        i = rpi_parse_array(js, toks, n, i, depth + 1, source_name);
                    } else {
                        i++;
                    }
                }
            }
        }

        (void)depth; /* suppress unused-variable warning when recursion unused */
    }

    /* Emit a leaf entry only if it has both a name and a download URL */
    if (entry.name[0] && entry.url[0])
        append_os(&entry);

    return i;
}

static void parse_rpi_catalog(const char *js, size_t len,
                               const char *source_name)
{
    jsmntok_t *toks = NULL;
    int n = json_parse(js, len, &toks);
    if (n < 0) return;

    for (int i = 0; i < n - 1; i++) {
        if (jsoneq(js, &toks[i], "os_list") != 0)  continue;
        if (toks[i + 1].type != JSMN_ARRAY)         continue;
        rpi_parse_array(js, toks, n, i + 1, 0, source_name);
        break;
    }
    free(toks);
}

/* ================================================================
 * Generic catalog parser
 *
 * { "os_list": [
 *     {
 *       "name":          "…",
 *       "description":   "…",
 *       "url":           "https://…",
 *       "download_size": 1234567890,
 *       "sha256":        "abc…"
 *     }
 *   ]
 * }
 * ================================================================ */
static void parse_generic_catalog(const char *js, size_t len,
                                   const char *source_name)
{
    jsmntok_t *toks = NULL;
    int n = json_parse(js, len, &toks);
    if (n < 0) return;

    for (int i = 0; i < n - 1; i++) {
        if (jsoneq(js, &toks[i], "os_list") != 0) continue;
        if (toks[i + 1].type != JSMN_ARRAY)        continue;

        int arr_size = toks[i + 1].size;
        int j        = i + 2;

        for (int k = 0; k < arr_size && j < n; k++) {
            if (toks[j].type != JSMN_OBJECT) { j++; continue; }

            int obj_size = toks[j].size;
            j++;

            OSEntry e;
            memset(&e, 0, sizeof(e));
            strncpy(e.source_name, source_name, sizeof(e.source_name) - 1);

            for (int m = 0; m < obj_size && j < n; m++) {
                if      (jsoneq(js, &toks[j], "name")          == 0)
                    json_str(js, &toks[j + 1], e.name,          sizeof(e.name));
                else if (jsoneq(js, &toks[j], "description")   == 0)
                    json_str(js, &toks[j + 1], e.description,   sizeof(e.description));
                else if (jsoneq(js, &toks[j], "url")           == 0)
                    json_str(js, &toks[j + 1], e.url,           sizeof(e.url));
                else if (jsoneq(js, &toks[j], "sha256")        == 0)
                    json_str(js, &toks[j + 1], e.sha256,        sizeof(e.sha256));
                else if (jsoneq(js, &toks[j], "download_size") == 0)
                    e.download_size = json_ll(js, &toks[j + 1]);
                j += 2;
            }

            if (e.name[0] && e.url[0])
                append_os(&e);
        }
        break;
    }
    free(toks);
}

/* ================================================================
 * oslist_fetch_all  – called from the fetch worker thread
 * ================================================================ */
void oslist_fetch_all(void)
{
    for (int s = 0; s < G.source_count; s++) {
        OSSource *src = &G.sources[s];

        /* Update status message (best-effort, no lock needed for display) */
        snprintf(G.status_msg, sizeof(G.status_msg),
                 "Fetching: %s…", src->name);

        char *buf  = NULL;
        size_t len = 0;

        if (net_fetch_buffer(src->catalog_url, &buf, &len) != 0) {
            /* Non-fatal: just skip this source */
            fprintf(stderr, "[oslist] Failed to fetch %s\n", src->catalog_url);
            continue;
        }

        if (strcmp(src->format, "rpi") == 0)
            parse_rpi_catalog(buf, len, src->name);
        else
            parse_generic_catalog(buf, len, src->name);

        free(buf);
    }
}

/* ================================================================
 * Thread entry-point
 * ================================================================ */
void *oslist_fetch_thread(void *arg)
{
    (void)arg;

    oslist_fetch_all();

    pthread_mutex_lock(&G.fetch.lock);
    G.fetch.done = 1;
    pthread_mutex_unlock(&G.fetch.lock);

    return NULL;
}
