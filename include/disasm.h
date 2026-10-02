#ifndef DISASM_H
#define DISASM_H

#include "chip8.h"

#include <stddef.h>
#include <stdio.h>

/* Render the instruction at addr into buf (NUL-terminated) and return the
 * number of bytes it occupies, always 2. Unknown opcodes render as DW. */
int disasm_line(const Chip8 *c, uint16_t addr, char *buf, size_t len);

/* Disassemble bytes bytes starting at base, one line per instruction. */
void disasm_rom(const Chip8 *c, uint16_t base, size_t bytes, FILE *out);

#endif
