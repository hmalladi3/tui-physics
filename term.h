#ifndef TERM_H
#define TERM_H

#include <stddef.h>

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY,
    EVENT_MOUSE_CLICK,
} EventType;

typedef struct {
    EventType type;
    union {
        int key;
        struct {
            int button;
            int col;   /* 1-indexed cell column */
            int row;   /* 1-indexed cell row */
        } mouse;
    };
} Event;

int  term_init(void);
void term_shutdown(void);
void term_get_size(int* cols, int* rows);
void term_move_cursor(int col, int row);
void term_write(const char* buf, size_t len);
void term_flush(void);
int  term_quit_requested(void);
int  term_resize_pending(void);
int  term_poll_event(Event* out);

/* Pure parser, exposed for unit testing. Returns 1 if a complete event was * parsed (out written, *consumed set to bytes used). Returns 0 if no event * (incomplete sequence, leave *consumed at 0; or recognized-but-no-event like * mouse-release, *consumed set to bytes to skip). */
int  term_parse_event(const char* buf, size_t len, size_t* consumed, Event* out);

#endif
