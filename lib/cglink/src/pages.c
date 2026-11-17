#include <cglink/pages.h>

#include <cglink/frame.h>
#include <cglink/inflate.h>

#include <string.h>

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void cglink_page_server_init(cglink_page_server *server, const cglink_pak *pak,
                             cglink_read_fn read, void *read_context)
{
    memset(server, 0, sizeof(*server));
    server->pak = pak;
    server->read = read;
    server->read_context = read_context;
}

bool cglink_page_server_receive(cglink_page_server *server,
                                const uint8_t *payload, size_t size)
{
    if (size < 4 || payload[0] != CGLINK_MSG_PAGE_REQUEST)
        return false;
    const uint32_t count = le16(payload + 2);
    if (count > CGLINK_MAX_REQUEST_PAGES || size != 4u + count * 2u)
        return false;
    server->generation = payload[1];
    server->queue_count = 0;
    server->queue_next = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint16_t index = le16(payload + 4 + i * 2u);
        if (index < server->pak->page_count)
            server->queue[server->queue_count++] = index;
    }
    return true;
}

size_t cglink_page_server_next(cglink_page_server *server, uint8_t *out)
{
    while (server->queue_next < server->queue_count)
    {
        const uint16_t index = server->queue[server->queue_next++];
        const cglink_pak_page page = cglink_pak_page_at(server->pak, index);
        if (page.stored_size > CGLINK_MAX_PAYLOAD - 6u)
            continue;
        out[0] = CGLINK_MSG_PAGE_DATA;
        out[1] = server->generation;
        put16(out + 2, index);
        put16(out + 4, page.stored_size);
        const uint32_t offset = (uint32_t)server->pak->data_start + page.data_offset;
        if (server->read(server->read_context, offset, out + 6, page.stored_size) != page.stored_size)
            continue;
        server->pages_sent++;
        return 6u + page.stored_size;
    }
    return 0;
}

void cglink_page_client_init(cglink_page_client *client, const cglink_pak *pak,
                             cglink_cache *cache)
{
    memset(client, 0, sizeof(*client));
    client->pak = pak;
    client->cache = cache;
}

size_t cglink_page_client_request(cglink_page_client *client,
                                  const uint16_t *indices, uint32_t count,
                                  uint8_t *out)
{
    uint32_t written = 0;
    for (uint32_t i = 0; i < count && written < CGLINK_MAX_REQUEST_PAGES; ++i)
    {
        if (indices[i] >= client->pak->page_count)
            continue;
        const cglink_pak_page page = cglink_pak_page_at(client->pak, indices[i]);
        if (cglink_cache_contains(client->cache, page.rom_page))
            continue;
        put16(out + 4 + written * 2u, indices[i]);
        written++;
    }
    if (written == 0)
        return 0;
    out[0] = CGLINK_MSG_PAGE_REQUEST;
    out[1] = ++client->generation;
    put16(out + 2, (uint16_t)written);
    return 4u + written * 2u;
}

bool cglink_page_client_receive(cglink_page_client *client,
                                const uint8_t *payload, size_t size)
{
    if (size < 6 || payload[0] != CGLINK_MSG_PAGE_DATA)
        return false;
    const uint16_t index = le16(payload + 2);
    const uint16_t stored = le16(payload + 4);
    if (index >= client->pak->page_count || size != 6u + stored)
    {
        client->bad_pages++;
        return false;
    }
    const cglink_pak_page page = cglink_pak_page_at(client->pak, index);
    if (cglink_cache_contains(client->cache, page.rom_page))
        return true; /* a duplicate from an older request */
    uint8_t *slot = cglink_cache_insert(client->cache, page.rom_page);
    if (!slot)
    {
        client->bad_pages++;
        return false;
    }
    const uint32_t page_size = 1u << client->pak->page_shift;
    long decoded;
    if (stored == page_size)
    {
        memcpy(slot, payload + 6, page_size);
        decoded = (long)page_size;
    }
    else
        decoded = cglink_inflate(payload + 6, stored, slot, page_size);
    if (decoded != (long)page_size)
    {
        /* Never leave a corrupt page cached. */
        client->cache->slot_of[page.rom_page] = CGLINK_CACHE_NONE;
        client->cache->slots[(slot - client->cache->pages) / page_size].rom_page = CGLINK_CACHE_NONE;
        client->bad_pages++;
        return false;
    }
    client->pages_received++;
    return true;
}
