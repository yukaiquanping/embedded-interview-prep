#include "interview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t checks;

#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static uint32_t random_word(void)
{
    static uint32_t state = UINT32_C(0x12345678);
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static void test_copy(void)
{
    char dst[8] = "KEEP";
    char overlap[12] = "abcdef";
    const char unterminated[] = {'x', 'y', 'z'};
    size_t length = 99;
    CHECK(bounded_str_copy(dst, sizeof(dst), "hello", 6, &length) == COPY_OK);
    CHECK(strcmp(dst, "hello") == 0 && length == 5);
    CHECK(bounded_str_copy(dst, 4, "hello", 6, &length) == COPY_TRUNCATED);
    CHECK(strcmp(dst, "hel") == 0 && length == 5);
    CHECK(bounded_str_copy(dst, 1, "hello", 6, NULL) == COPY_TRUNCATED);
    CHECK(dst[0] == '\0');
    CHECK(bounded_str_copy(dst, 6, "hello", 6, NULL) == COPY_OK);
    CHECK(strcmp(dst, "hello") == 0);
    CHECK(bounded_str_copy(NULL, 0, "", 1, &length) == COPY_TRUNCATED);
    CHECK(length == 0);
    CHECK(bounded_str_copy(dst, 1, "", 1, NULL) == COPY_OK);
    CHECK(dst[0] == '\0');
    memcpy(dst, "KEEP", 5);
    length = 99;
    CHECK(bounded_str_copy(dst, sizeof(dst), unterminated, sizeof(unterminated),
                           &length) == COPY_UNTERMINATED);
    CHECK(strcmp(dst, "KEEP") == 0 && length == 99);
    CHECK(bounded_str_copy(dst, sizeof(dst), "", 0, NULL) == COPY_UNTERMINATED);
    CHECK(bounded_str_copy(NULL, 1, "", 1, NULL) == COPY_BAD_ARGUMENT);
    CHECK(bounded_str_copy(dst, sizeof(dst), NULL, 0, NULL) == COPY_BAD_ARGUMENT);
    CHECK(bounded_str_copy(overlap + 2, 10, overlap, 7, NULL) == COPY_OK);
    CHECK(strcmp(overlap + 2, "abcdef") == 0);
    CHECK(bounded_str_copy(overlap, sizeof(overlap), overlap + 2, 7, NULL) == COPY_OK);
    CHECK(strcmp(overlap, "abcdef") == 0);
}

static void test_byte_order(void)
{
    uint8_t buffer[6] = {0xEE, 0, 0, 0, 0, 0xDD};
    uint32_t value = 77;
    const uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};
    CHECK(write_be32(buffer + 1, 4, UINT32_C(0x12345678)));
    CHECK(memcmp(buffer + 1, expected, 4) == 0);
    CHECK(buffer[0] == 0xEE && buffer[5] == 0xDD);
    CHECK(read_be32(buffer + 1, 4, &value) && value == UINT32_C(0x12345678));
    CHECK(!write_be32(buffer + 1, 3, 0));
    CHECK(memcmp(buffer + 1, expected, 4) == 0);
    value = 77;
    CHECK(!read_be32(buffer + 1, 3, &value) && value == 77);
    CHECK(!read_be32(NULL, 4, &value));
    CHECK(!read_be32(buffer, sizeof(buffer), NULL));
    CHECK(!write_be32(NULL, 4, 1));
    CHECK(write_be32(buffer + 1, 4, 0));
    CHECK(read_be32(buffer + 1, 4, &value) && value == 0);
    CHECK(write_be32(buffer + 1, 4, UINT32_MAX));
    CHECK(read_be32(buffer + 1, 4, &value) && value == UINT32_MAX);
    for (size_t i = 0; i < 1000; ++i) {
        const uint32_t original = random_word();
        CHECK(write_be32(buffer + 1, 4, original));
        CHECK(read_be32(buffer + 1, 4, &value) && value == original);
    }
}

