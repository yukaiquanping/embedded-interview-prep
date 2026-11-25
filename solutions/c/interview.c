#include "interview.h"

#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8, "These byte protocols require 8-bit bytes");

copy_result bounded_str_copy(char *dst, size_t capacity, const char *src,
                             size_t src_extent, size_t *source_length)
{
    size_t length = 0;
    size_t copied;
    if (src == NULL || (capacity > 0 && dst == NULL)) {
        return COPY_BAD_ARGUMENT;
    }
    while (length < src_extent && src[length] != '\0') {
        ++length;
    }
    if (length == src_extent) {
        return COPY_UNTERMINATED;
    }
    if (source_length != NULL) {
        *source_length = length;
    }
    if (capacity == 0) {
        return COPY_TRUNCATED;
    }
    copied = length < capacity ? length : capacity - 1;
    /* The scan has finished, so overlapping source/destination are supported. */
    memmove(dst, src, copied);
    dst[copied] = '\0';
    return length < capacity ? COPY_OK : COPY_TRUNCATED;
}

bool read_be32(const uint8_t *src, size_t length, uint32_t *value)
{
    if (src == NULL || value == NULL || length < 4) {
        return false;
    }
    *value = ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16)
           | ((uint32_t)src[2] << 8) | (uint32_t)src[3];
    return true;
}

bool write_be32(uint8_t *dst, size_t capacity, uint32_t value)
{
    if (dst == NULL || capacity < 4) {
        return false;
    }
    dst[0] = (uint8_t)(value >> 24);
    dst[1] = (uint8_t)(value >> 16);
    dst[2] = (uint8_t)(value >> 8);
    dst[3] = (uint8_t)value;
    return true;
}

bool ring_init(byte_ring *ring, uint8_t *storage, size_t capacity)
{
    if (ring == NULL || (capacity > 0 && storage == NULL)) {
        return false;
    }
    ring->data = storage;
    ring->capacity = capacity;
    ring->read_index = 0;
    ring->length = 0;
    return true;
}

static size_t next_index(size_t index, size_t capacity)
{
    return index + 1 == capacity ? 0 : index + 1;
}

size_t ring_write(byte_ring *ring, const uint8_t *src, size_t length)
{
    size_t accepted;
    size_t write_index;
    if (ring == NULL || (length > 0 && src == NULL)) {
        return 0;
    }
    accepted = ring->capacity - ring->length;
    if (accepted > length) {
        accepted = length;
    }
    if (accepted == 0) {
        return 0;
    }
    /* Avoid read_index + length overflowing size_t before taking modulo. */
    if (ring->length >= ring->capacity - ring->read_index) {
        write_index = ring->length - (ring->capacity - ring->read_index);
    } else {
        write_index = ring->read_index + ring->length;
    }
    for (size_t i = 0; i < accepted; ++i) {
        ring->data[write_index] = src[i];
        write_index = next_index(write_index, ring->capacity);
    }
    ring->length += accepted;
    return accepted;
}

size_t ring_read(byte_ring *ring, uint8_t *dst, size_t capacity)
{
    size_t taken;
    if (ring == NULL || (capacity > 0 && dst == NULL)) {
        return 0;
    }
    taken = ring->length < capacity ? ring->length : capacity;
    for (size_t i = 0; i < taken; ++i) {
        dst[i] = ring->data[ring->read_index];
        ring->read_index = next_index(ring->read_index, ring->capacity);
    }
    ring->length -= taken;
    return taken;
}

enum parser_state { WAIT_MAGIC, WAIT_LENGTH, READ_PAYLOAD, WAIT_CHECKSUM };

void parser_init(frame_parser *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

size_t parser_feed(frame_parser *parser, const uint8_t *bytes, size_t length,
                   frame_callback callback, void *ctx)
{
    size_t emitted = 0;
    if (parser == NULL || (length > 0 && bytes == NULL)) {
        return 0;
    }
    for (size_t i = 0; i < length; ++i) {
        const uint8_t byte = bytes[i];
        switch (parser->state) {
        case WAIT_MAGIC:
            if (byte == FRAME_MAGIC) {
                parser->state = WAIT_LENGTH;
            }
            break;
        case WAIT_LENGTH:
            if (byte > FRAME_MAX_PAYLOAD) {
                parser->state = WAIT_MAGIC;
                break;
            }
            parser->length = byte;
            parser->used = 0;
            parser->checksum = byte;
            parser->state = byte == 0 ? WAIT_CHECKSUM : READ_PAYLOAD;
            break;
        case READ_PAYLOAD:
            parser->payload[parser->used++] = byte;
            parser->checksum ^= byte;
            if (parser->used == parser->length) {
                parser->state = WAIT_CHECKSUM;
            }
            break;
        case WAIT_CHECKSUM:
            parser->state = WAIT_MAGIC;
            if (byte == parser->checksum) {
                ++emitted;
                if (callback != NULL) {
                    callback(parser->payload, parser->length, ctx);
                }
            }
            break;
        default:
            /* Call parser_init before first use; this is only a defensive reset. */
            parser->state = WAIT_MAGIC;
            break;
        }
    }
    return emitted;
}

bool sorted_lower_bound(const int32_t *array, size_t count, int32_t target,
                        size_t *index, bool *found)
{
    size_t left = 0;
    size_t right = count;
    if ((count > 0 && array == NULL) || index == NULL || found == NULL) {
        return false;
    }
    while (left < right) {
        const size_t mid = left + (right - left) / 2;
        if (array[mid] < target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    *index = left;
    *found = left < count && array[left] == target;
    return true;
}

static bool field_mask(unsigned int offset, unsigned int width, uint32_t *mask)
{
    if (offset >= 32 || width == 0 || width > 32 - offset) {
        return false;
    }
    *mask = width == 32 ? UINT32_MAX : (UINT32_C(1) << width) - UINT32_C(1);
    return true;
}

bool field_get_u32(uint32_t word, unsigned int offset, unsigned int width,
                   uint32_t *value)
{
    uint32_t mask;
    if (value == NULL || !field_mask(offset, width, &mask)) {
        return false;
    }
    *value = (word >> offset) & mask;
    return true;
}

bool field_set_u32(uint32_t word, unsigned int offset, unsigned int width,
                   uint32_t value, uint32_t *updated)
{
    uint32_t mask;
    if (updated == NULL || !field_mask(offset, width, &mask) || (value & ~mask) != 0) {
        return false;
    }
    *updated = (word & ~(mask << offset)) | (value << offset);
    return true;
}
