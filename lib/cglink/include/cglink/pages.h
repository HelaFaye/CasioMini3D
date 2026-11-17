#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cglink/cache.h>
#include <cglink/pak.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Page streaming over the link. The client (launcher) sends a PAGE_REQUEST
 * listing pak page indices in priority order; it replaces any earlier
 * request. The server (ESP32) streams one PAGE_DATA message per page, in
 * order, reading compressed bytes from the pak on its SD card. Page data
 * from an older request is still accepted: it is valid, just no longer the
 * most urgent.
 *
 *   PAGE_REQUEST: u8 type=0x10, u8 generation, u16 count, u16 index[count]
 *   PAGE_DATA:    u8 type=0x11, u8 generation, u16 index, u16 stored_size,
 *                 u8 data[stored_size]
 * All little-endian; each message is one cglink frame.
 */
#define CGLINK_MSG_PAGE_REQUEST 0x10u
#define CGLINK_MSG_PAGE_DATA 0x11u
#define CGLINK_MAX_REQUEST_PAGES 1024u

/* ---- server (ESP32) ---- */

/* Reads `size` bytes at `offset` of the pak file; returns bytes read. */
typedef size_t (*cglink_read_fn)(void *context, uint32_t offset, uint8_t *out, size_t size);

typedef struct
{
    const cglink_pak *pak;
    cglink_read_fn read;
    void *read_context;
    uint8_t generation;
    uint16_t queue[CGLINK_MAX_REQUEST_PAGES];
    uint32_t queue_count;
    uint32_t queue_next;
    uint32_t pages_sent;
} cglink_page_server;

void cglink_page_server_init(cglink_page_server *server, const cglink_pak *pak,
                             cglink_read_fn read, void *read_context);
/* Handle one received payload. Returns false if it was not a valid request. */
bool cglink_page_server_receive(cglink_page_server *server,
                                const uint8_t *payload, size_t size);
/* Build the next PAGE_DATA payload into `out` (CGLINK_MAX_PAYLOAD bytes).
   Returns its size, or 0 when nothing is queued. */
size_t cglink_page_server_next(cglink_page_server *server, uint8_t *out);

/* ---- client (launcher) ---- */

typedef struct
{
    const cglink_pak *pak;
    cglink_cache *cache;
    uint8_t generation;
    uint32_t pages_received;
    uint32_t bad_pages;
} cglink_page_client;

void cglink_page_client_init(cglink_page_client *client, const cglink_pak *pak,
                             cglink_cache *cache);
/* Build a PAGE_REQUEST for the listed pak page indices, skipping pages
   already cached. Returns the payload size (0 if nothing is missing). */
size_t cglink_page_client_request(cglink_page_client *client,
                                  const uint16_t *indices, uint32_t count,
                                  uint8_t *out);
/* Handle a received payload: decompress PAGE_DATA into the cache.
   Returns true if a page was stored. */
bool cglink_page_client_receive(cglink_page_client *client,
                                const uint8_t *payload, size_t size);

#ifdef __cplusplus
}
#endif
