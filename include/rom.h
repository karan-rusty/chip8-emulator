#ifndef ROM_H
#define ROM_H

#include "chip8.h"

int rom_load(Chip8 *c, const char *path);
void rom_dump(const Chip8 *c);

#endif
