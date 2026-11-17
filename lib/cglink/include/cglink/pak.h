#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reader for the page pak built by HelaFaye/tmc's
 * scripts/fxcg/build_page_pak.py ("PCPK" v1, little-endian): every page an
 * application needs, compressed on its own, with a resident baseline and a
 * manifest per room. The index is small (a few KB) and is kept in RAM; page
 * data stays on the SD card and is fetched through the link.
 */
#define CGLINK_PAK_CODEC_DEFLATE 1u

typedef struct
{
    uint16_t rom_page;
    uint16_t stored_size; /* == page size: stored uncompressed */
    uint32_t data_offset; /* from the start of the data blob */
} cglink_pak_page;

typedef struct
{
    const uint8_t *index; /* the pak bytes up to the data blob */
    size_t index_size;
    uint8_t codec;
    uint8_t page_shift;
    uint32_t rom_size;
    uint32_t page_count;
    uint32_t room_count;
    uint32_t baseline_count;
    uint32_t manifest_entries;
    size_t data_start; /* offset of the data blob in the pak */
} cglink_pak;

/* Parse the header from the first bytes of a pak; `size` may cover just the
   index. Returns false for a malformed or truncated index. */
bool cglink_pak_open(cglink_pak *pak, const uint8_t *bytes, size_t size);

cglink_pak_page cglink_pak_page_at(const cglink_pak *pak, uint32_t index);
/* Index of the stored page for a ROM page, or -1. */
long cglink_pak_find_page(const cglink_pak *pak, uint16_t rom_page);
uint16_t cglink_pak_baseline(const cglink_pak *pak, uint32_t i);

/* A room's manifest: its pages (indices into the page table) beyond the
   baseline. Returns false when the room has no manifest. */
bool cglink_pak_room(const cglink_pak *pak, uint8_t area, uint8_t room,
                     uint32_t *first, uint32_t *count);
uint16_t cglink_pak_manifest(const cglink_pak *pak, uint32_t i);

#ifdef __cplusplus
}
#endif
