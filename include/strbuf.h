/**
 * @file strbuf.h
 * @brief Growable string builder (opaque type).
 *
 * The CLI composes each screen into a StrBuf and writes it to the terminal
 * in one go, which removes flicker when the dashboard refreshes.
 */
#ifndef STRBUF_H
#define STRBUF_H

#include <stddef.h>

#include "tk_common.h"

typedef struct StrBuf StrBuf;

/** Creates an empty buffer. Returns NULL on allocation failure. */
StrBuf *strbuf_create(void);

/** Releases the buffer. Safe to call with NULL. */
void strbuf_destroy(StrBuf *sb);

/** Appends a NUL-terminated string. */
TkStatus strbuf_append(StrBuf *sb, const char *text);

/** Appends printf-formatted text, growing the buffer as required. */
TkStatus strbuf_printf(StrBuf *sb, const char *fmt, ...) TK_PRINTF(2, 3);

/** Returns the current contents (always NUL-terminated, never NULL). */
const char *strbuf_cstr(const StrBuf *sb);

/** Returns the number of bytes currently stored (excluding the NUL). */
size_t strbuf_len(const StrBuf *sb);

/** Empties the buffer but keeps its allocation for reuse. */
void strbuf_clear(StrBuf *sb);

#endif /* STRBUF_H */
