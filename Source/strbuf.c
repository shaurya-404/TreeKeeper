/**
 * @file strbuf.c
 * @brief Growable string builder implementation (geometric growth).
 */
#include "strbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STRBUF_INITIAL_CAPACITY 256u

struct StrBuf {
    char  *data;     /* NUL-terminated contents          */
    size_t len;      /* bytes used, excluding the NUL    */
    size_t capacity; /* bytes allocated, including NUL   */
};

StrBuf *strbuf_create(void)
{
    StrBuf *sb = malloc(sizeof *sb);
    if (sb == NULL) {
        return NULL;
    }
    sb->data = malloc(STRBUF_INITIAL_CAPACITY);
    if (sb->data == NULL) {
        free(sb);
        return NULL;
    }
    sb->data[0] = '\0';
    sb->len = 0;
    sb->capacity = STRBUF_INITIAL_CAPACITY;
    return sb;
}

void strbuf_destroy(StrBuf *sb)
{
    if (sb == NULL) {
        return;
    }
    free(sb->data);
    free(sb);
}

/* Makes sure at least @p extra more bytes (plus the NUL) fit. */
static TkStatus strbuf_reserve(StrBuf *sb, size_t extra)
{
    if (extra > SIZE_MAX - sb->len - 1) {
        return TK_ERR_NOMEM;
    }
    size_t needed = sb->len + extra + 1;
    if (needed <= sb->capacity) {
        return TK_OK;
    }

    size_t new_cap = sb->capacity;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) {
            new_cap = needed;
            break;
        }
        new_cap *= 2;
    }
    char *grown = realloc(sb->data, new_cap);
    if (grown == NULL) {
        return TK_ERR_NOMEM;
    }
    sb->data = grown;
    sb->capacity = new_cap;
    return TK_OK;
}

TkStatus strbuf_append(StrBuf *sb, const char *text)
{
    if (sb == NULL || text == NULL) {
        return TK_ERR_INVALID;
    }
    size_t n = strlen(text);
    TkStatus st = strbuf_reserve(sb, n);
    if (st != TK_OK) {
        return st;
    }
    memcpy(sb->data + sb->len, text, n + 1);
    sb->len += n;
    return TK_OK;
}

TkStatus strbuf_printf(StrBuf *sb, const char *fmt, ...)
{
    if (sb == NULL || fmt == NULL) {
        return TK_ERR_INVALID;
    }

    /* First pass: measure the formatted length. */
    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (needed < 0) {
        va_end(args);
        return TK_ERR_INVALID;
    }

    TkStatus st = strbuf_reserve(sb, (size_t)needed);
    if (st == TK_OK) {
        /* Second pass: format directly into the reserved space. */
        vsnprintf(sb->data + sb->len, sb->capacity - sb->len, fmt, args);
        sb->len += (size_t)needed;
    }
    va_end(args);
    return st;
}

const char *strbuf_cstr(const StrBuf *sb)
{
    return (sb == NULL) ? "" : sb->data;
}

size_t strbuf_len(const StrBuf *sb)
{
    return (sb == NULL) ? 0 : sb->len;
}

void strbuf_clear(StrBuf *sb)
{
    if (sb == NULL) {
        return;
    }
    sb->len = 0;
    sb->data[0] = '\0';
}
