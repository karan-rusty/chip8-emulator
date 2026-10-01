#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>

#include "chip8.h"

bool term_open(void);

void term_close(void);

void term_poll(Chip8 *c, bool *quit);

void term_draw(const Chip8 *c);

#endif
