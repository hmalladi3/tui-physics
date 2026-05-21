#include "../term.h"
#include "test.h"

#include <string.h>

/* The pure parser (term_parse_event) IS unit-testable; the I/O side of the * terminal driver (raw mode, alt screen, signals, ioctl) still requires * manual verification — see docs/specs/manual-verification.md. */

static void test_parse_plain_key(void) {
    Event  ev;
    size_t consumed;
    int    got = term_parse_event("q", 1, &consumed, &ev);
    ASSERT(got == 1);
    ASSERT(consumed == 1);
    ASSERT(ev.type == EVENT_KEY);
    ASSERT(ev.key == 'q');
}

static void test_parse_empty_buffer(void) {
    Event  ev;
    size_t consumed = 999;
    int    got = term_parse_event("", 0, &consumed, &ev);
    ASSERT(got == 0);
    ASSERT(consumed == 0);
}

static void test_parse_incomplete_escape_returns_no_event(void) {
    Event  ev;
    size_t consumed;
    int    got = term_parse_event("\x1b", 1, &consumed, &ev);
    ASSERT(got == 0);
    ASSERT(consumed == 0);
    /* \x1b[<0;5;5 — no terminator yet */
    got = term_parse_event("\x1b[<0;5;5", 8, &consumed, &ev);
    ASSERT(got == 0);
    ASSERT(consumed == 0);
}

static void test_parse_sgr_left_click(void) {
    Event  ev;
    size_t consumed;
    const char input[] = "\x1b[<0;42;13M";
    int got = term_parse_event(input, sizeof input - 1, &consumed, &ev);
    ASSERT(got == 1);
    ASSERT(consumed == sizeof input - 1);
    ASSERT(ev.type == EVENT_MOUSE_CLICK);
    ASSERT(ev.mouse.button == 0);
    ASSERT(ev.mouse.col == 42);
    ASSERT(ev.mouse.row == 13);
}

static void test_parse_sgr_right_click_carries_button(void) {
    Event  ev;
    size_t consumed;
    const char input[] = "\x1b[<2;7;3M";
    int got = term_parse_event(input, sizeof input - 1, &consumed, &ev);
    ASSERT(got == 1);
    ASSERT(ev.mouse.button == 2);
    ASSERT(ev.mouse.col == 7);
    ASSERT(ev.mouse.row == 3);
}

static void test_parse_sgr_release_consumed_no_event(void) {
    Event  ev;
    size_t consumed;
    const char input[] = "\x1b[<0;42;13m";  /* lowercase m = release */
    int got = term_parse_event(input, sizeof input - 1, &consumed, &ev);
    ASSERT(got == 0);
    ASSERT(consumed == sizeof input - 1);
}

static void test_parse_non_sgr_csi_skipped(void) {
    Event  ev;
    size_t consumed;
    /* Arrow-up: \x1b[A */
    const char input[] = "\x1b[A";
    int got = term_parse_event(input, sizeof input - 1, &consumed, &ev);
    ASSERT(got == 0);
    ASSERT(consumed == 3);
}

/* * bytes after an SGR event should be left for the next call */
static void test_parse_leaves_trailing_bytes(void) {
    Event  ev;
    size_t consumed;
    const char input[] = "\x1b[<0;1;1Mq";
    int got = term_parse_event(input, sizeof input - 1, &consumed, &ev);
    ASSERT(got == 1);
    ASSERT(consumed == sizeof input - 2);  /* SGR event, leaving 'q' */
    ASSERT(ev.type == EVENT_MOUSE_CLICK);
}

void run_term_tests(void) {
    RUN(test_parse_empty_buffer);
    RUN(test_parse_plain_key);
    RUN(test_parse_incomplete_escape_returns_no_event);
    RUN(test_parse_sgr_left_click);
    RUN(test_parse_sgr_right_click_carries_button);
    RUN(test_parse_sgr_release_consumed_no_event);
    RUN(test_parse_non_sgr_csi_skipped);
    RUN(test_parse_leaves_trailing_bytes);
    printf("    (I/O side: manual verification — see docs/specs/manual-verification.md)\n");
}
