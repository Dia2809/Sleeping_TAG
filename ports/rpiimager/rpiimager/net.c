#include "net.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Growing write-to-memory buffer used by libcurl
 * ---------------------------------------------------------------- */
typedef struct {
    char  *data;
    size_t size;
} MemBuf;

static size_t mem_write_cb(void *ptr, size_t sz, size_t nmemb, void *ud)
{
    size_t   bytes = sz * nmemb;
    MemBuf  *buf   = (MemBuf *)ud;

    char *tmp = realloc(buf->data, buf->size + bytes + 1);
    if (!tmp) return 0; /* signals error to curl */

    buf->data = tmp;
    memcpy(buf->data + buf->size, ptr, bytes);
    buf->size += bytes;
    buf->data[buf->size] = '\0';
    return bytes;
}

/* ----------------------------------------------------------------
 * Write-to-file callback
 * ---------------------------------------------------------------- */
static size_t file_write_cb(void *ptr, size_t sz, size_t nmemb, void *ud)
{
    return fwrite(ptr, sz, nmemb, (FILE *)ud);
}

/* ----------------------------------------------------------------
 * Progress callback shim
 * ---------------------------------------------------------------- */
typedef struct {
    net_progress_fn fn;
    void           *userdata;
} ProgressShim;

static int xfer_cb(void *ud,
                   curl_off_t dltotal, curl_off_t dlnow,
                   curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    ProgressShim *s = (ProgressShim *)ud;
    if (s->fn)
        s->fn((long long)dltotal, (long long)dlnow, s->userdata);
    return 0; /* non-zero would abort the transfer */
}

/* ----------------------------------------------------------------
 * Shared curl setup
 * ---------------------------------------------------------------- */
static CURL *new_curl(const char *url)
{
    CURL *c = curl_easy_init();
    if (!c) return NULL;

    curl_easy_setopt(c, CURLOPT_URL,            url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS,      10L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT,        0L);    /* no overall timeout */
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT,
                     "rpiimager-port/1.0 libcurl");
    return c;
}

/* ----------------------------------------------------------------
 * net_fetch_buffer
 * ---------------------------------------------------------------- */
int net_fetch_buffer(const char *url, char **out_buf, size_t *out_len)
{
    CURL *c = new_curl(url);
    if (!c) return -1;

    MemBuf buf = {NULL, 0};
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, mem_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,     &buf);

    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        free(buf.data);
        return -1;
    }

    *out_buf = buf.data;
    *out_len = buf.size;
    return 0;
}

/* ----------------------------------------------------------------
 * net_fetch_file
 * ---------------------------------------------------------------- */
int net_fetch_file(const char *url,
                   const char *dest_path,
                   net_progress_fn progress_fn,
                   void *userdata,
                   char *err_buf, size_t err_buf_size)
{
    CURL *c = new_curl(url);
    if (!c) {
        if (err_buf) snprintf(err_buf, err_buf_size, "curl_easy_init() failed");
        return -1;
    }

    FILE *fp = fopen(dest_path, "wb");
    if (!fp) {
        curl_easy_cleanup(c);
        if (err_buf) snprintf(err_buf, err_buf_size,
                              "Cannot create file: %s", dest_path);
        return -1;
    }

    ProgressShim shim = {progress_fn, userdata};

    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION,    file_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA,        fp);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xfer_cb);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA,     &shim);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS,       0L);

    CURLcode rc = curl_easy_perform(c);
    fclose(fp);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK) {
        if (err_buf) snprintf(err_buf, err_buf_size,
                              "Download failed: %s", curl_easy_strerror(rc));
        return -1;
    }
    return 0;
}
