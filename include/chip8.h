#ifndef CHIP8_H
#define CHIP8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEM_SIZE 4096
#define FB_W     64
#define FB_H     32
#define FB_SIZE  (FB_W * FB_H)

#define STACK_SIZE 16
#define PC_START   0x200
#define FONT_GLYPH 0x020

/* Memory map. 0x000-0x1FF is reserved for the interpreter; this build puts
 * the hex font at 0x000 so that Fx29's "I = digit * 5" lands exactly on a
 * glyph, which is what ROMs assume. Programs load at PROG_BASE and may occupy
 * everything up to the end of memory, so Fx55/Fx65 can legitimately move data
 * into what the comment above calls font space. */
#define FONT_BASE  0x000
#define FONT_H     5      /* rows per glyph */
#define FONT_COUNT 16     /* 0-9 and A-F */
#define FONT_BYTES (FONT_COUNT * FONT_H)
#define FONT_END   (FONT_BASE + FONT_BYTES)

#define PROG_BASE 0x200

/* Bytes available to a loaded program. */
#define PROG_MAX (MEM_SIZE - PROG_BASE)

typedef struct {
    uint8_t  mem[MEM_SIZE];
    uint16_t regs[16];
    uint16_t pc;
    uint16_t i;
    uint8_t  sp;
    uint16_t stack[STACK_SIZE];
    bool     fb[FB_SIZE];
    uint8_t  dt;
    uint8_t  st;
} Chip8;

void chip8_init(Chip8 *c);

/* Returns false and leaves `c` untouched if `len` is 0. A ROM longer than
 * PROG_MAX is truncated and `*truncated` is set, so the caller can warn. */
bool chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len,
                    bool *truncated);
uint16_t chip8_fetch(const Chip8 *c);
void chip8_execute(Chip8 *c, uint16_t opcode);
void chip8_run(Chip8 *c, int cycles);

#endif
