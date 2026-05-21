#include "../render.h"
#include "../render_internal.h"
#include "test.h"

#include <stddef.h>
#include <string.h>

/* Captured output from the term stub. */
extern char   g_stub_buf[];
extern size_t g_stub_len;
extern int    g_stub_flushes;
extern void   term_stub_reset(void);

static void setup(int cols, int rows) {
    term_stub_reset();
    render_shutdown();
    int rc = render_init(cols, rows);
    (void)rc;
}

static void test_init_zeroes_buffers(void) {
    setup(40, 20);
    ASSERT(render_get_cols() == 40);
    ASSERT(render_get_rows() == 20);
    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 40; c++) {
            ASSERT(render_get_cell(c, r) == 0);
        }
    }
}

static void test_init_rejects_too_small(void) {
    render_shutdown();
    ASSERT(render_init(19, 10) == -1);
    ASSERT(render_init(20, 9) == -1);
}

static void test_clear_zeros_front(void) {
    setup(40, 20);
    render_circle(10.0f, 10.0f, 4.0f);
    /* At least one cell must be set now */
    int any = 0;
    for (int r = 0; r < 20 && !any; r++)
        for (int c = 0; c < 40 && !any; c++)
            if (render_get_cell(c, r)) any = 1;
    ASSERT(any);

    render_clear();
    for (int r = 0; r < 20; r++)
        for (int c = 0; c < 40; c++)
            ASSERT(render_get_cell(c, r) == 0);
}

static void test_circle_sets_expected_dot(void) {
    setup(40, 20);
    /* A 0-radius-eps disk at center of cell (0,0) dot-(0,0) lights only dot 1 (bit 0). */
    render_circle(0.0f, 0.0f, 0.4f);
    ASSERT(render_get_cell(0, 0) == 0x01);
}

static void test_circle_clips_to_buffer(void) {
    setup(20, 10);
    /* Center way outside; AABB should clip; no crash and no out-of-bounds writes. */
    render_circle(-100.0f, -100.0f, 5.0f);
    render_circle(10000.0f, 10000.0f, 5.0f);
    for (int r = 0; r < 10; r++)
        for (int c = 0; c < 20; c++)
            ASSERT(render_get_cell(c, r) == 0);
}

static void test_circle_nonpositive_radius_is_noop(void) {
    setup(40, 20);
    render_circle(10, 10, 0.0f);
    render_circle(10, 10, -3.0f);
    for (int r = 0; r < 20; r++)
        for (int c = 0; c < 40; c++)
            ASSERT(render_get_cell(c, r) == 0);
}

static void test_circle_is_additive(void) {
    setup(40, 20);
    /* Set dot 1 (bit 0) by tiny disk at dot (0,0). */
    render_circle(0.0f, 0.0f, 0.4f);
    int before = render_get_cell(0, 0);
    /* Set dot 2 (bit 1) by tiny disk at dot (0,1) — same cell, different dot. */
    render_circle(0.0f, 1.0f, 0.4f);
    int after = render_get_cell(0, 0);
    ASSERT(before == 0x01);
    ASSERT(after == 0x03); /* bits 0 and 1 both set: additive OR, not overwrite */
}

static void test_present_emits_utf8_braille(void) {
    setup(40, 20);
    render_circle(0.0f, 0.0f, 0.4f); /* lights dot 1 → pattern 0x01 → U+2801 */
    term_stub_reset();
    render_present();
    /* Expected encoded glyph for U+2801: 0xE2 0xA0 0x81 */
    int found = 0;
    for (size_t i = 0; i + 2 < g_stub_len; i++) {
        if ((unsigned char)g_stub_buf[i]     == 0xE2 &&
            (unsigned char)g_stub_buf[i + 1] == 0xA0 &&
            (unsigned char)g_stub_buf[i + 2] == 0x81) {
            found = 1;
            break;
        }
    }
    ASSERT(found);
}

