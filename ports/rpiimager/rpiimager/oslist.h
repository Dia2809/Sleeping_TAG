#ifndef OSLIST_H_
#define OSLIST_H_

#include "types.h"

/*
 * oslist_load_sources
 *
 * Parse the local `oses.json` file.  Fills G.sources / G.source_count.
 * Returns the number of sources loaded (>= 0), or -1 on hard error.
 */
int oslist_load_sources(const char *json_path);

/*
 * oslist_fetch_all  (call from a worker thread)
 *
 * For every source in G.sources, fetch the remote catalog URL, parse it,
 * and append the resulting OS entries to G.oses / G.os_count.
 *
 * Two catalog formats are supported:
 *
 *   "rpi"     – Raspberry Pi Imager v4 JSON
 *               { "os_list": [ { "name":…, "url":…, "subitems":[…] } ] }
 *
 *   "generic" – Our own simple JSON
 *               { "os_list": [ { "name":…, "url":…, "description":…,
 *                                "download_size":…, "sha256":… } ] }
 *
 * On completion the caller should set G.fetch.done = 1 (and .error if needed).
 */
void oslist_fetch_all(void);

/*
 * Thread entry-point wrapper – pass to pthread_create.
 * arg is ignored; returns NULL.
 */
void *oslist_fetch_thread(void *arg);

#endif /* OSLIST_H_ */
