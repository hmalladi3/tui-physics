#include "term.h"

/* Pure parser, no I/O. Extracted so tests can link the real implementation * without pulling in the rest of term.c (which depends on termios, signals, * stdio, etc.). */

int term_parse_event(const char* buf, size_t len, size_t* consumed, Event* out) {
    *consumed = 0;
    if (len == 0) return 0;

    unsigned char c0 = (unsigned char)buf[0];
    if (c0 != 0x1B) {
        out->type = EVENT_KEY;
        out->key  = (int)c0;
        *consumed = 1;
        return 1;
    }

    if (len < 2) return 0;
    if (buf[1] != '[') {
        *consumed = 1;
        return 0;
    }
    if (len < 3) return 0;

    if (buf[2] != '<') {
        /* Non-SGR CSI — scan for terminator (0x40..0x7E) starting from buf[2]. */
        for (size_t i = 2; i < len; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (c >= 0x40 && c <= 0x7E) {
                *consumed = i + 1;
                return 0;
            }
        }
        return 0;
    }

    /* SGR mouse: '\x1b[<' digits ';' digits ';' digits ('M' | 'm') */
    int parts[3]  = {0, 0, 0};
    int part_idx  = 0;
    int has_digit = 0;
    for (size_t i = 3; i < len; i++) {
        char c = buf[i];
        if (c == 'M' || c == 'm') {
            if (part_idx != 2 || !has_digit) {
                *consumed = i + 1;
                return 0;
            }
            if (c == 'm') {
                *consumed = i + 1;
                return 0;
            }
            out->type         = EVENT_MOUSE_CLICK;
            out->mouse.button = parts[0];
            out->mouse.col    = parts[1];
            out->mouse.row    = parts[2];
            *consumed         = i + 1;
            return 1;
        }
        if (c == ';') {
            if (part_idx >= 2) {
                *consumed = i + 1;
                return 0;
            }
            part_idx++;
            has_digit = 0;
            continue;
        }
        if (c >= '0' && c <= '9') {
            parts[part_idx] = parts[part_idx] * 10 + (c - '0');
            has_digit       = 1;
            continue;
        }
        *consumed = i + 1;
        return 0;
    }
    return 0;
}