static void test_ring(void)
{
    byte_ring ring;
    uint8_t storage[5];
    const uint8_t input[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t out[8] = {0};
    CHECK(!ring_init(NULL, storage, sizeof(storage)));
    CHECK(!ring_init(&ring, NULL, 1));
    CHECK(ring_init(&ring, NULL, 0));
    CHECK(ring_write(&ring, input, sizeof(input)) == 0);
    CHECK(ring_read(&ring, out, sizeof(out)) == 0);
    CHECK(ring_init(&ring, storage, sizeof(storage)));
    CHECK(ring_write(&ring, input, 7) == 5 && ring.length == 5);
    CHECK(ring_write(&ring, input, 1) == 0);
    CHECK(ring_read(&ring, out, 3) == 3 && memcmp(out, input, 3) == 0);
    CHECK(ring_write(&ring, input + 5, 3) == 3);
    CHECK(ring_read(&ring, out, sizeof(out)) == 5);
    CHECK(memcmp(out, input + 3, 5) == 0 && ring.length == 0);
    CHECK(ring_read(&ring, out, 1) == 0);
    CHECK(ring_write(&ring, NULL, 1) == 0);
    CHECK(ring_write(&ring, NULL, 0) == 0);
    CHECK(ring_write(&ring, input, 1) == 1);
    CHECK(ring_read(&ring, NULL, 1) == 0 && ring.length == 1);
    CHECK(ring_read(&ring, NULL, 0) == 0 && ring.length == 1);
    CHECK(ring_init(&ring, storage, 1));
    CHECK(ring_write(&ring, input, 2) == 1);
    CHECK(ring_read(&ring, out, 2) == 1 && out[0] == 1);

    /* Compare randomized traffic with a small linear FIFO model. */
    for (size_t cap = 0; cap <= sizeof(storage); ++cap) {
        uint8_t model[5];
        size_t used = 0;
        CHECK(ring_init(&ring, storage, cap));
        for (size_t step = 0; step < 2000; ++step) {
            const size_t request = (size_t)(random_word() % 8);
            if ((random_word() & 1U) != 0) {
                uint8_t generated[8];
                size_t expected = cap - used;
                if (expected > request) {
                    expected = request;
                }
                for (size_t j = 0; j < request; ++j) {
                    generated[j] = (uint8_t)random_word();
                }
                CHECK(ring_write(&ring, generated, request) == expected);
                memcpy(model + used, generated, expected);
                used += expected;
            } else {
                const size_t expected = used < request ? used : request;
                CHECK(ring_read(&ring, out, request) == expected);
                CHECK(memcmp(out, model, expected) == 0);
                used -= expected;
                memmove(model, model + expected, used);
            }
            CHECK(ring.length == used);
        }
    }
}

typedef struct {
    size_t count;
    size_t lengths[8];
    uint8_t data[8][FRAME_MAX_PAYLOAD];
} received_frames;

static void collect_frame(const uint8_t *payload, size_t length, void *ctx)
{
    received_frames *received = ctx;
    CHECK(received->count < 8 && length <= FRAME_MAX_PAYLOAD);
    received->lengths[received->count] = length;
    memcpy(received->data[received->count], payload, length);
    ++received->count;
}

static void test_parser(void)
{
    /* Leading noise; empty frame; payload containing the magic byte. */
    const uint8_t bytes[] = {0x00, 0x17, 0xA5, 0x00, 0x00,
                             0xA5, 0x03, 0x11, 0xA5, 0x22, 0x95};
    const uint8_t payload[] = {0x11, 0xA5, 0x22};
    frame_parser parser;
    for (size_t split = 0; split <= sizeof(bytes); ++split) {
        received_frames received = {0};
        size_t count;
        parser_init(&parser);
        count = parser_feed(&parser, bytes, split, collect_frame, &received);
        count += parser_feed(&parser, bytes + split, sizeof(bytes) - split,
                             collect_frame, &received);
        CHECK(count == 2 && received.count == 2);
        CHECK(received.lengths[0] == 0 && received.lengths[1] == 3);
        CHECK(memcmp(received.data[1], payload, sizeof(payload)) == 0);
    }
    {
        received_frames received = {0};
        size_t count = 0;
        parser_init(&parser);
        for (size_t i = 0; i < sizeof(bytes); ++i) {
            count += parser_feed(&parser, bytes + i, 1, collect_frame, &received);
        }
        CHECK(count == 2 && received.count == 2);
    }
    {
        const uint8_t damaged[] = {0xA5, 33, 0xA5, 1, 0x12, 0x00,
                                   0xA5, 0, 0};
        received_frames received = {0};
        parser_init(&parser);
        CHECK(parser_feed(&parser, damaged, sizeof(damaged), collect_frame,
                           &received) == 1);
        CHECK(received.count == 1 && received.lengths[0] == 0);
    }
    {
        /* Invalid length 0xA5 is consumed, not reused as a new magic byte. */
        const uint8_t bad_length_magic[] = {0xA5, 0xA5, 0, 0};
        const uint8_t recovery[] = {0xA5, 0, 0};
        received_frames received = {0};
        parser_init(&parser);
        CHECK(parser_feed(&parser, bad_length_magic, sizeof(bad_length_magic),
                           collect_frame, &received) == 0);
        CHECK(received.count == 0);
        CHECK(parser_feed(&parser, recovery, sizeof(recovery), collect_frame,
                           &received) == 1);
        CHECK(received.count == 1 && received.lengths[0] == 0);
    }
    {
        /* Wrong checksum 0xA5 is also consumed before searching resumes. */
        const uint8_t bad_checksum_magic[] = {0xA5, 1, 0x12, 0xA5, 0, 0};
        const uint8_t recovery[] = {0xA5, 0, 0};
        received_frames received = {0};
        parser_init(&parser);
        CHECK(parser_feed(&parser, bad_checksum_magic, sizeof(bad_checksum_magic),
                           collect_frame, &received) == 0);
        CHECK(received.count == 0);
        CHECK(parser_feed(&parser, recovery, sizeof(recovery), collect_frame,
                           &received) == 1);
        CHECK(received.count == 1 && received.lengths[0] == 0);
    }
    {
        uint8_t maximum[FRAME_MAX_PAYLOAD + 3];
        received_frames received = {0};
        maximum[0] = FRAME_MAGIC;
        maximum[1] = FRAME_MAX_PAYLOAD;
        maximum[sizeof(maximum) - 1] = FRAME_MAX_PAYLOAD;
        for (size_t i = 0; i < FRAME_MAX_PAYLOAD; ++i) {
            maximum[i + 2] = (uint8_t)i;
            maximum[sizeof(maximum) - 1] ^= (uint8_t)i;
        }
        parser_init(&parser);
        CHECK(parser_feed(&parser, maximum, sizeof(maximum) - 1,
                           collect_frame, &received) == 0);
        CHECK(received.count == 0);
        CHECK(parser_feed(&parser, NULL, 1, collect_frame, &received) == 0);
        CHECK(parser_feed(&parser, maximum + sizeof(maximum) - 1, 1,
                           collect_frame, &received) == 1);
        CHECK(received.lengths[0] == FRAME_MAX_PAYLOAD);
        CHECK(memcmp(received.data[0], maximum + 2, FRAME_MAX_PAYLOAD) == 0);
    }
    parser_init(&parser);
    CHECK(parser_feed(&parser, bytes, sizeof(bytes), NULL, NULL) == 2);
    CHECK(parser_feed(&parser, NULL, 0, NULL, NULL) == 0);
    CHECK(parser_feed(NULL, bytes, sizeof(bytes), NULL, NULL) == 0);
    parser_init(NULL);
}

static void test_lower_bound(void)
{
    const int32_t array[] = {INT32_MIN, -3, 0, 0, 0, 7, INT32_MAX};
    const size_t count = sizeof(array) / sizeof(array[0]);
    size_t index = 99;
    bool found = true;
    CHECK(sorted_lower_bound(NULL, 0, 1, &index, &found));
    CHECK(index == 0 && !found);
    CHECK(sorted_lower_bound(array, count, INT32_MIN, &index, &found));
    CHECK(index == 0 && found);
    CHECK(sorted_lower_bound(array, count, INT32_MAX, &index, &found));
    CHECK(index == 6 && found);
    CHECK(sorted_lower_bound(array, count, 0, &index, &found));
    CHECK(index == 2 && found);
    CHECK(sorted_lower_bound(array + 1, 5, 10, &index, &found));
    CHECK(index == 5 && !found);
    for (int32_t target = -10; target <= 10; ++target) {
        size_t linear = 0;
        while (linear < count && array[linear] < target) {
            ++linear;
        }
        CHECK(sorted_lower_bound(array, count, target, &index, &found));
        CHECK(index == linear && found == (linear < count && array[linear] == target));
    }
    index = 99;
    found = true;
    CHECK(!sorted_lower_bound(NULL, 1, 0, &index, &found));
    CHECK(index == 99 && found);
    CHECK(!sorted_lower_bound(array, count, 0, NULL, &found));
    CHECK(!sorted_lower_bound(array, count, 0, &index, NULL));
}

static void test_bit_fields(void)
{
    uint32_t value = 0;
    uint32_t updated = 0;
    CHECK(field_get_u32(UINT32_C(0xA5), 4, 4, &value) && value == 10);
    CHECK(field_set_u32(UINT32_C(0xA5), 4, 4, 3, &updated) && updated == 0x35);
    CHECK(field_get_u32(UINT32_MAX, 0, 32, &value) && value == UINT32_MAX);
    CHECK(field_set_u32(0, 0, 32, UINT32_MAX, &updated) && updated == UINT32_MAX);
    CHECK(field_set_u32(0, 31, 1, 1, &updated) && updated == UINT32_C(0x80000000));
    value = 77;
    updated = 88;
    CHECK(!field_get_u32(0, 32, 1, &value));
    CHECK(!field_get_u32(0, 0, 0, &value));
    CHECK(!field_get_u32(0, 31, 2, &value));
    CHECK(!field_get_u32(0, 0, 33, &value));
    CHECK(!field_get_u32(0, 0, 1, NULL));
    CHECK(value == 77);
    CHECK(!field_set_u32(0, 1, 32, 0, &updated));
    CHECK(!field_set_u32(0, 0, 0, 0, &updated));
    CHECK(!field_set_u32(0, 0, 3, 8, &updated));
    CHECK(!field_set_u32(0, 0, 1, 0, NULL));
    CHECK(updated == 88);
    for (unsigned int offset = 0; offset < 32; ++offset) {
        for (unsigned int width = 1; width <= 32 - offset; ++width) {
            const uint32_t mask = width == 32 ? UINT32_MAX : (UINT32_C(1) << width) - 1U;
            const uint32_t original = random_word();
            const uint32_t replacement = random_word() & mask;
            CHECK(field_set_u32(original, offset, width, replacement, &updated));
            CHECK(field_get_u32(updated, offset, width, &value) && value == replacement);
            CHECK((updated & ~(mask << offset)) == (original & ~(mask << offset)));
        }
    }
}

int main(void)
{
    test_copy();
    test_byte_order();
    test_ring();
    test_parser();
    test_lower_bound();
    test_bit_fields();
    printf("PASS: %zu checks across 6 exercises\n", checks);
    return EXIT_SUCCESS;
}
