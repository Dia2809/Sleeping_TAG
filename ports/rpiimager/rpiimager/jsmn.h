/*
 * jsmn.h - Minimal JSON tokenizer (public domain)
 *
 * Based on the JSMN algorithm by Serge Zaitsev.
 * This single-header implementation tokenizes JSON into a flat token array.
 * Each token carries: type, start/end byte offsets, and child count (size).
 *
 * Usage:
 *   jsmn_parser p;
 *   jsmntok_t   toks[256];
 *   jsmn_init(&p);
 *   int n = jsmn_parse(&p, json_str, strlen(json_str), toks, 256);
 *   // n < 0  => error (JSMN_ERROR_*)
 *   // n >= 0 => number of tokens
 */

#ifndef JSMN_H_
#define JSMN_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Token types */
typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT    = 1,
    JSMN_ARRAY     = 2,
    JSMN_STRING    = 3,
    JSMN_PRIMITIVE = 4
} jsmntype_t;

/* Error codes (returned by jsmn_parse) */
enum {
    JSMN_ERROR_NOMEM = -1,  /* Not enough token slots          */
    JSMN_ERROR_INVAL = -2,  /* Invalid character in JSON       */
    JSMN_ERROR_PART  = -3   /* JSON string is not complete yet */
};

typedef struct {
    jsmntype_t type;
    int        start; /* Byte offset of first character  */
    int        end;   /* Byte offset after last character */
    int        size;  /* Number of direct children        */
} jsmntok_t;

typedef struct {
    unsigned int pos;       /* Current byte position in the source */
    unsigned int toknext;   /* Index of the next free token slot   */
    int          toksuper;  /* Index of the current parent token   */
} jsmn_parser;

/* Forward declarations */
static void jsmn_init(jsmn_parser *p);
static int  jsmn_parse(jsmn_parser *p, const char *js, size_t len,
                        jsmntok_t *toks, unsigned int num_toks);

/* ---------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------- */

static jsmntok_t *jsmn__alloc(jsmn_parser *p, jsmntok_t *toks, unsigned int n)
{
    if (p->toknext >= n)
        return NULL;
    jsmntok_t *t = &toks[p->toknext++];
    t->start = t->end = -1;
    t->size  = 0;
    return t;
}

/* Parse a JSON primitive (number, bool, null) starting at p->pos.
   Returns 0 on success or a negative error code. */
static int jsmn__primitive(jsmn_parser *p, const char *js, size_t len,
                            jsmntok_t *toks, unsigned int num)
{
    int start = (int)p->pos;
    for (; p->pos < len && js[p->pos] != '\0'; p->pos++) {
        char c = js[p->pos];
        if (c == '\t' || c == '\r' || c == '\n' || c == ' ' ||
            c == ','  || c == ']'  || c == '}')
            goto found;
        if ((unsigned char)c < 32) {
            p->pos = (unsigned)start;
            return JSMN_ERROR_INVAL;
        }
    }
found:
    if (!toks) { p->pos--; return 0; }
    jsmntok_t *t = jsmn__alloc(p, toks, num);
    if (!t) { p->pos = (unsigned)start; return JSMN_ERROR_NOMEM; }
    t->type  = JSMN_PRIMITIVE;
    t->start = start;
    t->end   = (int)p->pos;
    p->pos--;
    return 0;
}

/* Parse a JSON string (with opening quote at p->pos).
   Returns 0 on success or a negative error code. */
static int jsmn__string(jsmn_parser *p, const char *js, size_t len,
                         jsmntok_t *toks, unsigned int num)
{
    int start = (int)p->pos;
    p->pos++;
    for (; p->pos < len && js[p->pos] != '\0'; p->pos++) {
        char c = js[p->pos];
        if (c == '"') {
            if (!toks) return 0;
            jsmntok_t *t = jsmn__alloc(p, toks, num);
            if (!t) { p->pos = (unsigned)start; return JSMN_ERROR_NOMEM; }
            t->type  = JSMN_STRING;
            t->start = start + 1;   /* exclude the opening quote */
            t->end   = (int)p->pos; /* exclude the closing quote */
            return 0;
        }
        if (c == '\\' && p->pos + 1 < len) {
            p->pos++;
            switch (js[p->pos]) {
            case '"': case '/': case '\\':
            case 'b': case 'f': case 'r':
            case 'n': case 't':
                break;
            case 'u':
                p->pos++;
                for (int i = 0; i < 4 && p->pos < len; i++, p->pos++) {
                    char h = js[p->pos];
                    if (!((h >= '0' && h <= '9') ||
                          (h >= 'a' && h <= 'f') ||
                          (h >= 'A' && h <= 'F'))) {
                        p->pos = (unsigned)start;
                        return JSMN_ERROR_INVAL;
                    }
                }
                p->pos--;
                break;
            default:
                p->pos = (unsigned)start;
                return JSMN_ERROR_INVAL;
            }
        }
    }
    p->pos = (unsigned)start;
    return JSMN_ERROR_PART;
}

