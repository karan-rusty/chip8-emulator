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

static const Quirks quirks_vip = {
    .vf_reset     = true,
    .mem_inc      = true,
    .shift_vy     = true,
    .jump_v0      = true,
    .clip         = true,
    .display_wait = true,
};

static const Quirks quirks_modern = {
    .vf_reset     = false,
    .mem_inc      = false,
    .shift_vy     = false,
    .jump_v0      = false,
    .clip         = true,
    .display_wait = false,
};

void chip8_init(Chip8 *c)
{
    memset(c, 0, sizeof(*c));
    c->pc = PC_START;
    c->rng = 0x2545F491u;
    c->quirks = quirks_vip;
    memcpy(c->mem + FONT_BASE, font, sizeof(font));
}

void chip8_seed(Chip8 *c, uint32_t seed)
{
    c->rng = seed ? seed : 1u;
}

void chip8_set_quirks(Chip8 *c, QuirkPreset preset)
{
    c->quirks = preset == QUIRKS_MODERN ? quirks_modern : quirks_vip;
}

const char *chip8_quirk_name(QuirkPreset preset)
{
    return preset == QUIRKS_MODERN ? "modern" : "vip";
}

bool chip8_quirk_parse(const char *name, QuirkPreset *out)
{
    if (!name || !out)
        return false;

    if (strcmp(name, "vip") == 0 || strcmp(name, "original") == 0) {
        *out = QUIRKS_VIP;
        return true;
    }
    if (strcmp(name, "modern") == 0 || strcmp(name, "chip48") == 0) {
        *out = QUIRKS_MODERN;
        return true;
    }
    return false;
}

bool chip8_load_rom(Chip8 *c, const uint8_t *data, size_t len,
                    bool *truncated)
{
    if (len == 0 || data == NULL)
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

/* ---- state snapshot ---- */

#define STATE_MAGIC   "C8ST"
#define STATE_MLEN    4
#define STATE_VERSION 1u

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static uint8_t quirks_pack(const Quirks *q)
{
    return (uint8_t)((q->vf_reset     ? 0x01u : 0u) |
                     (q->mem_inc      ? 0x02u : 0u) |
                     (q->shift_vy     ? 0x04u : 0u) |
                     (q->jump_v0      ? 0x08u : 0u) |
                     (q->clip         ? 0x10u : 0u) |
                     (q->display_wait ? 0x20u : 0u));
}

static void quirks_unpack(Quirks *q, uint8_t bits)
{
    q->vf_reset     = (bits & 0x01u) != 0;
    q->mem_inc      = (bits & 0x02u) != 0;
    q->shift_vy     = (bits & 0x04u) != 0;
    q->jump_v0      = (bits & 0x08u) != 0;
    q->clip         = (bits & 0x10u) != 0;
    q->display_wait = (bits & 0x20u) != 0;
}

size_t chip8_state_size(void)
{
    return STATE_MLEN + 2 + MEM_SIZE + 16 + 2 + 2 + 1
         + STACK_SIZE * 2 + FB_SIZE + NUM_KEYS + 1 + 1 + 4 + 1;
}

bool chip8_state_save(const Chip8 *c, uint8_t *buf, size_t len)
{
    if (!buf || len < chip8_state_size())
        return false;

    uint8_t *p = buf;
    memcpy(p, STATE_MAGIC, STATE_MLEN);
    p += STATE_MLEN;
    *p++ = STATE_VERSION;
    *p++ = quirks_pack(&c->quirks);

    memcpy(p, c->mem, MEM_SIZE);
    p += MEM_SIZE;
    memcpy(p, c->regs, 16);
    p += 16;
    put16(p, c->pc);
    p += 2;
    put16(p, c->i);
    p += 2;
    *p++ = c->sp;
    for (int k = 0; k < STACK_SIZE; k++) {
        put16(p, c->stack[k]);
        p += 2;
    }
    for (int k = 0; k < FB_SIZE; k++)
        *p++ = c->fb[k] ? 1 : 0;
    for (int k = 0; k < NUM_KEYS; k++)
        *p++ = c->keys[k] ? 1 : 0;
    *p++ = c->dt;
    *p++ = c->st;
    put32(p, c->rng);
    p += 4;
    *p++ = c->vblank ? 1 : 0;

    return true;
}

bool chip8_state_load(Chip8 *c, const uint8_t *buf, size_t len)
{
    if (!buf || len < chip8_state_size())
        return false;
    if (memcmp(buf, STATE_MAGIC, STATE_MLEN) != 0)
        return false;
    if (buf[STATE_MLEN] != STATE_VERSION)
        return false;

    const uint8_t *p = buf + STATE_MLEN + 1;
    quirks_unpack(&c->quirks, *p++);

    memcpy(c->mem, p, MEM_SIZE);
    p += MEM_SIZE;
    memcpy(c->regs, p, 16);
    p += 16;
    c->pc = get16(p);
    p += 2;
    c->i = get16(p);
    p += 2;
    c->sp = *p++;
    for (int k = 0; k < STACK_SIZE; k++) {
        c->stack[k] = get16(p);
        p += 2;
    }
    for (int k = 0; k < FB_SIZE; k++)
        c->fb[k] = *p++ != 0;
    for (int k = 0; k < NUM_KEYS; k++)
        c->keys[k] = *p++ != 0;
    c->dt = *p++;
    c->st = *p++;
    c->rng = get32(p);
    p += 4;
    c->vblank = *p++ != 0;

    return true;
}
