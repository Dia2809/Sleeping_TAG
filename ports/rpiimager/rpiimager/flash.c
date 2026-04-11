#include "flash.h"
#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define FLASH_TMP     "/tmp/rpiimager_dl.img"
#define WRITE_BUF_SZ  (1024 * 1024)   /* 1 MiB write chunks */

/* ----------------------------------------------------------------
 * Download progress → G.flash
 * ---------------------------------------------------------------- */
static void dl_progress(long long total, long long now, void *ud)
{
    (void)ud;
    FlashState *fs = &G.flash;
    pthread_mutex_lock(&fs->lock);
    fs->phase    = 0; /* downloading */
    fs->progress = (total > 0) ? (double)now / (double)total : 0.0;
    pthread_mutex_unlock(&fs->lock);
}

/* ----------------------------------------------------------------
 * Write a local file to a block device
 * ---------------------------------------------------------------- */
static int write_to_device(const char *src_path, const char *dev_path,
                            FlashState *fs)
{
    FILE *in = fopen(src_path, "rb");
    if (!in) {
        snprintf(fs->error_msg, sizeof(fs->error_msg),
                 "Cannot open source: %s", strerror(errno));
        return -1;
    }

    /* Get source size for progress */
    fseek(in, 0, SEEK_END);
    long long total = ftell(in);
    rewind(in);

    /* O_SYNC for safe unbuffered writes to a block device */
    int fd = open(dev_path, O_WRONLY | O_SYNC);
    if (fd < 0) {
        snprintf(fs->error_msg, sizeof(fs->error_msg),
                 "Cannot open device %s: %s", dev_path, strerror(errno));
        fclose(in);
        return -1;
    }

    char *buf = malloc(WRITE_BUF_SZ);
    if (!buf) {
        snprintf(fs->error_msg, sizeof(fs->error_msg), "Out of memory");
        close(fd);
        fclose(in);
        return -1;
    }

    pthread_mutex_lock(&fs->lock);
    fs->phase    = 1; /* writing */
    fs->progress = 0.0;
    pthread_mutex_unlock(&fs->lock);

    long long written = 0;
    size_t    n;
    while ((n = fread(buf, 1, WRITE_BUF_SZ, in)) > 0) {
        char   *p   = buf;
        size_t  rem = n;
        while (rem > 0) {
            ssize_t w = write(fd, p, rem);
            if (w < 0) {
                snprintf(fs->error_msg, sizeof(fs->error_msg),
                         "Write error at byte %lld: %s", written, strerror(errno));
                free(buf); close(fd); fclose(in);
                return -1;
            }
            p       += w;
            rem     -= (size_t)w;
            written += w;
        }

        if (total > 0) {
            pthread_mutex_lock(&fs->lock);
            fs->progress = (double)written / (double)total;
            pthread_mutex_unlock(&fs->lock);
        }
    }

    fsync(fd);
    free(buf);
    close(fd);
    fclose(in);
    return 0;
}

/* ================================================================
 * Thread entry-point
 * ================================================================ */
void *flash_thread_fn(void *arg)
{
    (void)arg;

    FlashState  *fs      = &G.flash;
    const char  *dev     = G.devices[G.dev_sel].path;
    const char  *src_url = G.image_url;
    int          local   = G.image_is_local;

    const char *src_file = local ? src_url : FLASH_TMP;

    /* Step 1: download (skip if source is already a local file) */
    if (!local) {
        char errbuf[256] = {0};
        int rc = net_fetch_file(src_url, FLASH_TMP,
                                dl_progress, NULL,
                                errbuf, sizeof(errbuf));
        if (rc != 0) {
            pthread_mutex_lock(&fs->lock);
            snprintf(fs->error_msg, sizeof(fs->error_msg), "%s", errbuf);
            fs->error = 1;
            fs->done  = 1;
            pthread_mutex_unlock(&fs->lock);
            return NULL;
        }
    }

    /* Step 2: write to device */
    int rc = write_to_device(src_file, dev, fs);

    /* Clean up temp file */
    if (!local) unlink(FLASH_TMP);

    pthread_mutex_lock(&fs->lock);
    if (rc != 0) fs->error = 1;
    fs->progress = 1.0;
    fs->done     = 1;
    pthread_mutex_unlock(&fs->lock);

    return NULL;
}

/* ================================================================
 * Public helpers
 * ================================================================ */
void flash_start(void)
{
    FlashState *fs = &G.flash;
    fs->progress = 0.0;
    fs->phase    = 0;
    fs->done     = 0;
    fs->error    = 0;
    fs->error_msg[0] = '\0';
    pthread_create(&G.flash_thread, NULL, flash_thread_fn, NULL);
}

void flash_join(void)
{
    pthread_join(G.flash_thread, NULL);
}
