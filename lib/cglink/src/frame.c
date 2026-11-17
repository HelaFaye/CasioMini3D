#include <cglink/frame.h>

#define SLIP_END 0xC0u
#define SLIP_ESC 0xDBu
#define SLIP_ESC_END 0xDCu
#define SLIP_ESC_ESC 0xDDu

uint16_t cglink_crc16(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xffffu;
    for (size_t i = 0; i < size; ++i)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static size_t put(uint8_t value, uint8_t *out, size_t at, size_t capacity)
{
    if (value == SLIP_END || value == SLIP_ESC)
    {
        if (at + 2 > capacity)
            return 0;
        out[at] = SLIP_ESC;
        out[at + 1] = value == SLIP_END ? SLIP_ESC_END : SLIP_ESC_ESC;
        return at + 2;
    }
    if (at + 1 > capacity)
        return 0;
    out[at] = value;
    return at + 1;
}

size_t cglink_frame_encode(const uint8_t *payload, size_t size,
                           uint8_t *out, size_t capacity)
{
    if (size > CGLINK_MAX_PAYLOAD || capacity < 1)
        return 0;
    size_t at = 0;
    out[at++] = SLIP_END; /* flushes any line noise before the frame */
    for (size_t i = 0; i < size; ++i)
        if (!(at = put(payload[i], out, at, capacity)))
            return 0;
    const uint16_t crc = cglink_crc16(payload, size);
    if (!(at = put((uint8_t)(crc & 0xffu), out, at, capacity)) ||
        !(at = put((uint8_t)(crc >> 8), out, at, capacity)) || at + 1 > capacity)
        return 0;
    out[at++] = SLIP_END;
    return at;
}

void cglink_rx_init(cglink_rx *rx)
{
    rx->length = 0;
    rx->escaped = false;
    rx->overflow = false;
    rx->dropped = 0;
}

long cglink_rx_feed(cglink_rx *rx, uint8_t byte)
{
    if (byte == SLIP_END)
    {
        const size_t length = rx->length;
        const bool overflow = rx->overflow;
        rx->length = 0;
        rx->escaped = false;
        rx->overflow = false;
        if (length == 0 && !overflow)
            return -1; /* idle or leading END */
        if (overflow || length < 2)
        {
            rx->dropped++;
            return -1;
        }
        const size_t payload = length - 2;
        const uint16_t crc = (uint16_t)(rx->buffer[payload] | rx->buffer[payload + 1] << 8);
        if (crc != cglink_crc16(rx->buffer, payload))
        {
            rx->dropped++;
            return -1;
        }
        return (long)payload;
    }
    if (byte == SLIP_ESC)
    {
        rx->escaped = true;
        return -1;
    }
    if (rx->escaped)
    {
        rx->escaped = false;
        byte = byte == SLIP_ESC_END ? SLIP_END : byte == SLIP_ESC_ESC ? SLIP_ESC : byte;
    }
    if (rx->length >= sizeof(rx->buffer))
    {
        rx->overflow = true;
        return -1;
    }
    rx->buffer[rx->length++] = byte;
    return -1;
}
