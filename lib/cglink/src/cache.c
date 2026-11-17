#include <cglink/cache.h>

void cglink_cache_init(cglink_cache *cache, uint8_t *pages,
                       cglink_cache_slot *slots, uint16_t *slot_of,
                       uint32_t slot_count, uint32_t page_size)
{
    cache->pages = pages;
    cache->slots = slots;
    cache->slot_of = slot_of;
    cache->slot_count = slot_count;
    cache->page_size = page_size;
    cache->clock = 0;
    cache->hits = cache->misses = cache->evictions = 0;
    for (uint32_t i = 0; i < slot_count; ++i)
    {
        slots[i].rom_page = CGLINK_CACHE_NONE;
        slots[i].pinned = 0;
        slots[i].last_use = 0;
    }
    for (uint32_t i = 0; i < CGLINK_CACHE_MAX_PAGES; ++i)
        slot_of[i] = CGLINK_CACHE_NONE;
}

bool cglink_cache_contains(const cglink_cache *cache, uint16_t rom_page)
{
    return rom_page < CGLINK_CACHE_MAX_PAGES && cache->slot_of[rom_page] != CGLINK_CACHE_NONE;
}

uint8_t *cglink_cache_get(cglink_cache *cache, uint16_t rom_page)
{
    if (!cglink_cache_contains(cache, rom_page))
    {
        cache->misses++;
        return 0;
    }
    const uint16_t slot = cache->slot_of[rom_page];
    cache->slots[slot].last_use = ++cache->clock;
    cache->hits++;
    return cache->pages + (size_t)slot * cache->page_size;
}

uint8_t *cglink_cache_insert(cglink_cache *cache, uint16_t rom_page)
{
    if (rom_page >= CGLINK_CACHE_MAX_PAGES)
        return 0;
    uint16_t slot = cache->slot_of[rom_page];
    if (slot == CGLINK_CACHE_NONE)
    {
        uint32_t best = CGLINK_CACHE_NONE;
        for (uint32_t i = 0; i < cache->slot_count; ++i)
        {
            const cglink_cache_slot *candidate = &cache->slots[i];
            if (candidate->rom_page == CGLINK_CACHE_NONE)
            {
                best = i;
                break;
            }
            if (!candidate->pinned &&
                (best == CGLINK_CACHE_NONE || candidate->last_use < cache->slots[best].last_use))
                best = i;
        }
        if (best == CGLINK_CACHE_NONE)
            return 0;
        slot = (uint16_t)best;
        if (cache->slots[slot].rom_page != CGLINK_CACHE_NONE)
        {
            cache->slot_of[cache->slots[slot].rom_page] = CGLINK_CACHE_NONE;
            cache->evictions++;
        }
        cache->slots[slot].rom_page = rom_page;
        cache->slots[slot].pinned = 0;
        cache->slot_of[rom_page] = slot;
    }
    cache->slots[slot].last_use = ++cache->clock;
    return cache->pages + (size_t)slot * cache->page_size;
}

void cglink_cache_pin(cglink_cache *cache, uint16_t rom_page, bool pinned)
{
    if (cglink_cache_contains(cache, rom_page))
        cache->slots[cache->slot_of[rom_page]].pinned = pinned ? 1 : 0;
}

void cglink_cache_unpin_all(cglink_cache *cache)
{
    for (uint32_t i = 0; i < cache->slot_count; ++i)
        cache->slots[i].pinned = 0;
}

uint32_t cglink_cache_used(const cglink_cache *cache)
{
    uint32_t used = 0;
    for (uint32_t i = 0; i < cache->slot_count; ++i)
        used += cache->slots[i].rom_page != CGLINK_CACHE_NONE;
    return used;
}
