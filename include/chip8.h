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

/* The six places the platform dialects disagree. Each flag picks the original
 * COSMAC VIP behaviour when set and the later Chip-48 behaviour when clear. */
typedef struct {
    bool vf_reset;      /* 8xy1/2/3 clear VF after the operation */
    bool mem_inc;       /* Fx55/Fx65 advance I by x + 1 */
    bool shift_vy;      /* 8xy6/8xyE shift Vy and store in Vx */
    bool jump_v0;       /* Bnnn adds V0 instead of Vx */
    bool clip;          /* Dxyn clips at the edges instead of wrapping */
    bool display_wait;  /* Dxyn stalls the rest of the frame */
} Quirks;

typedef enum {
    QUIRKS_VIP = 0,     /* original COSMAC VIP: every quirk on */
    QUIRKS_MODERN,      /* Chip-48: no VF reset, in-place shifts, I stays */
} QuirkPreset;

typedef struct {
    uint8_t  mem[MEM_SIZE];
    uint8_t  regs[16];
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
    Quirks   quirks;
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

void chip8_set_quirks(Chip8 *c, QuirkPreset preset);
const char *chip8_quirk_name(QuirkPreset preset);
bool chip8_quirk_parse(const char *name, QuirkPreset *out);

size_t chip8_state_size(void);
bool chip8_state_save(const Chip8 *c, uint8_t *buf, size_t len);
bool chip8_state_load(Chip8 *c, const uint8_t *buf, size_t len);

#endif
