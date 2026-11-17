#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Raw DEFLATE (RFC 1951) decoder for the fx-CG50 launcher: no allocation,
 * no floating point, about 2.5 KB of state on the stack. Decodes one complete
 * stream into a caller buffer.
 *
 * Returns the number of bytes written, or a negative cglink_inflate_error.
 */
enum cglink_inflate_error
{
    CGLINK_INFLATE_TRUNCATED = -1,  /* input ended inside the stream */
    CGLINK_INFLATE_BAD_DATA = -2,   /* invalid block type, code or distance */
    CGLINK_INFLATE_TOO_LONG = -3    /* output would exceed out_capacity */
};

long cglink_inflate(const uint8_t *in, size_t in_size,
                    uint8_t *out, size_t out_capacity);

#ifdef __cplusplus
}
#endif
