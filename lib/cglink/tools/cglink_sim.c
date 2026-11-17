/*
 * cglink_sim: replay a route of room changes over a simulated serial link and
 * report how long the game would stall waiting for pages.
 *
 * usage: cglink_sim <page.pak> <route.txt> [baud ...]
 *   route.txt  lines "area room dwell_seconds" (area/room in hex); '#' comments
 *
 * The real cglink page server, client, framing, deflate decoder and cache run
 * here; only time is simulated. The link carries baud/10 bytes per second in
 * each direction (8N1). A room can start only when the baseline and that
 * room's manifest pages are cached; until then the game stalls. Three preload
 * policies are compared:
 *   demand   request a room's pages when entering it
 *   next     also prefetch the next room on the route while playing
 *   next2    also prefetch the two next rooms
 * "next" knows the route in advance, so it is the best case for a preloader
 * that predicts the next room from the room's exits.
 */
#include <cglink/cache.h>
#include <cglink/frame.h>
#include <cglink/pages.h>
#include <cglink/pak.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAMES_PER_SECOND 60
#define MAX_ROUTE 512
#define CACHE_PAGES 512 /* 2 MB of 4 KB pages */

typedef struct
{
    unsigned area, room;
    double dwell;
} stop;

static uint8_t *pak_bytes;
static size_t pak_size;
static cglink_pak pak;
static stop route[MAX_ROUTE];
static size_t route_length;

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

/* Pages a room needs: the baseline and its manifest (pak page indices). */
static uint32_t room_pages(unsigned area, unsigned room, uint16_t *out, uint32_t at, uint32_t cap,
                           int with_baseline)
{
    if (with_baseline)
        for (uint32_t i = 0; i < pak.baseline_count && at < cap; ++i)
            out[at++] = cglink_pak_baseline(&pak, i);
    uint32_t first, count;
    if (cglink_pak_room(&pak, (uint8_t)area, (uint8_t)room, &first, &count))
        for (uint32_t i = 0; i < count && at < cap; ++i)
            out[at++] = cglink_pak_manifest(&pak, first + i);
    return at;
}

static int all_cached(const cglink_cache *cache, const uint16_t *pages, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
        if (!cglink_cache_contains(cache, cglink_pak_page_at(&pak, pages[i]).rom_page))
            return 0;
    return 1;
}

typedef struct
{
    double total_stall, worst_stall;
    unsigned stalls;
    unsigned long bytes;
} result;

