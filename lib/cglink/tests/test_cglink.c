/* Host tests for cglink. zlib is the reference compressor. */
#include <cglink/cache.h>
#include <cglink/frame.h>
#include <cglink/inflate.h>
#include <cglink/pages.h>
#include <cglink/pak.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint32_t rng = 12345u;
static uint8_t next_byte(void)
{
    rng = rng * 1103515245u + 12345u;
    return (uint8_t)(rng >> 16);
}

/* Raw deflate with zlib at `level` (window bits -15: no zlib header). */
static size_t raw_deflate(const uint8_t *in, size_t size, uint8_t *out, size_t cap, int level, int strategy)
{
    z_stream z;
    memset(&z, 0, sizeof(z));
    assert(deflateInit2(&z, level, Z_DEFLATED, -15, 9, strategy) == Z_OK);
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)size;
    z.next_out = out;
    z.avail_out = (uInt)cap;
    assert(deflate(&z, Z_FINISH) == Z_STREAM_END);
    const size_t written = cap - z.avail_out;
    deflateEnd(&z);
    return written;
}

static void fill(uint8_t *data, size_t size, int kind)
{
    for (size_t i = 0; i < size; ++i)
    {
        switch (kind)
        {
            case 0: data[i] = next_byte(); break;                       /* noise */
            case 1: data[i] = (uint8_t)(i % 7 == 0 ? next_byte() : i / 64); break; /* tile-like */
            case 2: data[i] = (uint8_t)"minish cap "[i % 11]; break;    /* long repeats */
            default: data[i] = 0; break;
        }
    }
}

static void test_inflate(void)
{
    static uint8_t original[70000], packed[80000], decoded[70000];
    const size_t sizes[] = {0, 1, 100, 4096, 70000};
    const int levels[] = {0, 1, 6, 9};
    const int strategies[] = {Z_DEFAULT_STRATEGY, Z_FIXED, Z_HUFFMAN_ONLY, Z_RLE};
    int cases = 0;
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s)
        for (int kind = 0; kind < 4; ++kind)
            for (size_t l = 0; l < 4; ++l)
                for (size_t st = 0; st < 4; ++st)
                {
                    fill(original, sizes[s], kind);
                    const size_t n = raw_deflate(original, sizes[s], packed, sizeof(packed),
                                                 levels[l], strategies[st]);
                    const long got = cglink_inflate(packed, n, decoded, sizeof(decoded));
                    assert(got == (long)sizes[s]);
                    assert(memcmp(original, decoded, sizes[s]) == 0);
                    ++cases;
                }
    /* Errors: truncated input, too-small output, invalid block type. */
    fill(original, 4096, 1);
    const size_t n = raw_deflate(original, 4096, packed, sizeof(packed), 9, Z_DEFAULT_STRATEGY);
    assert(cglink_inflate(packed, n / 2, decoded, sizeof(decoded)) < 0);
    assert(cglink_inflate(packed, n, decoded, 100) == CGLINK_INFLATE_TOO_LONG);
    const uint8_t bad_type[] = {0x07};
    assert(cglink_inflate(bad_type, 1, decoded, sizeof(decoded)) == CGLINK_INFLATE_BAD_DATA);
    printf("inflate: %d zlib streams decoded, errors detected\n", cases);
}

static void test_frame(void)
{
    uint8_t payload[600], framed[CGLINK_MAX_FRAMED];
    for (size_t i = 0; i < sizeof(payload); ++i)
        payload[i] = (uint8_t)(i % 3 == 0 ? 0xC0 : i % 3 == 1 ? 0xDB : i);
    const size_t n = cglink_frame_encode(payload, sizeof(payload), framed, sizeof(framed));
    assert(n > sizeof(payload));
    static cglink_rx rx;
    cglink_rx_init(&rx);
    /* Line noise before the frame is discarded by the leading END. */
    cglink_rx_feed(&rx, 0x55);
    long got = -1;
    for (size_t i = 0; i < n; ++i)
    {
        const long r = cglink_rx_feed(&rx, framed[i]);
        if (r >= 0)
            got = r;
    }
    assert(got == (long)sizeof(payload) && memcmp(rx.buffer, payload, sizeof(payload)) == 0);
    /* A flipped bit fails the CRC and is counted, not delivered. */
    framed[10] ^= 0x01;
    const uint32_t dropped = rx.dropped;
    got = -1;
    for (size_t i = 0; i < n; ++i)
    {
        const long r = cglink_rx_feed(&rx, framed[i]);
        if (r >= 0)
            got = r;
    }
    assert(got < 0 && rx.dropped == dropped + 1);
    assert(cglink_crc16((const uint8_t *)"123456789", 9) == 0x29B1); /* CCITT-FALSE check value */
    printf("frame: escapes, noise and corruption handled\n");
}

