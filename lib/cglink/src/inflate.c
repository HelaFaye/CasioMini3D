/* Raw DEFLATE decoder (RFC 1951). Canonical Huffman decoding by code length
   counts, as in the RFC's reference construction. */
#include <cglink/inflate.h>

#include <string.h>

#define MAX_BITS 15
#define MAX_LITERAL_CODES 288
#define MAX_DISTANCE_CODES 30

typedef struct
{
    uint16_t count[MAX_BITS + 1];   /* codes of each length */
    uint16_t symbol[MAX_LITERAL_CODES]; /* symbols ordered by code */
} huffman;

typedef struct
{
    const uint8_t *in;
    size_t in_size;
    size_t in_pos;
    uint32_t bit_buffer;
    unsigned bit_count;
    uint8_t *out;
    size_t out_capacity;
    size_t out_pos;
} state;

/* Returns -1 when the input runs out. */
static int bits(state *s, unsigned need)
{
    while (s->bit_count < need)
    {
        if (s->in_pos >= s->in_size)
            return -1;
        s->bit_buffer |= (uint32_t)s->in[s->in_pos++] << s->bit_count;
        s->bit_count += 8;
    }
    const int value = (int)(s->bit_buffer & ((1u << need) - 1u));
    s->bit_buffer >>= need;
    s->bit_count -= need;
    return value;
}

/* Build a decoding table from code lengths. Returns 0 on success, -1 for an
   over-subscribed set (an incomplete set is allowed, as zlib allows it). */
static int build(huffman *h, const uint8_t *lengths, unsigned n)
{
    uint16_t offsets[MAX_BITS + 1];
    memset(h->count, 0, sizeof(h->count));
    for (unsigned i = 0; i < n; ++i)
        h->count[lengths[i]]++;
    if (h->count[0] == n)
        return 0;
    int left = 1;
    for (unsigned len = 1; len <= MAX_BITS; ++len)
    {
        left <<= 1;
        left -= h->count[len];
        if (left < 0)
            return -1;
    }
    offsets[1] = 0;
    for (unsigned len = 1; len < MAX_BITS; ++len)
        offsets[len + 1] = (uint16_t)(offsets[len] + h->count[len]);
    for (unsigned i = 0; i < n; ++i)
        if (lengths[i] != 0)
            h->symbol[offsets[lengths[i]]++] = (uint16_t)i;
    return 0;
}

/* Decode one symbol, reading the code bit by bit (codes are stored MSB
   first within the LSB-first bit stream). */
