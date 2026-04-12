#ifndef JSON_H_
#define JSON_H_

#include <stddef.h>
#include "jsmn.h"

/* Maximum tokens we allocate per parse call */
#define JSON_MAX_TOKENS 4096

/*
 * json_load_file – read a file into a malloc'd buffer.
 * Caller must free() the returned pointer.
 * Returns NULL on error.  *out_len is set to the buffer size (excl. NUL).
 */
char *json_load_file(const char *path, size_t *out_len);

/*
 * json_parse – tokenise `js` (length `len`) into a freshly malloc'd
 * jsmntok_t array.  Caller must free() *out_toks.
 * Returns token count (>= 1) or a negative JSMN_ERROR_* code.
 */
int json_parse(const char *js, size_t len,
               jsmntok_t **out_toks);

/*
 * jsoneq – return 0 if token tok in string js equals the C string s.
 */
int jsoneq(const char *js, const jsmntok_t *tok, const char *s);

/*
 * json_str – copy the raw text of token tok from js into buf (NUL-terminated,
 * truncated to buflen-1 bytes).
 */
void json_str(const char *js, const jsmntok_t *tok,
              char *buf, int buflen);

/*
 * json_long – convert the text of a PRIMITIVE token to long long.
 */
long long json_ll(const char *js, const jsmntok_t *tok);

#endif /* JSON_H_ */
