#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * json_load_file
 * ---------------------------------------------------------------- */
char *json_load_file(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz <= 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

/* ----------------------------------------------------------------
 * json_parse
 * ---------------------------------------------------------------- */
int json_parse(const char *js, size_t len, jsmntok_t **out_toks)
{
    jsmntok_t *toks = malloc(sizeof(jsmntok_t) * JSON_MAX_TOKENS);
    if (!toks) return JSMN_ERROR_NOMEM;

    jsmn_parser p;
    jsmn_init(&p);
    int n = jsmn_parse(&p, js, len, toks, JSON_MAX_TOKENS);
    if (n < 0) { free(toks); return n; }

    *out_toks = toks;
    return n;
}

/* ----------------------------------------------------------------
 * jsoneq – case-sensitive key comparison
 * ---------------------------------------------------------------- */
int jsoneq(const char *js, const jsmntok_t *tok, const char *s)
{
    int tlen = tok->end - tok->start;
    if (tok->type != JSMN_STRING) return -1;
    if ((int)strlen(s) != tlen)   return -1;
    return strncmp(js + tok->start, s, (size_t)tlen) == 0 ? 0 : -1;
}

/* ----------------------------------------------------------------
 * json_str
 * ---------------------------------------------------------------- */
void json_str(const char *js, const jsmntok_t *tok, char *buf, int buflen)
{
    int len = tok->end - tok->start;
    if (len >= buflen) len = buflen - 1;
    memcpy(buf, js + tok->start, (size_t)len);
    buf[len] = '\0';
}

/* ----------------------------------------------------------------
 * json_ll
 * ---------------------------------------------------------------- */
long long json_ll(const char *js, const jsmntok_t *tok)
{
    char buf[32];
    json_str(js, tok, buf, sizeof(buf));
    return atoll(buf);
}