static int decode(state *s, const huffman *h)
{
    int code = 0, first = 0, index = 0;
    for (unsigned len = 1; len <= MAX_BITS; ++len)
    {
        const int bit = bits(s, 1);
        if (bit < 0)
            return CGLINK_INFLATE_TRUNCATED;
        code |= bit;
        const int count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return CGLINK_INFLATE_BAD_DATA;
}

static const uint16_t LENGTH_BASE[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t LENGTH_EXTRA[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t DISTANCE_BASE[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
    8193, 12289, 16385, 24577};
static const uint8_t DISTANCE_EXTRA[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(state *s, const huffman *literals, const huffman *distances)
{
    for (;;)
    {
        int symbol = decode(s, literals);
        if (symbol < 0)
            return symbol;
        if (symbol < 256)
        {
            if (s->out_pos >= s->out_capacity)
                return CGLINK_INFLATE_TOO_LONG;
            s->out[s->out_pos++] = (uint8_t)symbol;
            continue;
        }
        if (symbol == 256)
            return 0;
        symbol -= 257;
        if (symbol >= 29)
            return CGLINK_INFLATE_BAD_DATA;
        int extra = bits(s, LENGTH_EXTRA[symbol]);
        if (extra < 0)
            return CGLINK_INFLATE_TRUNCATED;
        const size_t length = (size_t)LENGTH_BASE[symbol] + (size_t)extra;
        const int distance_symbol = decode(s, distances);
        if (distance_symbol < 0)
            return distance_symbol;
        if (distance_symbol >= 30)
            return CGLINK_INFLATE_BAD_DATA;
        extra = bits(s, DISTANCE_EXTRA[distance_symbol]);
        if (extra < 0)
            return CGLINK_INFLATE_TRUNCATED;
        const size_t distance =
            (size_t)DISTANCE_BASE[distance_symbol] + (size_t)extra;
        if (distance > s->out_pos)
            return CGLINK_INFLATE_BAD_DATA;
        if (length > s->out_capacity - s->out_pos)
            return CGLINK_INFLATE_TOO_LONG;
        /* Byte by byte: a match may overlap its own output. */
        for (size_t i = 0; i < length; ++i, ++s->out_pos)
            s->out[s->out_pos] = s->out[s->out_pos - distance];
    }
}

static int stored(state *s)
{
    s->bit_buffer = 0;
    s->bit_count = 0; /* skip to the byte boundary */
    if (s->in_size - s->in_pos < 4)
        return CGLINK_INFLATE_TRUNCATED;
    const unsigned length = s->in[s->in_pos] | (unsigned)s->in[s->in_pos + 1] << 8;
    const unsigned check = s->in[s->in_pos + 2] | (unsigned)s->in[s->in_pos + 3] << 8;
    s->in_pos += 4;
    if (length != (~check & 0xffffu))
        return CGLINK_INFLATE_BAD_DATA;
    if (s->in_size - s->in_pos < length)
        return CGLINK_INFLATE_TRUNCATED;
    if (s->out_capacity - s->out_pos < length)
        return CGLINK_INFLATE_TOO_LONG;
    memcpy(s->out + s->out_pos, s->in + s->in_pos, length);
    s->out_pos += length;
    s->in_pos += length;
    return 0;
}

static int fixed(state *s)
{
    static huffman literals, distances;
    static int built = 0;
    if (!built)
    {
        uint8_t lengths[MAX_LITERAL_CODES];
        unsigned i = 0;
        for (; i < 144; ++i) lengths[i] = 8;
        for (; i < 256; ++i) lengths[i] = 9;
        for (; i < 280; ++i) lengths[i] = 7;
        for (; i < 288; ++i) lengths[i] = 8;
        build(&literals, lengths, 288);
        for (i = 0; i < 30; ++i) lengths[i] = 5;
        build(&distances, lengths, 30);
        built = 1;
    }
    return codes(s, &literals, &distances);
}

static int dynamic(state *s)
{
    static const uint8_t ORDER[19] = {
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t lengths[MAX_LITERAL_CODES + MAX_DISTANCE_CODES];
    huffman literals, distances;

    const int nlen = bits(s, 5), ndist = bits(s, 5), ncode = bits(s, 4);
    if (nlen < 0 || ndist < 0 || ncode < 0)
        return CGLINK_INFLATE_TRUNCATED;
    const unsigned literal_count = (unsigned)nlen + 257;
    const unsigned distance_count = (unsigned)ndist + 1;
    if (literal_count > 286 || distance_count > 30)
        return CGLINK_INFLATE_BAD_DATA;

    memset(lengths, 0, 19);
    for (unsigned i = 0; i < (unsigned)ncode + 4; ++i)
    {
        const int length = bits(s, 3);
        if (length < 0)
            return CGLINK_INFLATE_TRUNCATED;
        lengths[ORDER[i]] = (uint8_t)length;
    }
    if (build(&literals, lengths, 19) != 0)
        return CGLINK_INFLATE_BAD_DATA;

    unsigned index = 0;
    while (index < literal_count + distance_count)
    {
        int symbol = decode(s, &literals);
        if (symbol < 0)
            return symbol;
        if (symbol < 16)
        {
            lengths[index++] = (uint8_t)symbol;
            continue;
        }
        uint8_t value = 0;
        int repeat;
        if (symbol == 16)
        {
            if (index == 0)
                return CGLINK_INFLATE_BAD_DATA;
            value = lengths[index - 1];
            repeat = bits(s, 2);
            repeat = repeat < 0 ? -1 : repeat + 3;
        }
        else if (symbol == 17)
        {
            repeat = bits(s, 3);
            repeat = repeat < 0 ? -1 : repeat + 3;
        }
        else
        {
            repeat = bits(s, 7);
            repeat = repeat < 0 ? -1 : repeat + 11;
        }
        if (repeat < 0)
            return CGLINK_INFLATE_TRUNCATED;
        if (index + (unsigned)repeat > literal_count + distance_count)
            return CGLINK_INFLATE_BAD_DATA;
        while (repeat--)
            lengths[index++] = value;
    }
    if (lengths[256] == 0)
        return CGLINK_INFLATE_BAD_DATA; /* no end-of-block code */
    if (build(&literals, lengths, literal_count) != 0 ||
        build(&distances, lengths + literal_count, distance_count) != 0)
        return CGLINK_INFLATE_BAD_DATA;
    return codes(s, &literals, &distances);
}

long cglink_inflate(const uint8_t *in, size_t in_size,
                    uint8_t *out, size_t out_capacity)
{
    state s;
    memset(&s, 0, sizeof(s));
    s.in = in;
    s.in_size = in_size;
    s.out = out;
    s.out_capacity = out_capacity;
    int last;
    do
    {
        last = bits(&s, 1);
        const int type = bits(&s, 2);
        if (last < 0 || type < 0)
            return CGLINK_INFLATE_TRUNCATED;
        int result;
        switch (type)
        {
            case 0: result = stored(&s); break;
            case 1: result = fixed(&s); break;
            case 2: result = dynamic(&s); break;
            default: return CGLINK_INFLATE_BAD_DATA;
        }
        if (result != 0)
            return result;
    } while (!last);
    return (long)s.out_pos;
}
