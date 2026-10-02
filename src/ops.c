#include "chip8.h"

#include <string.h>

static void push(Chip8 *c, uint16_t addr)
{
    if (c->sp < STACK_SIZE)
        c->stack[c->sp++] = addr;
    else if (c->sp < UINT8_MAX)
        c->sp++;
}

static uint32_t rand32(Chip8 *c)
{
    uint32_t x = c->rng;
    if (x == 0)
        x = 0x2545F491u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    c->rng = x;
    return x;
}

static bool key_pressed(const Chip8 *c, uint8_t vx)
{
    return vx < NUM_KEYS && c->keys[vx];
}

void chip8_execute(Chip8 *c, uint16_t opcode)
{
    uint16_t x   = (opcode >> 8) & 0x0F;
    uint16_t y   = (opcode >> 4) & 0x0F;
    uint16_t nnn = opcode & 0x0FFF;
    uint16_t kk  = opcode & 0x00FF;
    uint16_t n   = opcode & 0x000F;
    int step = 2;

    switch (opcode & 0xF000) {
    case 0x0000:
        if (opcode == 0x00E0) {
            memset(c->fb, 0, sizeof c->fb);
        } else if (opcode == 0x00EE) {
            if (c->sp > STACK_SIZE)
                c->sp = STACK_SIZE;
            if (c->sp > 0) {
                c->pc = c->stack[--c->sp];
                return;
            }
        }
        break;

    case 0x1000:
        c->pc = nnn;
        return;

    case 0x2000:
        push(c, (uint16_t)(c->pc + 2));
        c->pc = nnn;
        return;

    case 0x3000:
        if (c->regs[x] == kk)
            step = 4;
        break;

    case 0x4000:
        if (c->regs[x] != kk)
            step = 4;
        break;

    case 0x5000:
        if (n == 0 && c->regs[x] == c->regs[y])
            step = 4;
        break;

    case 0x6000:
        c->regs[x] = (uint8_t)kk;
        break;

    case 0x7000:
        c->regs[x] = (uint8_t)((c->regs[x] + kk) & 0xFF);
        break;

    case 0x8000: {
        uint8_t vx = c->regs[x];
        uint8_t vy = c->regs[y];
        switch (n) {
        case 0x0:
            c->regs[x] = vy;
            break;
        case 0x1:
            c->regs[x] = (uint8_t)(vx | vy);
            if (c->quirks.vf_reset)
                c->regs[0xF] = 0;
            break;
        case 0x2:
            c->regs[x] = (uint8_t)(vx & vy);
            if (c->quirks.vf_reset)
                c->regs[0xF] = 0;
            break;
        case 0x3:
            c->regs[x] = (uint8_t)(vx ^ vy);
            if (c->quirks.vf_reset)
                c->regs[0xF] = 0;
            break;
        case 0x4:
            c->regs[x] = (uint8_t)((vx + vy) & 0xFF);
            c->regs[0xF] = (unsigned)vx + vy > 0xFF ? 1 : 0;
            break;
        case 0x5:
            c->regs[x] = (uint8_t)((vx - vy) & 0xFF);
            c->regs[0xF] = vx >= vy ? 1 : 0;
            break;
        case 0x6: {
            uint8_t src = c->quirks.shift_vy ? vy : vx;
            c->regs[x] = (uint8_t)(src >> 1);
            c->regs[0xF] = (uint8_t)(src & 1);
            break;
        }
        case 0x7:
            c->regs[x] = (uint8_t)((vy - vx) & 0xFF);
            c->regs[0xF] = vy >= vx ? 1 : 0;
            break;
        case 0xE: {
            uint8_t src = c->quirks.shift_vy ? vy : vx;
            c->regs[x] = (uint8_t)(src << 1);
            c->regs[0xF] = (uint8_t)((src >> 7) & 1);
            break;
        }
        default:
            break;
        }
        break;
    }

    case 0x9000:
        if (n == 0 && c->regs[x] != c->regs[y])
            step = 4;
        break;

    case 0xA000:
        c->i = nnn;
        break;

    case 0xB000:
        c->pc = (uint16_t)((nnn + (c->quirks.jump_v0 ? c->regs[0]
                                                      : c->regs[x])) & 0x0FFF);
        return;

    case 0xC000:
        c->regs[x] = (uint8_t)(rand32(c) & kk);
        break;

    case 0xD000: {
        int x0 = c->regs[x] % FB_W;
        int y0 = c->regs[y] % FB_H;
        bool collision = false;
        bool clip = c->quirks.clip;

        for (int row = 0; row < n; row++) {
            int py = clip ? y0 + row : (y0 + row) % FB_H;
            if (clip && py >= FB_H)
                break;
            uint8_t bits = c->mem[(c->i + row) & 0x0FFF];
            for (int bit = 0; bit < 8; bit++) {
                int px = clip ? x0 + bit : (x0 + bit) % FB_W;
                if (clip && px >= FB_W)
                    break;
                if (!(bits & (0x80 >> bit)))
                    continue;
                int idx = py * FB_W + px;
                if (c->fb[idx])
                    collision = true;
                c->fb[idx] = !c->fb[idx];
            }
        }

        c->regs[0xF] = collision ? 1 : 0;
        if (c->quirks.display_wait)
            c->vblank = true;
        break;
    }

    case 0xE000:
        if (kk == 0x9E) {
            if (key_pressed(c, c->regs[x]))
                step = 4;
        } else if (kk == 0xA1) {
            if (!key_pressed(c, c->regs[x]))
                step = 4;
        }
        break;

    case 0xF000: {
        uint8_t vx = c->regs[x];
        switch (kk) {
        case 0x07:
            c->regs[x] = c->dt;
            break;

        case 0x0A:
            step = 0;
            for (unsigned k = 0; k < NUM_KEYS; k++) {
                if (c->keys[k]) {
                    c->regs[x] = (uint8_t)k;
                    step = 2;
                    break;
                }
            }
            break;

        case 0x15:
            c->dt = vx;
            break;

        case 0x18:
            c->st = vx;
            break;

        case 0x1E:
            c->i = (uint16_t)(c->i + vx);
            break;

        case 0x29:
            c->i = (uint16_t)(FONT_BASE + ((vx & 0x0F) * FONT_H));
            break;

        case 0x33:
            c->mem[c->i & 0x0FFF] = (uint8_t)(vx / 100);
            c->mem[(c->i + 1) & 0x0FFF] = (uint8_t)((vx / 10) % 10);
            c->mem[(c->i + 2) & 0x0FFF] = (uint8_t)(vx % 10);
            break;

        case 0x55:
            for (unsigned r = 0; r <= x; r++)
                c->mem[(c->i + r) & 0x0FFF] = c->regs[r];

            if (c->quirks.mem_inc)
                c->i = (uint16_t)(c->i + x + 1);
            break;

        case 0x65:
            for (unsigned r = 0; r <= x; r++)
                c->regs[r] = c->mem[(c->i + r) & 0x0FFF];

            if (c->quirks.mem_inc)
                c->i = (uint16_t)(c->i + x + 1);
            break;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    c->pc = (uint16_t)((c->pc + step) & 0x0FFF);
}
