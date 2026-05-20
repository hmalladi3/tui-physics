#include "term.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define MAX_COLS 300
#define MAX_ROWS 100
#define OUT_CAP  (1 << 18) /* 256 KiB */

static struct termios saved_termios;
static int            saved_termios_valid = 0;

static char   out_buf[OUT_CAP];
static size_t out_len = 0;

static volatile sig_atomic_t quit_requested = 0;
static volatile sig_atomic_t resize_pending = 0;

static void on_quit_signal(int signo) {
    (void)signo;
    quit_requested = 1;
}

static void on_winch(int signo) {
    (void)signo;
    resize_pending = 1;
}

static void write_all(int fd, const char* buf, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        buf += (size_t)n;
        len -= (size_t)n;
    }
}

int term_init(void) {
    if (saved_termios_valid) return 0;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "physics: stdin and stdout must both be a TTY\n");
        return -1;
    }

    struct winsize ws = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col > MAX_COLS || ws.ws_row > MAX_ROWS) {
            fprintf(stderr, "physics: terminal must be at most %dx%d (got %dx%d)\n",
                    MAX_COLS, MAX_ROWS, ws.ws_col, ws.ws_row);
            return -1;
        }
    }

    if (tcgetattr(STDIN_FILENO, &saved_termios) != 0) {
        fprintf(stderr, "physics: tcgetattr failed: %s\n", strerror(errno));
        return -1;
    }
    saved_termios_valid = 1;

    struct termios raw = saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        fprintf(stderr, "physics: tcsetattr failed: %s\n", strerror(errno));
        saved_termios_valid = 0;
        return -1;
    }

    /* Order matters: alt screen first, then cursor hide, then clear, then mouse on. */
    const char init_seq[] = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[?1000h\x1b[?1006h";
    write_all(STDOUT_FILENO, init_seq, sizeof init_seq - 1);

    atexit(term_shutdown);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_quit_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);

    struct sigaction sw;
    memset(&sw, 0, sizeof sw);
    sw.sa_handler = on_winch;
    sigemptyset(&sw.sa_mask);
    sw.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sw, NULL);

    return 0;
}

void term_shutdown(void) {
    if (!saved_termios_valid) return;
    /* Order: mouse off first, then show cursor, then leave alt screen. */
    const char end_seq[] = "\x1b[?1006l\x1b[?1000l\x1b[?25h\x1b[?1049l";
    write_all(STDOUT_FILENO, end_seq, sizeof end_seq - 1);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    saved_termios_valid = 0;
}

void term_get_size(int* cols, int* rows) {
    struct winsize ws = {0};
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    *cols = ws.ws_col;
    *rows = ws.ws_row;
}

void term_move_cursor(int col, int row) {
    char buf[32];
    int  n = snprintf(buf, sizeof buf, "\x1b[%d;%dH", row, col);
    if (n > 0) term_write(buf, (size_t)n);
}

void term_write(const char* buf, size_t len) {
    if (out_len + len > OUT_CAP) {
        fprintf(stderr, "physics: terminal output buffer overflow\n");
        abort();
    }
    memcpy(out_buf + out_len, buf, len);
    out_len += len;
}

void term_flush(void) {
    if (out_len == 0) return;
    write_all(STDOUT_FILENO, out_buf, out_len);
    out_len = 0;
}

int term_quit_requested(void) {
    return __atomic_load_n(&quit_requested, __ATOMIC_RELAXED);
}

int term_resize_pending(void) {
    return __atomic_exchange_n(&resize_pending, 0, __ATOMIC_RELAXED);
}

int term_poll_event(Event* out) {
    static char   input_buf[64];
    static size_t input_buf_len = 0;

    if (input_buf_len < sizeof input_buf) {
        ssize_t n = read(STDIN_FILENO, input_buf + input_buf_len,
                         sizeof input_buf - input_buf_len);
        if (n > 0) input_buf_len += (size_t)n;
    }

    if (input_buf_len == 0) {
        out->type = EVENT_NONE;
        return 0;
    }

    size_t consumed = 0;
    int    got      = term_parse_event(input_buf, input_buf_len, &consumed, out);

    if (consumed > 0) {
        if (consumed > input_buf_len) consumed = input_buf_len;
        memmove(input_buf, input_buf + consumed, input_buf_len - consumed);
        input_buf_len -= consumed;
    }

    if (got) return 1;
    out->type = EVENT_NONE;
    return 0;
}