static result simulate(long baud, int lookahead, int verbose)
{
    static uint8_t cache_pages[CACHE_PAGES * 4096];
    static cglink_cache_slot slots[CACHE_PAGES];
    static uint16_t slot_of[CGLINK_CACHE_MAX_PAGES];
    static cglink_page_server server;
    static cglink_page_client client;
    static uint8_t payload[CGLINK_MAX_PAYLOAD], framed[CGLINK_MAX_FRAMED];
    cglink_cache cache;
    cglink_cache_init(&cache, cache_pages, slots, slot_of, CACHE_PAGES, 1u << pak.page_shift);
    cglink_page_server_init(&server, &pak, read_pak, NULL);
    cglink_page_client_init(&client, &pak, &cache);

    const double bytes_per_frame = baud / 10.0 / FRAMES_PER_SECOND;
    double up_credit = 0, down_credit = 0;
    /* A request in flight to the server and a page frame in flight back. */
    size_t up_left = 0, up_size = 0, down_left = 0, down_size = 0;
    static uint8_t up_payload[CGLINK_MAX_PAYLOAD], down_payload[CGLINK_MAX_PAYLOAD];

    result r = {0, 0, 0, 0};
    for (size_t s = 0; s < route_length; ++s)
    {
        static uint16_t need[2048], want[4096];
        const uint32_t need_count = room_pages(route[s].area, route[s].room, need, 0, 2048, 1);
        uint32_t want_count = 0;
        memcpy(want, need, need_count * sizeof(uint16_t));
        want_count = need_count;
        for (int k = 1; k <= lookahead && s + (size_t)k < route_length; ++k)
            want_count = room_pages(route[s + k].area, route[s + k].room, want, want_count, 4096, 0);
        /* Pin what this room needs once it is cached; unpin the last room. */
        cglink_cache_unpin_all(&cache);
        const size_t n = cglink_page_client_request(&client, want, want_count, payload);
        if (n)
        {
            up_size = cglink_frame_encode(payload, n, framed, sizeof(framed));
            memcpy(up_payload, payload, n);
            up_left = up_size;
            up_size = n;
        }

        long frame = 0, stall_frames = -1;
        const long dwell_frames = (long)(route[s].dwell * FRAMES_PER_SECOND);
        long played = 0;
        while (played < dwell_frames)
        {
            ++frame;
            /* Upstream: the request reaches the server when fully sent. */
            up_credit += bytes_per_frame;
            if (up_left)
            {
                const size_t step = up_credit >= (double)up_left ? up_left : (size_t)up_credit;
                up_left -= step;
                up_credit -= (double)step;
                r.bytes += step;
                if (!up_left)
                    cglink_page_server_receive(&server, up_payload, up_size);
            }
            else
                up_credit = 0;
            /* Downstream: page frames back to back. */
            down_credit += bytes_per_frame;
            for (;;)
            {
                if (!down_left)
                {
                    down_size = cglink_page_server_next(&server, down_payload);
                    if (!down_size)
                    {
                        down_credit = 0;
                        break;
                    }
                    down_left = cglink_frame_encode(down_payload, down_size, framed, sizeof(framed));
                }
                const size_t step = down_credit >= (double)down_left ? down_left : (size_t)down_credit;
                down_left -= step;
                down_credit -= (double)step;
                r.bytes += step;
                if (down_left)
                    break;
                cglink_page_client_receive(&client, down_payload, down_size);
            }
            /* The game runs only once its pages are present. */
            if (stall_frames < 0)
            {
                if (all_cached(&cache, need, need_count))
                {
                    stall_frames = frame - 1;
                    for (uint32_t i = 0; i < need_count; ++i)
                        cglink_cache_pin(&cache, cglink_pak_page_at(&pak, need[i]).rom_page, 1);
                }
                else
                    continue;
            }
            ++played;
        }
        const double stall = stall_frames / (double)FRAMES_PER_SECOND;
        r.total_stall += stall;
        if (stall > r.worst_stall)
            r.worst_stall = stall;
        if (stall > 0.1)
            r.stalls++;
        if (verbose)
            printf("    %02x/%02x: needs %3u pages, stall %5.2f s\n", route[s].area, route[s].room,
                   (unsigned)need_count, stall);
    }
    return r;
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <page.pak> <route.txt> [baud ...] [-v]\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f)
        return perror(argv[1]), 1;
    fseek(f, 0, SEEK_END);
    pak_size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    pak_bytes = malloc(pak_size);
    if (fread(pak_bytes, 1, pak_size, f) != pak_size || !cglink_pak_open(&pak, pak_bytes, pak_size))
        return fprintf(stderr, "%s: not a page pak\n", argv[1]), 1;
    fclose(f);

    f = fopen(argv[2], "r");
    if (!f)
        return perror(argv[2]), 1;
    char line[256];
    while (fgets(line, sizeof(line), f) && route_length < MAX_ROUTE)
    {
        stop s;
        if (line[0] == '#' || sscanf(line, "%x %x %lf", &s.area, &s.room, &s.dwell) != 3)
            continue;
        uint32_t first, count;
        if (!cglink_pak_room(&pak, (uint8_t)s.area, (uint8_t)s.room, &first, &count))
            fprintf(stderr, "note: %02x/%02x has no manifest (baseline only)\n", s.area, s.room);
        route[route_length++] = s;
    }
    fclose(f);

    int verbose = 0;
    long bauds[8];
    int baud_count = 0;
    for (int i = 3; i < argc; ++i)
    {
        if (strcmp(argv[i], "-v") == 0)
            verbose = 1;
        else if (baud_count < 8)
            bauds[baud_count++] = atol(argv[i]);
    }
    if (!baud_count)
    {
        bauds[baud_count++] = 115200;
        bauds[baud_count++] = 460800;
        bauds[baud_count++] = 921600;
    }
    double route_seconds = 0;
    for (size_t i = 0; i < route_length; ++i)
        route_seconds += route[i].dwell;
    printf("route: %zu rooms, %.0f s of play; cache %d pages (%d KB)\n", route_length, route_seconds,
           CACHE_PAGES, CACHE_PAGES * 4);
    static const char *const names[] = {"demand", "next", "next2"};
    for (int b = 0; b < baud_count; ++b)
        for (int policy = 0; policy < 3; ++policy)
        {
            if (verbose)
                printf("  %ld baud, %s:\n", bauds[b], names[policy]);
            const result r = simulate(bauds[b], policy, verbose);
            printf("%7ld baud %-6s: stalled %6.1f s total, worst %5.2f s, %3u stalls over 0.1 s, "
                   "%lu KB moved\n",
                   bauds[b], names[policy], r.total_stall, r.worst_stall, r.stalls, r.bytes / 1024);
        }
    return 0;
}
