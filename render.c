#include "render.h"
#include "term.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef RENDER_TEST_INSPECT
#include "render_internal.h"
#endif

#define MIN_COLS 20
#define MIN_ROWS 10

typedef struct {
    uint8_t* cells;
    int      cols;
    int      rows;
} BrailleBuf;

static BrailleBuf front;
static BrailleBuf back;

/* Unicode Braille dot-to-bit mapping. in_col in [0,1], in_row in [0,3]. * 1 4 bit 0 bit 3 * 2 5 bit 1 bit 4 * 3 6 bit 2 bit 5 * 7 8 bit 6 bit 7 */
static const uint8_t bit_for[2][4] = {
    {0, 1, 2, 6},
    {3, 4, 5, 7},
};

static int alloc_buf(BrailleBuf* b, int cols, int rows) {
    uint8_t* cells = calloc((size_t)(cols * rows), 1);
    if (!cells) return -1;
    free(b->cells);
    b->cells = cells;
    b->cols  = cols;
    b->rows  = rows;
    return 0;
}

int render_init(int cols, int rows) {
    if (cols < MIN_COLS || rows < MIN_ROWS) {
        fprintf(stderr, "physics: terminal too small (min %dx%d, got %dx%d)\n",
                MIN_COLS, MIN_ROWS, cols, rows);
        return -1;
    }
    if (alloc_buf(&front, cols, rows) != 0) return -1;
    if (alloc_buf(&back,  cols, rows) != 0) {
        free(front.cells);
        front.cells = NULL;
        front.cols = front.rows = 0;
        return -1;
    }
    return 0;
}

void render_shutdown(void) {
    free(front.cells);
    front.cells = NULL;
    front.cols  = 0;
    front.rows  = 0;
    free(back.cells);
    back.cells = NULL;
    back.cols  = 0;
    back.rows  = 0;
}

void render_resize(int cols, int rows) {
    size_t   sz = (size_t)(cols * rows);
    uint8_t* nf = realloc(front.cells, sz);
    uint8_t* nb = realloc(back.cells,  sz);
    if (!nf || !nb) {
        fprintf(stderr, "physics: render_resize allocation failed\n");
        abort();
    }
    front.cells = nf;
    front.cols  = cols;
    front.rows  = rows;
    back.cells  = nb;
    back.cols   = cols;
    back.rows   = rows;
    memset(front.cells, 0, sz);
    memset(back.cells,  0, sz);
}

void render_clear(void) {
    memset(front.cells, 0, (size_t)(front.cols * front.rows));
}

void render_circle(float cx, float cy, float radius) {
    if (radius <= 0.0f) return;

    int px_min = (int)(cx - radius);
    int py_min = (int)(cy - radius);
    int px_max = (int)(cx + radius) + 1;
    int py_max = (int)(cy + radius) + 1;
    int x_lim  = front.cols * 2;
    int y_lim  = front.rows * 4;

    if (px_min < 0)      px_min = 0;
    if (py_min < 0)      py_min = 0;
    if (px_max > x_lim)  px_max = x_lim;
    if (py_max > y_lim)  py_max = y_lim;

    float r2 = radius * radius;
    for (int py = py_min; py < py_max; py++) {
        for (int px = px_min; px < px_max; px++) {
            float dx = (float)px - cx;
            float dy = (float)py - cy;
            if (dx * dx + dy * dy <= r2) {
                int cell_col = px >> 1;
                int cell_row = py >> 2;
                int in_col   = px & 1;
                int in_row   = py & 3;
                front.cells[cell_row * front.cols + cell_col]
                    |= (uint8_t)(1u << bit_for[in_col][in_row]);
            }
        }
    }
}

static void emit_run(int row, int col_start, int col_end) {
    char buf[32];
    int  n = snprintf(buf, sizeof buf, "\x1b[%d;%dH", row + 1, col_start + 1);
    if (n > 0) term_write(buf, (size_t)n);

    for (int c = col_start; c < col_end; c++) {
        uint8_t p = front.cells[row * front.cols + c];
        char    glyph[3];
        glyph[0] = (char)0xE2;
        glyph[1] = (char)(0xA0 | (p >> 6));
        glyph[2] = (char)(0x80 | (p & 0x3F));
        term_write(glyph, 3);
    }
}

void render_present(void) {
    int changed_any = 0;
    for (int r = 0; r < front.rows; r++) {
        int run_start = -1;
        for (int c = 0; c < front.cols; c++) {
            int i = r * front.cols + c;
            if (front.cells[i] != back.cells[i]) {
                if (run_start < 0) run_start = c;
                changed_any = 1;
            } else if (run_start >= 0) {
                emit_run(r, run_start, c);
                run_start = -1;
            }
        }
        if (run_start >= 0) {
            emit_run(r, run_start, front.cols);
        }
    }

    if (changed_any) {
        memcpy(back.cells, front.cells, (size_t)(front.cols * front.rows));
        term_flush();
    }
}

void render_stats(const char* text) {
    if (front.cols <= 0 || text == NULL) return;
    int  cap = front.cols < 500 ? front.cols : 500;
    char buf[640];
    int  n = snprintf(buf, sizeof buf, "\x1b[1;1H%-*.*s", cap, cap, text);
    if (n > 0) term_write(buf, (size_t)n);
    term_flush();
}

#ifdef RENDER_TEST_INSPECT
int render_get_cell(int col, int row) {
    if (col < 0 || col >= front.cols || row < 0 || row >= front.rows) return -1;
    return front.cells[row * front.cols + col];
}
int render_get_cols(void) { return front.cols; }
int render_get_rows(void) { return front.rows; }
#endif