static void test_cache(void)
{
    static uint8_t pages[4 * 16];
    static cglink_cache_slot slots[4];
    static uint16_t slot_of[CGLINK_CACHE_MAX_PAGES];
    cglink_cache cache;
    cglink_cache_init(&cache, pages, slots, slot_of, 4, 16);
    for (uint16_t p = 0; p < 4; ++p)
        memset(cglink_cache_insert(&cache, p), p, 16);
    cglink_cache_pin(&cache, 0, true);
    assert(cglink_cache_get(&cache, 1)[0] == 1); /* 1 is now recent */
    cglink_cache_insert(&cache, 10);             /* evicts 2: oldest unpinned */
    assert(!cglink_cache_contains(&cache, 2) && cglink_cache_contains(&cache, 0));
    cglink_cache_insert(&cache, 11);             /* evicts 3 */
    cglink_cache_insert(&cache, 12);             /* evicts 1 */
    assert(cglink_cache_contains(&cache, 0) && !cglink_cache_contains(&cache, 1));
    for (uint16_t p = 10; p <= 12; ++p)
        cglink_cache_pin(&cache, p, true);
    assert(cglink_cache_insert(&cache, 13) == NULL); /* everything pinned */
    assert(cglink_cache_get(&cache, 99) == NULL && cache.misses == 1);
    printf("cache: LRU eviction and pinning\n");
}

/* A synthetic PCPK pak: 40 pages of 4 KB, baseline = pages 0-1, two rooms. */
static uint8_t pak_bytes[400000];
static size_t pak_size;
static uint8_t rom[64 * 4096];

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }

static void build_pak(void)
{
    const uint32_t pages = 40, baseline = 2, rooms = 2;
    const uint16_t manifest[] = {2, 3, 4, 10, 11, 12, 13}; /* room A: 3, room B: 4 */
    for (size_t i = 0; i < sizeof(rom); ++i)
        rom[i] = (uint8_t)((i / 4096) % 3 == 0 ? next_byte() : (i * 7 / 4096) & 0xff);
    uint8_t *p = pak_bytes;
    memcpy(p, "PCPK", 4);
    put16(p + 4, 1);
    p[6] = CGLINK_PAK_CODEC_DEFLATE;
    p[7] = 12;
    put32(p + 8, sizeof(rom));
    put32(p + 12, pages);
    put32(p + 16, rooms);
    put32(p + 20, baseline);
    put32(p + 24, sizeof(manifest) / 2);
    size_t index = 28 + pages * 8 + baseline * 2 + rooms * 8 + sizeof(manifest);
    uint8_t *data = pak_bytes + index;
    uint32_t offset = 0;
    for (uint32_t i = 0; i < pages; ++i)
    {
        const uint16_t rom_page = (uint16_t)(i + 5); /* stored pages 5..44 */
        size_t n = raw_deflate(rom + rom_page * 4096u, 4096, data + offset, 8192, 9, Z_DEFAULT_STRATEGY);
        if (n >= 4096)
        {
            memcpy(data + offset, rom + rom_page * 4096u, 4096);
            n = 4096;
        }
        put16(p + 28 + i * 8, rom_page);
        put16(p + 28 + i * 8 + 2, (uint16_t)n);
        put32(p + 28 + i * 8 + 4, offset);
        offset += (uint32_t)n;
    }
    uint8_t *q = p + 28 + pages * 8;
    put16(q, 0); put16(q + 2, 1); q += 4;
    q[0] = 0x02; q[1] = 0x00; put16(q + 2, 3); put32(q + 4, 0); q += 8;
    q[0] = 0x05; q[1] = 0x01; put16(q + 2, 4); put32(q + 4, 3); q += 8;
    for (size_t i = 0; i < sizeof(manifest) / 2; ++i, q += 2)
        put16(q, manifest[i]);
    pak_size = index + offset;
}

