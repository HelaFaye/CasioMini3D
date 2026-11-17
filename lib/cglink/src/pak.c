#include <cglink/pak.h>

#include <string.h>

#define HEADER_SIZE 28u
#define PAGE_ENTRY 8u
#define ROOM_ENTRY 8u

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static size_t pages_at(const cglink_pak *pak) { (void)pak; return HEADER_SIZE; }
static size_t baseline_at(const cglink_pak *pak) { return pages_at(pak) + pak->page_count * PAGE_ENTRY; }
static size_t rooms_at(const cglink_pak *pak) { return baseline_at(pak) + pak->baseline_count * 2u; }
static size_t manifests_at(const cglink_pak *pak) { return rooms_at(pak) + pak->room_count * ROOM_ENTRY; }

bool cglink_pak_open(cglink_pak *pak, const uint8_t *bytes, size_t size)
{
    memset(pak, 0, sizeof(*pak));
    if (size < HEADER_SIZE || memcmp(bytes, "PCPK", 4) != 0 || le16(bytes + 4) != 1)
        return false;
    pak->codec = bytes[6];
    pak->page_shift = bytes[7];
    pak->rom_size = le32(bytes + 8);
    pak->page_count = le32(bytes + 12);
    pak->room_count = le32(bytes + 16);
    pak->baseline_count = le32(bytes + 20);
    pak->manifest_entries = le32(bytes + 24);
    if (pak->page_shift < 9 || pak->page_shift > 16 || pak->page_count > 0xffffu ||
        pak->baseline_count > pak->page_count || pak->room_count > 0x10000u ||
        pak->manifest_entries > 0x100000u)
        return false;
    pak->index = bytes;
    pak->data_start = manifests_at(pak) + pak->manifest_entries * 2u;
    pak->index_size = pak->data_start;
    return size >= pak->data_start;
}

cglink_pak_page cglink_pak_page_at(const cglink_pak *pak, uint32_t index)
{
    const uint8_t *entry = pak->index + pages_at(pak) + index * PAGE_ENTRY;
    cglink_pak_page page = {le16(entry), le16(entry + 2), le32(entry + 4)};
    return page;
}

long cglink_pak_find_page(const cglink_pak *pak, uint16_t rom_page)
{
    /* The page table is sorted by ROM page. */
    uint32_t low = 0, high = pak->page_count;
    while (low < high)
    {
        const uint32_t middle = low + (high - low) / 2u;
        const uint16_t value = le16(pak->index + pages_at(pak) + middle * PAGE_ENTRY);
        if (value == rom_page)
            return (long)middle;
        if (value < rom_page)
            low = middle + 1u;
        else
            high = middle;
    }
    return -1;
}

uint16_t cglink_pak_baseline(const cglink_pak *pak, uint32_t i)
{
    return le16(pak->index + baseline_at(pak) + i * 2u);
}

bool cglink_pak_room(const cglink_pak *pak, uint8_t area, uint8_t room,
                     uint32_t *first, uint32_t *count)
{
    for (uint32_t i = 0; i < pak->room_count; ++i)
    {
        const uint8_t *entry = pak->index + rooms_at(pak) + i * ROOM_ENTRY;
        if (entry[0] == area && entry[1] == room)
        {
            *count = le16(entry + 2);
            *first = le32(entry + 4);
            return *first + *count <= pak->manifest_entries;
        }
    }
    return false;
}

uint16_t cglink_pak_manifest(const cglink_pak *pak, uint32_t i)
{
    return le16(pak->index + manifests_at(pak) + i * 2u);
}
