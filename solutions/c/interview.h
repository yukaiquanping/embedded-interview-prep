#ifndef INTERVIEW_H
#define INTERVIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    COPY_OK,
    COPY_TRUNCATED,
    COPY_UNTERMINATED,
    COPY_BAD_ARGUMENT
} copy_result;

/* src/dst may overlap. A non-NULL source_length must be a separate size_t
 * object that overlaps neither the source nor the destination byte region. */
copy_result bounded_str_copy(char *dst, size_t capacity, const char *src,
                             size_t src_extent, size_t *source_length);

bool read_be32(const uint8_t *src, size_t length, uint32_t *value);
bool write_be32(uint8_t *dst, size_t capacity, uint32_t value);

/* Single-threaded. Keep fields private to callers after ring_init(). */
typedef struct {
    uint8_t *data;
    size_t capacity;
    size_t read_index;
    size_t length;
} byte_ring;

bool ring_init(byte_ring *ring, uint8_t *storage, size_t capacity);
size_t ring_write(byte_ring *ring, const uint8_t *src, size_t length);
size_t ring_read(byte_ring *ring, uint8_t *dst, size_t capacity);

enum { FRAME_MAGIC = 0xA5, FRAME_MAX_PAYLOAD = 32 };

/* Callback payload is borrowed; copy it before returning if needed later. */
typedef void (*frame_callback)(const uint8_t *payload, size_t length, void *ctx);

typedef struct {
    unsigned int state;
    uint8_t length;
    uint8_t used;
    uint8_t checksum;
    uint8_t payload[FRAME_MAX_PAYLOAD];
} frame_parser;

void parser_init(frame_parser *parser);
size_t parser_feed(frame_parser *parser, const uint8_t *bytes, size_t length,
                   frame_callback callback, void *ctx);

/* Requires a nondecreasing array. Outputs are unchanged on bad arguments. */
bool sorted_lower_bound(const int32_t *array, size_t count, int32_t target,
                        size_t *index, bool *found);

bool field_get_u32(uint32_t word, unsigned int offset, unsigned int width,
                   uint32_t *value);
bool field_set_u32(uint32_t word, unsigned int offset, unsigned int width,
                   uint32_t value, uint32_t *updated);

#endif
