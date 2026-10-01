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

static bool key_pressed(const Chip8 *c, uint16_t vx)
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
            if (c->sp > STACK_SIZE)   /* sp is past the array: clamp first */
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
        if ((c->regs[x] & 0xFF) == kk)
            step = 4;
        break;

    case 0x4000:
        if ((c->regs[x] & 0xFF) != kk)
            step = 4;
        break;

    case 0x5000:
        if (n == 0 && (c->regs[x] & 0xFF) == (c->regs[y] & 0xFF))
            step = 4;
        break;

    case 0x6000:
        c->regs[x] = kk;
        break;

    case 0x7000:
        c->regs[x] = (uint16_t)(((c->regs[x] & 0xFF) + kk) & 0xFF);
        break;

    case 0x8000: {
        uint8_t vx = (uint8_t)(c->regs[x] & 0xFF);
        uint8_t vy = (uint8_t)(c->regs[y] & 0xFF);
        switch (n) {
        case 0x0:
            c->regs[x] = vy;
            break;
        case 0x1:
            c->regs[x] = (uint16_t)(vx | vy);
            c->regs[0xF] = 0;
            break;
        case 0x2:
            c->regs[x] = (uint16_t)(vx & vy);
            c->regs[0xF] = 0;
            break;
        case 0x3:
            c->regs[x] = (uint16_t)(vx ^ vy);
            c->regs[0xF] = 0;
            break;
        case 0x4:
            c->regs[x] = (uint16_t)((vx + vy) & 0xFF);
            c->regs[0xF] = (unsigned)vx + vy > 0xFF ? 1 : 0;
            break;
        case 0x5:
            c->regs[x] = (uint16_t)((vx - vy) & 0xFF);
            c->regs[0xF] = vx >= vy ? 1 : 0;
            break;
        case 0x6:   /* VIP shifts Vy into Vx; Chip-48 shifts Vx in place */
            c->regs[x] = (uint16_t)(vy >> 1);
            c->regs[0xF] = (uint16_t)(vy & 1);
            break;
        case 0x7:
            c->regs[x] = (uint16_t)((vy - vx) & 0xFF);
            c->regs[0xF] = vy >= vx ? 1 : 0;
            break;
        case 0xE:
            c->regs[x] = (uint16_t)((vy << 1) & 0xFF);
            c->regs[0xF] = (uint16_t)((vy >> 7) & 1);
            break;
        default:
            break;
        }
        break;
    }

    case 0x9000:
        if (n == 0 && (c->regs[x] & 0xFF) != (c->regs[y] & 0xFF))
            step = 4;
        break;

    case 0xA000:
        c->i = nnn;
        break;

    case 0xB000:
        c->pc = (uint16_t)((nnn + (c->regs[0] & 0xFF)) & 0x0FFF);
        return;

    case 0xC000:
        c->regs[x] = (uint16_t)(rand32(c) & kk);
        break;

    case 0xD000: {
        int x0 = (c->regs[x] & 0xFF) % FB_W;
        int y0 = (c->regs[y] & 0xFF) % FB_H;
        bool collision = false;

        for (int row = 0; row < n; row++) {
            int py = y0 + row;
            if (py >= FB_H)
                break;
            uint8_t bits = c->mem[(c->i + row) & 0x0FFF];
            for (int bit = 0; bit < 8; bit++) {
                int px = x0 + bit;
                if (px >= FB_W)
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

    case 0xF000: {   /* kk alone decides; there is no F0nn, Annn is what sets I */
        uint8_t vx = (uint8_t)(c->regs[x] & 0xFF);
        switch (kk) {
        case 0x07:
            c->regs[x] = c->dt;
            break;

        case 0x0A:
            step = 0;
            for (unsigned k = 0; k < NUM_KEYS; k++) {
                if (c->keys[k]) {
                    c->regs[x] = k;
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

        case 0x55:   /* VIP leaves I past the last register: I += x + 1 */
            for (unsigned r = 0; r <= x; r++)
                c->mem[(c->i + r) & 0x0FFF] = (uint8_t)(c->regs[r] & 0xFF);

            c->i = (uint16_t)(c->i + x + 1);
            break;

        case 0x65:
            for (unsigned r = 0; r <= x; r++)
                c->regs[r] = c->mem[(c->i + r) & 0x0FFF];
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
