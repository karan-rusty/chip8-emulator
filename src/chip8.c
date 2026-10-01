#include "chip8.h"

#include <string.h>

static const uint8_t font[FONT_BYTES] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,
    0x20, 0x60, 0x20, 0x20, 0x70,
    0xF0, 0x10, 0xF0, 0x80, 0xF0,
    0xF0, 0x10, 0xF0, 0x10, 0xF0,
    0x90, 0x90, 0xF0, 0x10, 0x10,
    0xF0, 0x80, 0xF0, 0x10, 0xF0,
    0xF0, 0x80, 0xF0, 0x90, 0xF0,
    0xF0, 0x10, 0x20, 0x40, 0x40,
    0xF0, 0x90, 0xF0, 0x90, 0xF0,
    0xF0, 0x90, 0xF0, 0x10, 0xF0,
    0xF0, 0x90, 0xF0, 0x90, 0x90,
    0xE0, 0x90, 0xE0, 0x90, 0xE0,
    0xF0, 0x80, 0x80, 0x80, 0xF0,
    0xE0, 0x90, 0x90, 0x90, 0xE0,
    0xF0, 0x80, 0xF0, 0x80, 0xF0,
    0xF0, 0x80, 0xF0, 0x80, 0x80,
};

void chip8_init(Chip8 *c)
{
    memset(c, 0, sizeof(*c));
    c->pc = PC_START;
    c->rng = 0x2545F491u;
    memcpy(c->mem + FONT_BASE, font, sizeof(font));
}

void chip8_seed(Chip8 *c, uint32_t seed)
{
    c->rng = seed ? seed : 1u;
}

bool chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len,
                    bool *truncated)
{
    if (len == 0)
        return false;

    if (truncated)
        *truncated = false;
    if (len > PROG_MAX) {
        len = PROG_MAX;
        if (truncated)
            *truncated = true;
    }

    memcpy(c->mem + PROG_BASE, data, len);
    return true;
}

void chip8_set_key(Chip8 *c, unsigned key, bool down)
{
    if (key < NUM_KEYS)
        c->keys[key] = down;
}

void chip8_tick_timers(Chip8 *c)
{
    if (c->dt > 0)
        c->dt--;
    if (c->st > 0)
        c->st--;

    c->vblank = false;
}

bool chip8_sound(const Chip8 *c)
{
    return c->st > 0;
}

uint16_t chip8_fetch(const Chip8 *c)
{
    uint8_t hi = c->mem[c->pc & 0x0FFF];
    uint8_t lo = c->mem[(c->pc + 1) & 0x0FFF];
    return (uint16_t)((hi << 8) | lo);
}

void chip8_run(Chip8 *c, int cycles)
{
    for (int i = 0; i < cycles && !c->vblank; i++)
        chip8_execute(c, chip8_fetch(c));
}
