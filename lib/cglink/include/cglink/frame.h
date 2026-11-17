#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Byte-stream framing for the calculator <-> ESP32 link (2.5 mm serial):
 * SLIP (RFC 1055) around [payload][crc16 LE], where the CRC is
 * CRC-16/CCITT-FALSE over the payload -- the same conventions as
 * HelaFaye/gbagate's link_proto, with room for whole compressed pages.
 */
#define CGLINK_MAX_PAYLOAD 4352u
#define CGLINK_MAX_FRAMED ((CGLINK_MAX_PAYLOAD + 2u) * 2u + 2u)

uint16_t cglink_crc16(const uint8_t *data, size_t size);

/* Frame `size` payload bytes into `out`. Returns the framed length, or 0 if
   the payload is too long or `capacity` too small. */
size_t cglink_frame_encode(const uint8_t *payload, size_t size,
                           uint8_t *out, size_t capacity);

typedef struct
{
    uint8_t buffer[CGLINK_MAX_PAYLOAD + 2u];
    size_t length;
    bool escaped;
    bool overflow;
    uint32_t dropped; /* frames discarded for overflow or a bad CRC */
} cglink_rx;

void cglink_rx_init(cglink_rx *rx);

/* Feed one received byte. Returns the payload length when a complete,
   CRC-clean frame ends here (payload in rx->buffer), else -1. */
long cglink_rx_feed(cglink_rx *rx, uint8_t byte);

#ifdef __cplusplus
}
#endif
