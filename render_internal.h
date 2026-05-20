#ifndef RENDER_INTERNAL_H
#define RENDER_INTERNAL_H

/* Test-only introspection. Production code must not include this header. */

int render_get_cell(int col, int row);
int render_get_cols(void);
int render_get_rows(void);

#endif
