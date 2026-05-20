#ifndef RENDER_H
#define RENDER_H

int  render_init(int cols, int rows);
void render_shutdown(void);
void render_resize(int cols, int rows);
void render_clear(void);
void render_circle(float cx, float cy, float radius);
void render_present(void);

/* Emit a one-line ASCII stats overlay at terminal row 1, padded with spaces to * the full terminal width and truncated if longer. Bypasses the Braille frame * diff path — the caller (main) decides cadence. */
void render_stats(const char* text);

#endif
