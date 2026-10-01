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

#define FONT_BASE  0x000
#define FONT_H     5
#define FONT_COUNT 16
#define FONT_BYTES (FONT_COUNT * FONT_H)
#define FONT_END   (FONT_BASE + FONT_BYTES)

#define PROG_BASE 0x200

#define PROG_MAX (MEM_SIZE - PROG_BASE)

#define NUM_KEYS 16

typedef struct {
    uint8_t  mem[MEM_SIZE];
    uint16_t regs[16];
    uint16_t pc;
    uint16_t i;
    uint8_t  sp;
    uint16_t stack[STACK_SIZE];
    bool     fb[FB_SIZE];
    bool     keys[NUM_KEYS];
    uint8_t  dt;
    uint8_t  st;
    uint32_t rng;
    bool     vblank;         /* set by Dxyn, cleared by chip8_tick_timers */
} Chip8;

void chip8_init(Chip8 *c);

void chip8_seed(Chip8 *c, uint32_t seed);

bool chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len,
                    bool *truncated);

uint16_t chip8_fetch(const Chip8 *c);
void chip8_execute(Chip8 *c, uint16_t opcode);
void chip8_run(Chip8 *c, int cycles);

void chip8_set_key(Chip8 *c, unsigned key, bool down);
void chip8_tick_timers(Chip8 *c);   /* call at 60Hz, not once per opcode */
bool chip8_sound(const Chip8 *c);

#endif
