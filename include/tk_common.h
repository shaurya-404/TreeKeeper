/**
 * @file tk_common.h
 * @brief Shared status codes, limits and defensive helper routines.
 *
 * Every module in TreeKeeper reports failure through a TkStatus value
 * instead of printing or exiting, so the caller (usually the CLI) decides
 * how to react. This keeps the data-structure modules reusable and testable.
 */
#ifndef TK_COMMON_H
#define TK_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Maximum length of a zone or volunteer name, including the NUL byte. */
#define TK_NAME_MAX 32

/** Maximum length of a file-system path built by the program. */
#define TK_PATH_MAX 1024

/** Lets the compiler check printf-style format strings in our own APIs. */
#if defined(__GNUC__) || defined(__clang__)
#define TK_PRINTF(fmt_idx, args_idx) __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define TK_PRINTF(fmt_idx, args_idx)
#endif

#define TK_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

/** Result codes returned by every fallible function in the project. */
typedef enum {
    TK_OK = 0,          /**< Operation succeeded.                         */
    TK_ERR_NOMEM,       /**< Memory allocation failed.                    */
    TK_ERR_INVALID,     /**< Bad argument (NULL pointer, out of range).   */
    TK_ERR_NOT_FOUND,   /**< Requested key / id does not exist.           */
    TK_ERR_EXISTS,      /**< Key / name already present.                  */
    TK_ERR_EMPTY,       /**< Container is empty.                          */
    TK_ERR_UNREACHABLE, /**< No path exists between two graph vertices.   */
    TK_ERR_IO,          /**< File-system read/write failure.              */
    TK_ERR_PARSE,       /**< Text could not be parsed.                    */
    TK_ERR_THREAD       /**< pthread primitive failed.                    */
} TkStatus;

/** Returns a human-readable, static description of a status code. */
const char *tk_strerror(TkStatus status);

/**
 * realloc() for arrays that refuses to overflow (count * size).
 * Returns NULL on overflow or allocation failure; @p ptr stays valid then.
 */
void *tk_reallocarray(void *ptr, size_t count, size_t size);

/**
 * Parses a base-10 integer that must fill the whole string and lie in
 * [min, max]. Returns TK_ERR_PARSE for malformed text, TK_ERR_INVALID
 * when the value is out of range.
 */
TkStatus tk_parse_long(const char *text, long min, long max, long *out);

/** Same contract as tk_parse_long() for finite floating-point values. */
TkStatus tk_parse_double(const char *text, double min, double max, double *out);

/**
 * Checks that @p name is 1..TK_NAME_MAX-1 characters drawn from
 * [A-Za-z0-9_-.]. Such names are safe to store in CSV without quoting
 * and to type as a single CLI token.
 */
TkStatus tk_validate_name(const char *name);

#endif /* TK_COMMON_H */