/* ---------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

static void jsmn_init(jsmn_parser *p)
{
    p->pos      = 0;
    p->toknext  = 0;
    p->toksuper = -1;
}

static int jsmn_parse(jsmn_parser *p, const char *js, size_t len,
                       jsmntok_t *toks, unsigned int num)
{
    int r;
    int count = (int)p->toknext;

    for (; p->pos < len && js[p->pos] != '\0'; p->pos++) {
        char       c = js[p->pos];
        jsmntok_t *t;

        switch (c) {
        /* ---- Open brace / bracket ---- */
        case '{': case '[':
            count++;
            if (!toks) break;
            t = jsmn__alloc(p, toks, num);
            if (!t) return JSMN_ERROR_NOMEM;
            if (p->toksuper != -1)
                toks[p->toksuper].size++;
            t->type     = (c == '{') ? JSMN_OBJECT : JSMN_ARRAY;
            t->start    = (int)p->pos;
            p->toksuper = (int)(p->toknext - 1);
            break;

        /* ---- Close brace / bracket ---- */
        case '}': case ']':
            if (!toks) break;
            {
                jsmntype_t want = (c == '}') ? JSMN_OBJECT : JSMN_ARRAY;
                /* Close the innermost open container of the right type */
                int i;
                for (i = (int)p->toknext - 1; i >= 0; i--) {
                    t = &toks[i];
                    if (t->start != -1 && t->end == -1) {
                        if (t->type != want) return JSMN_ERROR_INVAL;
                        t->end      = (int)p->pos + 1;
                        p->toksuper = -1;
                        break;
                    }
                }
                if (i < 0) return JSMN_ERROR_INVAL;
                /* Find the new toksuper (nearest still-open container) */
                for (i--; i >= 0; i--) {
                    t = &toks[i];
                    if (t->start != -1 && t->end == -1) {
                        p->toksuper = i;
                        break;
                    }
                }
            }
            break;

        /* ---- String ---- */
        case '"':
            r = jsmn__string(p, js, len, toks, num);
            if (r < 0) return r;
            count++;
            if (p->toksuper != -1 && toks)
                toks[p->toksuper].size++;
            break;

        /* ---- Whitespace ---- */
        case '\t': case '\r': case '\n': case ' ':
            break;

        /* ---- Key-value separator ---- */
        case ':':
            if (toks) p->toksuper = (int)p->toknext - 1;
            break;

        /* ---- Item separator ---- */
        case ',':
            if (toks && p->toksuper != -1 &&
                toks[p->toksuper].type != JSMN_ARRAY &&
                toks[p->toksuper].type != JSMN_OBJECT) {
                /* Pop back to the enclosing container */
                for (int i = (int)p->toknext - 1; i >= 0; i--) {
                    if ((toks[i].type == JSMN_ARRAY ||
                         toks[i].type == JSMN_OBJECT) &&
                        toks[i].start != -1 && toks[i].end == -1) {
                        p->toksuper = i;
                        break;
                    }
                }
            }
            break;

        /* ---- Primitive value ---- */
        default:
            r = jsmn__primitive(p, js, len, toks, num);
            if (r < 0) return r;
            count++;
            if (p->toksuper != -1 && toks)
                toks[p->toksuper].size++;
            break;
        }
    }

    /* Verify all containers are closed */
    if (toks) {
        for (int i = (int)p->toknext - 1; i >= 0; i--) {
            if (toks[i].start != -1 && toks[i].end == -1)
                return JSMN_ERROR_PART;
        }
    }

    return count;
}

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H_ */