static void test_present_emits_and_swaps(void) {
    setup(40, 20);
    render_circle(10.0f, 10.0f, 3.0f);
    term_stub_reset();
    render_present();
    ASSERT(g_stub_len > 0);
    ASSERT(g_stub_flushes == 1);
    /* After present, front has been copied into back. A second present of the * same content should emit zero bytes. */
    term_stub_reset();
    render_present();
    ASSERT(g_stub_len == 0);
}

static void test_present_with_no_changes_skips_flush(void) {
    setup(40, 20);
    /* Present an empty frame, then another. Second should be a no-op on the wire. */
    render_present();
    term_stub_reset();
    render_present();
    ASSERT(g_stub_flushes == 0);
    ASSERT(g_stub_len == 0);
}

static void test_resize_reallocates_and_zeroes(void) {
    setup(40, 20);
    render_circle(10.0f, 10.0f, 3.0f);
    render_resize(60, 30);
    ASSERT(render_get_cols() == 60);
    ASSERT(render_get_rows() == 30);
    /* New front must be zeroed */
    for (int r = 0; r < 30; r++)
        for (int c = 0; c < 60; c++)
            ASSERT(render_get_cell(c, r) == 0);
}

static void test_stats_emits_cursor_move_and_padded_text(void) {
    setup(40, 20);
    term_stub_reset();
    render_stats("FPS 60");
    /* Expect cursor move to (1,1) followed by "FPS 60" padded to 40 chars. */
    const char expected_prefix[] = "\x1b[1;1HFPS 60";
    ASSERT(g_stub_len >= sizeof expected_prefix - 1);
    ASSERT(memcmp(g_stub_buf, expected_prefix, sizeof expected_prefix - 1) == 0);
    /* Total text part (after escape) must be exactly 40 chars. */
    size_t escape_len = strlen("\x1b[1;1H");
    ASSERT(g_stub_len - escape_len == 40);
    /* Padding is spaces. */
    for (size_t i = escape_len + 6; i < g_stub_len; i++) {
        ASSERT(g_stub_buf[i] == ' ');
    }
    ASSERT(g_stub_flushes == 1);
}

static void test_stats_truncates_text_longer_than_terminal_width(void) {
    setup(20, 10);
    term_stub_reset();
    /* 30-char text in a 20-col terminal. */
    render_stats("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123");
    size_t escape_len = strlen("\x1b[1;1H");
    ASSERT(g_stub_len == escape_len + 20);
    /* First 20 chars of the input present, no overflow. */
    ASSERT(memcmp(g_stub_buf + escape_len, "ABCDEFGHIJKLMNOPQRST", 20) == 0);
}

static void test_stats_null_text_is_noop(void) {
    setup(40, 20);
    term_stub_reset();
    render_stats(NULL);
    ASSERT(g_stub_len == 0);
    ASSERT(g_stub_flushes == 0);
}

/* are not unit-tested — they would * require simulating malloc/realloc failure (LD_PRELOAD hooks, --wrap, etc.) * and are surfaced naturally via running under tight memory pressure. * * is verified by inspecting the byte count of a * typical frame — the smoke test below provides coarse coverage; the rigorous * check is in test_present_emits_and_swaps where second present == 0 bytes. * * is verified manually via * valgrind / heap-profile on the running demo. See manual-verification.md. * * is exercised by setup() calling * shutdown then init multiple times; ASan would catch double-free. */

void run_render_tests(void) {
    RUN(test_init_zeroes_buffers);
    RUN(test_init_rejects_too_small);
    RUN(test_clear_zeros_front);
    RUN(test_circle_sets_expected_dot);
    RUN(test_circle_clips_to_buffer);
    RUN(test_circle_nonpositive_radius_is_noop);
    RUN(test_circle_is_additive);
    RUN(test_present_emits_utf8_braille);
    RUN(test_present_emits_and_swaps);
    RUN(test_present_with_no_changes_skips_flush);
    RUN(test_resize_reallocates_and_zeroes);
    RUN(test_stats_emits_cursor_move_and_padded_text);
    RUN(test_stats_truncates_text_longer_than_terminal_width);
    RUN(test_stats_null_text_is_noop);
    render_shutdown();
}
