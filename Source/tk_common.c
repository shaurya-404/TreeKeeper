/**
 * @file tk_common.c
 * @brief Implementation of the shared helper routines in tk_common.h.
 */
#include "tk_common.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

const char *tk_strerror(TkStatus status)
{
    switch (status) {
    case TK_OK:              return "success";
    case TK_ERR_NOMEM:       return "out of memory";
    case TK_ERR_INVALID:     return "invalid argument";
    case TK_ERR_NOT_FOUND:   return "not found";
    case TK_ERR_EXISTS:      return "already exists";
    case TK_ERR_EMPTY:       return "nothing to do (empty)";
    case TK_ERR_UNREACHABLE: return "destination unreachable";
    case TK_ERR_IO:          return "file I/O error";
    case TK_ERR_PARSE:       return "could not parse input";
    case TK_ERR_THREAD:      return "threading error";
    }
    return "unknown error";
}

void *tk_reallocarray(void *ptr, size_t count, size_t size)
{
    /* Guard against count * size wrapping around SIZE_MAX. */
    if (size != 0 && count > SIZE_MAX / size) {
        errno = ENOMEM;
        return NULL;
    }
    size_t bytes = count * size;
    return realloc(ptr, bytes == 0 ? 1 : bytes);
}

TkStatus tk_parse_long(const char *text, long min, long max, long *out)
{
    if (text == NULL || out == NULL || *text == '\0') {
        return TK_ERR_INVALID;
    }

    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE) {
        return TK_ERR_PARSE;
    }
    if (value < min || value > max) {
        return TK_ERR_INVALID;
    }
    *out = value;
    return TK_OK;
}

TkStatus tk_parse_double(const char *text, double min, double max, double *out)
{
    if (text == NULL || out == NULL || *text == '\0') {
        return TK_ERR_INVALID;
    }

    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (end == text || *end != '\0' || errno == ERANGE || !isfinite(value)) {
        return TK_ERR_PARSE;
    }
    if (value < min || value > max) {
        return TK_ERR_INVALID;
    }
    *out = value;
    return TK_OK;
}

TkStatus tk_validate_name(const char *name)
{
    if (name == NULL) {
        return TK_ERR_INVALID;
    }
    size_t len = strlen(name);
    if (len == 0 || len >= TK_NAME_MAX) {
        return TK_ERR_INVALID;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.') {
            return TK_ERR_INVALID;
        }
    }
    return TK_OK;
}
