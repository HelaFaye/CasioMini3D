#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RAM page cache for the launcher: fixed slots of one page each, an O(1)
 * ROM-page -> slot map, least-recently-used eviction, and pinning for pages
 * that must stay resident (the baseline and the current room).
 * No allocation: the caller provides all storage.
 */
#define CGLINK_CACHE_MAX_PAGES 4096u /* 16 MB of 4 KB pages */
#define CGLINK_CACHE_NONE 0xffffu

typedef struct
{
    uint16_t rom_page; /* CGLINK_CACHE_NONE when free */
    uint8_t pinned;
    uint32_t last_use;
} cglink_cache_slot;

typedef struct
{
    uint8_t *pages;            /* slot_count * page_size bytes */
    cglink_cache_slot *slots;  /* slot_count entries */
    uint16_t *slot_of;         /* CGLINK_CACHE_MAX_PAGES entries */
    uint32_t slot_count;
    uint32_t page_size;
    uint32_t clock;
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
} cglink_cache;

void cglink_cache_init(cglink_cache *cache, uint8_t *pages,
                       cglink_cache_slot *slots, uint16_t *slot_of,
                       uint32_t slot_count, uint32_t page_size);

/* The cached page, or NULL. Counts a hit or a miss and refreshes LRU. */
uint8_t *cglink_cache_get(cglink_cache *cache, uint16_t rom_page);
/* True if cached, without touching statistics or LRU order. */
bool cglink_cache_contains(const cglink_cache *cache, uint16_t rom_page);

/* A slot to fill with `rom_page`, evicting the least recently used unpinned
   page if needed. Returns NULL if every slot is pinned. The page counts as
   cached once this returns, so fill it before the next get. */
uint8_t *cglink_cache_insert(cglink_cache *cache, uint16_t rom_page);

void cglink_cache_pin(cglink_cache *cache, uint16_t rom_page, bool pinned);
void cglink_cache_unpin_all(cglink_cache *cache);
uint32_t cglink_cache_used(const cglink_cache *cache);

#ifdef __cplusplus
}
#endif
