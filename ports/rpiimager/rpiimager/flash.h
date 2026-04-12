#ifndef FLASH_H_
#define FLASH_H_

#include "types.h"

/*
 * flash_start
 *
 * Spawns the flash worker thread.
 * Must be called only when:
 *   G.image_url       is set  (URL or local path)
 *   G.image_is_local  is set  (0 = URL, 1 = local file)
 *   G.dev_sel         is valid inside G.devices[]
 *
 * The worker sets G.flash.done when finished (and G.flash.error on failure).
 * Caller should join with flash_join() after done is set.
 */
void flash_start(void);

/*
 * flash_join – pthread_join wrapper; call once G.flash.done == 1.
 */
void flash_join(void);

/*
 * flash_thread_fn – the actual pthread entry point (exported so it can be
 * passed to pthread_create; do not call directly).
 */
void *flash_thread_fn(void *arg);

#endif /* FLASH_H_ */
