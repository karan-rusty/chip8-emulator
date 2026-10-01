#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>

#include "chip8.h"

/* Frontend for the interpreter built out of nothing but POSIX and ANSI
 * escapes: raw termios for input, direct escape sequences for output. No
 * SDL, no ncurses, no dependency beyond libc. */

/* Switches stdin out of canonical mode and clears the screen. Returns false
 * if stdin is not a terminal, in which case nothing was changed. */
bool term_open(void);

/* Puts the tty back the way it was and shows the cursor again. Safe to call
 * even if term_open() failed or never ran, so it can be registered with
 * atexit(). */
void term_close(void);

/* Drains stdin onto the CHIP-8 keypad and expires keys from earlier frames.
 * Sets *quit when the user asks to leave. */
void term_poll(Chip8 *c, bool *quit);

/* Repaints the whole framebuffer at the top of the screen. */
void term_draw(const Chip8 *c);

#endif
