#ifndef NET_H_
#define NET_H_

#include <stddef.h>

/*
 * Progress callback type.
 * total    – expected total bytes (may be 0 if server doesn't send Content-Length)
 * now      – bytes transferred so far
 * userdata – pointer passed to net_fetch_file()
 */
typedef void (*net_progress_fn)(long long total, long long now, void *userdata);

/*
 * net_fetch_buffer
 *
 * Download `url` into a malloc'd buffer.
 * Caller must free() *out_buf.
 * Returns 0 on success, -1 on error.
 * *out_len is set to the buffer size (no NUL terminator counted, but one is
 * appended so the buffer is safe to use as a C string).
 */
int net_fetch_buffer(const char *url,
                     char **out_buf, size_t *out_len);

/*
 * net_fetch_file
 *
 * Download `url` and write it to `dest_path`.
 * `progress_fn` is called periodically with byte counts (may be NULL).
 * Returns 0 on success, -1 on error.
 * On error, `err_buf` (size `err_buf_size`) receives a human-readable message.
 */
int net_fetch_file(const char *url,
                   const char *dest_path,
                   net_progress_fn progress_fn,
                   void *userdata,
                   char *err_buf, size_t err_buf_size);

#endif /* NET_H_ */
