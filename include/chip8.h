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

/* Memory map. 0x000-0x1FF is reserved for the interpreter; this build puts
 * the hex font at 0x000 so that Fx29's "I = FONT_BASE + digit * 5" lands on
 * a glyph, which is what ROMs assume. Programs load at PROG_BASE and may
 * occupy everything up to the end of memory, so Fx55/Fx65 can legitimately
 * move data into what the comment above calls font space. */
#define FONT_BASE  0x000
#define FONT_H     5      /* rows per glyph */
#define FONT_COUNT 16     /* 0-9 and A-F */
#define FONT_BYTES (FONT_COUNT * FONT_H)
#define FONT_END   (FONT_BASE + FONT_BYTES)

#define PROG_BASE 0x200

/* Bytes available to a loaded program. */
#define PROG_MAX (MEM_SIZE - PROG_BASE)

/* The keypad is 16 keys, one per hex digit. */
#define NUM_KEYS 16

typedef struct {
    uint8_t  mem[MEM_SIZE];
    uint16_t regs[16];    /* V0-VF; every write masks to 8 bits */
    uint16_t pc;
    uint16_t i;
    uint8_t  sp;
    uint16_t stack[STACK_SIZE];
    bool     fb[FB_SIZE];
    bool     keys[NUM_KEYS];
    uint8_t  dt;          /* delay timer, decremented at 60Hz */
    uint8_t  st;          /* sound timer, decremented at 60Hz */
    uint32_t rng;         /* xorshift32 state for Cxkk; never 0 */
    bool     vblank;      /* set by Dxyn, cleared by chip8_tick_timers */
} Chip8;

void chip8_init(Chip8 *c);

/* Reseed Cxkk. Passing 0 is allowed and becomes 1, because xorshift must not
 * run from an all-zero state. */
void chip8_seed(Chip8 *c, uint32_t seed);

/* Returns false and leaves `c` untouched if `len` is 0. A ROM longer than
 * PROG_MAX is truncated and `*truncated` is set, so the caller can warn. */
bool chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len,
                    bool *truncated);

uint16_t chip8_fetch(const Chip8 *c);
void chip8_execute(Chip8 *c, uint16_t opcode);
void chip8_run(Chip8 *c, int cycles);

/* Host-side input and clocking. The core never reads stdin or wall time; the
 * frontend pushes both in. */
void chip8_set_key(Chip8 *c, unsigned key, bool down);
void chip8_tick_timers(Chip8 *c);   /* call at 60Hz, not once per opcode */
bool chip8_sound(const Chip8 *c);   /* true while the buzzer should sound */

#endif
