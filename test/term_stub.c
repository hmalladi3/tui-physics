/* Test-only stub of term.h. Captures emitted bytes into a buffer for inspection * by render tests. Links into the test binary in place of the real term.c. */

#include "../term.h"

#include <stdio.h>
#include <string.h>

#define STUB_CAP (1 << 16)

char   g_stub_buf[STUB_CAP];
size_t g_stub_len    = 0;
int    g_stub_flushes = 0;

void term_stub_reset(void) {
    g_stub_len     = 0;
    g_stub_flushes = 0;
}

int  term_init(void)                    { return 0; }
void term_shutdown(void)                { }
void term_get_size(int* c, int* r)      { *c = 80; *r = 24; }
int  term_quit_requested(void)          { return 0; }
int  term_resize_pending(void)          { return 0; }

int term_poll_event(Event* out) {
    out->type = EVENT_NONE;
    return 0;
}

/* term_parse_event lives in term_parse.c and is linked into the test binary. */

void term_move_cursor(int col, int row) {
    char buf[32];
    int  n = snprintf(buf, sizeof buf, "\x1b[%d;%dH", row, col);
    if (n > 0 && g_stub_len + (size_t)n < STUB_CAP) {
        memcpy(g_stub_buf + g_stub_len, buf, (size_t)n);
        g_stub_len += (size_t)n;
    }
}

void term_write(const char* buf, size_t len) {
    if (g_stub_len + len < STUB_CAP) {
        memcpy(g_stub_buf + g_stub_len, buf, len);
        g_stub_len += len;
    }
}

void term_flush(void) {
    g_stub_flushes++;
}