static size_t read_pak(void *context, uint32_t offset, uint8_t *out, size_t size)
{
    (void)context;
    if (offset >= pak_size)
        return 0;
    if (size > pak_size - offset)
        size = pak_size - offset;
    memcpy(out, pak_bytes + offset, size);
    return size;
}

static void test_streaming(void)
{
    build_pak();
    cglink_pak pak;
    assert(cglink_pak_open(&pak, pak_bytes, pak_size));
    assert(pak.page_count == 40 && pak.baseline_count == 2 && pak.room_count == 2);
    assert(cglink_pak_find_page(&pak, 5) == 0 && cglink_pak_find_page(&pak, 44) == 39);
    assert(cglink_pak_find_page(&pak, 4) == -1);
    uint32_t first, count;
    assert(cglink_pak_room(&pak, 0x05, 0x01, &first, &count) && first == 3 && count == 4);
    assert(!cglink_pak_room(&pak, 0x07, 0x00, &first, &count));

    static uint8_t pages[8 * 4096];
    static cglink_cache_slot slots[8];
    static uint16_t slot_of[CGLINK_CACHE_MAX_PAGES];
    cglink_cache cache;
    cglink_cache_init(&cache, pages, slots, slot_of, 8, 4096);
    static cglink_page_server server;
    static cglink_page_client client;
    cglink_page_server_init(&server, &pak, read_pak, NULL);
    cglink_page_client_init(&client, &pak, &cache);

    /* Request baseline + room 0x05/0x01, then carry every message through
       real frames and the receiver, as the serial link will. */
    uint16_t want[6] = {cglink_pak_baseline(&pak, 0), cglink_pak_baseline(&pak, 1)};
    for (uint32_t i = 0; i < count; ++i)
        want[2 + i] = cglink_pak_manifest(&pak, first + i);
    static uint8_t payload[CGLINK_MAX_PAYLOAD], framed[CGLINK_MAX_FRAMED];
    static cglink_rx to_server, to_client;
    cglink_rx_init(&to_server);
    cglink_rx_init(&to_client);
    size_t n = cglink_page_client_request(&client, want, 6, payload);
    size_t f = cglink_frame_encode(payload, n, framed, sizeof(framed));
    for (size_t i = 0; i < f; ++i)
    {
        const long r = cglink_rx_feed(&to_server, framed[i]);
        if (r >= 0)
            assert(cglink_page_server_receive(&server, to_server.buffer, (size_t)r));
    }
    while ((n = cglink_page_server_next(&server, payload)) != 0)
    {
        f = cglink_frame_encode(payload, n, framed, sizeof(framed));
        for (size_t i = 0; i < f; ++i)
        {
            const long r = cglink_rx_feed(&to_client, framed[i]);
            if (r >= 0)
                assert(cglink_page_client_receive(&client, to_client.buffer, (size_t)r));
        }
    }
    assert(client.pages_received == 6 && client.bad_pages == 0);
    for (int i = 0; i < 6; ++i)
    {
        const uint16_t rom_page = cglink_pak_page_at(&pak, want[i]).rom_page;
        const uint8_t *cached = cglink_cache_get(&cache, rom_page);
        assert(cached && memcmp(cached, rom + rom_page * 4096u, 4096) == 0);
    }
    /* Everything cached: no new request is needed. */
    assert(cglink_page_client_request(&client, want, 6, payload) == 0);
    printf("streaming: %u pages framed, served, decoded and cached exactly\n",
           (unsigned)client.pages_received);
}

int main(void)
{
    test_inflate();
    test_frame();
    test_cache();
    test_streaming();
    printf("all cglink tests passed\n");
    return 0;
}
