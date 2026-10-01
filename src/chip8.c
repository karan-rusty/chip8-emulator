#include "chip8.h"

#include <string.h>

/* The standard 8x5 hex font, 16 glyphs of 5 bytes. Each byte holds one row,
 * with the glyph occupying the high nibble and the low nibble left clear, so
 * glyphs are 4 pixels wide and 5 tall.
 *
 * These exact values are what Cowgod's technical reference, tonisagrista's
 * spec, and common emulator implementations all agree on, which is what makes
 * them the de-facto standard. The original COSMAC VIP used a different,
 * space-optimised overlapping font with a lookup table instead; this linear
 * 80-byte layout is the modern convention every ROM is written against. */
static const uint8_t font[FONT_BYTES] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
    0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
    0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
    0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
    0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
    0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
    0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
    0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
    0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
    0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
    0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
    0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
    0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
    0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
    0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
    0xF0, 0x80, 0xF0, 0x80, 0x80, /* F */
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

/* Push return address, saturating at STACK_SIZE. The original COSMAC VIP only
 * checked SP on call, so overflowing the stack here is defined behaviour
 * rather than a crash. */
static void push(Chip8 *c, uint16_t addr)
{
    if (c->sp < STACK_SIZE)
        c->stack[c->sp++] = addr;
    else if (c->sp < UINT8_MAX)
        /* Past the top, but sp is uint8_t: it has to saturate rather than
         * wrap to 0, or a runaway call chain starts overwriting stack[0]
         * from below while still believing it is deep in the stack. */
        c->sp++;
}

/* xorshift32. The state is never allowed to be 0, so a zeroed Chip8 (or a
 * caller that seeds with 0) still produces a stream instead of sticking. */
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

/* The keypad holds 16 keys, so a Vx above 0xF names no key at all. Real ROMs
 * keep key values in range; one that does not should see "not pressed" rather
 * than silently wrapping onto a different key. */
static bool key_pressed(const Chip8 *c, uint16_t vx)
{
    return vx < NUM_KEYS && c->keys[vx];
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
    /* Called once per frame by the frontend, so this is the vertical blank
     * that a drawing instruction waits for. */
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

/* The 35 standard instructions from Cowgod's reference section 3.1.
 *
 * Decode produces a `step` of 2, which every instruction inherits unless it
 * says otherwise: a taken skip uses 4, and Fx0A uses 0 to stay on itself
 * until a key arrives. Jumps and calls set pc directly and return, because
 * for them the fall-through increment is wrong rather than merely undesired.
 *
 * The 0xF group is decoded by its low byte alone. There is no "F0nn" opcode:
 * setting I is Annn, so F055 means only "store V0 at I" and nothing else.
 * Every low byte that is not a real instruction falls through to the inert
 * default rather than being read as a literal. */
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
        if (opcode == 0x00E0) { /* 00E0 - clear the display */
            memset(c->fb, 0, sizeof c->fb);
        } else if (opcode == 0x00EE) { /* 00EE - return */
            /* Calls that overflowed the array still counted on sp, so sp can
             * sit above STACK_SIZE where no return address exists. Collapse
             * those phantom frames first: indexing the array from there would
             * read past its end, and leaving pc alone would loop forever. */
            if (c->sp > STACK_SIZE)
                c->sp = STACK_SIZE;
            if (c->sp > 0) {
                c->pc = c->stack[--c->sp];
                return; /* pc is the return address; do not add 2 */
            }
            /* empty stack: fall through to the step below */
        }
        /* 0nnn SYS is ignored by modern interpreters, per Cowgod 3.1.
         * 00FB/00FC/00FD/00FE are SCHIP and not implemented. */
        break;

    case 0x1000: /* 1nnn - jump to nnn */
        c->pc = nnn;
        return;

    case 0x2000: /* 2nnn - call nnn */
        push(c, (uint16_t)(c->pc + 2));
        c->pc = nnn;
        return;

    case 0x3000: /* 3xkk - skip if VX == kk */
        if ((c->regs[x] & 0xFF) == kk)
            step = 4;
        break;

    case 0x4000: /* 4xkk - skip if VX != kk */
        if ((c->regs[x] & 0xFF) != kk)
            step = 4;
        break;

    case 0x5000: /* 5xy0 - skip if VX == VY. n must be 0; 5xy1-5xyD do not exist. */
        if (n == 0 && (c->regs[x] & 0xFF) == (c->regs[y] & 0xFF))
            step = 4;
        break;

    case 0x6000: /* 6xkk - VX = kk */
        c->regs[x] = kk;
        break;

    case 0x7000: /* 7xkk - VX += kk. No carry and no VF change. */
        c->regs[x] = (uint16_t)(((c->regs[x] & 0xFF) + kk) & 0xFF);
        break;

    case 0x8000: { /* 8xyN - register to register */
        uint8_t vx = (uint8_t)(c->regs[x] & 0xFF);
        uint8_t vy = (uint8_t)(c->regs[y] & 0xFF);
        switch (n) {
        case 0x0: /* LD Vx, Vy */
            c->regs[x] = vy;
            break;
        case 0x1: /* OR */
            c->regs[x] = (uint16_t)(vx | vy);
            c->regs[0xF] = 0;      /* COSMAC VIP quirk: the logical ops
                                    * clear the flags register afterwards */
            break;
        case 0x2: /* AND */
            c->regs[x] = (uint16_t)(vx & vy);
            c->regs[0xF] = 0;
            break;
        case 0x3: /* XOR */
            c->regs[x] = (uint16_t)(vx ^ vy);
            c->regs[0xF] = 0;
            break;
        case 0x4: /* ADD, VF = carry */
            c->regs[x] = (uint16_t)((vx + vy) & 0xFF);
            c->regs[0xF] = (unsigned)vx + vy > 0xFF ? 1 : 0;
            break;
        case 0x5: /* SUB, VF = NOT borrow */
            /* Mask to 8 bits: vx - vy is a negative int here, and casting a
             * negative value straight to uint16_t gives 0xFFF0, not 0xE0. */
            c->regs[x] = (uint16_t)((vx - vy) & 0xFF);
            c->regs[0xF] = vx >= vy ? 1 : 0;
            break;
        case 0x6: /* SHR, VF = old bit 0 of the register being shifted */
            /* The VIP shifts Vy into Vx; Chip-48 shifts Vx in place. This
             * build targets original CHIP-8, so Vy is the source and VF
             * therefore reports Vy's outgoing bit. */
            c->regs[x] = (uint16_t)(vy >> 1);
            c->regs[0xF] = (uint16_t)(vy & 1);
            break;
        case 0x7: /* SUBN, VF = NOT borrow */
            c->regs[x] = (uint16_t)((vy - vx) & 0xFF);
            c->regs[0xF] = vy >= vx ? 1 : 0;
            break;
        case 0xE: /* SHL, VF = old bit 7 of the register being shifted */
            c->regs[x] = (uint16_t)((vy << 1) & 0xFF);
            c->regs[0xF] = (uint16_t)((vy >> 7) & 1);
            break;
        default:  /* 8xy8 through 8xyD do not exist */
            break;
        }
        break;
    }

    case 0x9000: /* 9xy0 - skip if VX != VY. n must be 0. */
        if (n == 0 && (c->regs[x] & 0xFF) != (c->regs[y] & 0xFF))
            step = 4;
        break;

    case 0xA000: /* Annn - I = nnn. This is how a ROM sets I. */
        c->i = nnn;
        break;

    case 0xB000: /* Bnnn - jump to nnn + V0 */
        c->pc = (uint16_t)((nnn + (c->regs[0] & 0xFF)) & 0x0FFF);
        return;

    case 0xC000: /* Cxkk - VX = random AND kk */
        c->regs[x] = (uint16_t)(rand32(c) & kk);
        break;

    case 0xD000: { /* Dxyn - draw an n-row, 8-pixel-wide sprite at VX,VY */
        /* Read the coordinates before drawing: if x or y is 0xF, VF is about
         * to become the collision flag and the sprite must still land where
         * the old VF value said. */
        int vx = c->regs[x] & 0xFF;
        int vy = c->regs[y] & 0xFF;
        /* Dxyn is four hex digits: D, x, y, n. n is a single nibble, so the
         * height is opcode & 0x0F (0-15 rows) - not the 12-bit nnn, and not
         * the low byte either.
         *
         * Two SCHIP forms reuse this encoding and are NOT implemented here,
         * because this interpreter targets original CHIP-8 only:
         *   Dx0  - 16x16 sprite. n=0, so this draws nothing.
         *   DxCn - C rows of a 16-pixel-wide sprite (8x16 video mode).
         *          Drawn here as C rows of 8 pixels, i.e. half width.
         * A ROM using either will look wrong rather than crash. Treat that as
         * "needs SCHIP", not as a decoder fault. */
        int height = n;
        bool collision = false;
        /* The start coordinate wraps into the screen first, then the sprite is
         * clipped at the right and bottom edges. The VIP's DRAW walks a
         * contiguous framebuffer and stops at the end of it; it does not run
         * off the end and reappear at the top. Wrapping here would be the
         * SCHIP behaviour instead. */
        int x0 = vx % FB_W;
        int y0 = vy % FB_H;

        for (int row = 0; row < height; row++) {
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
        /* VF is the collision flag. Games poll it to bounce off walls, so
         * writing it last also makes it win when x or y is 0xF. */
        c->regs[0xF] = collision ? 1 : 0;
        /* Display-wait quirk: the VIP's DRAW blocks until the next vertical
         * blank, which caps drawing at one sprite per frame. chip8_run stops
         * on this flag and chip8_tick_timers clears it, so the rest of the
         * current frame's cycles are simply not spent. */
        c->vblank = true;
        break;
    }

    case 0xE000: /* Ex9E / ExA1 - skip on key state */
        if (kk == 0x9E) {          /* SKP  - skip while the key is down */
            if (key_pressed(c, c->regs[x]))
                step = 4;
        } else if (kk == 0xA1) {   /* SKNP - skip while the key is up */
            if (!key_pressed(c, c->regs[x]))
                step = 4;
        }
        break;

    case 0xF000: { /* Fxkk - timers, keys and memory, all named by kk */
        uint8_t vx = (uint8_t)(c->regs[x] & 0xFF);
        switch (kk) {
        case 0x07: /* Fx07 - VX = delay timer */
            c->regs[x] = c->dt;
            break;

        case 0x0A: /* Fx0A - wait for a key, then VX = that key */
            /* Stalling is a step of 0: pc stays on this instruction until a
             * key is down, so the fetch re-reads it next cycle. Terminals
             * report presses but not releases, so this waits for "a key is
             * currently down" rather than a rising edge. */
            step = 0;
            for (unsigned k = 0; k < NUM_KEYS; k++) {
                if (c->keys[k]) {
                    c->regs[x] = k;
                    step = 2;
                    break;
                }
            }
            break;

        case 0x15: /* Fx15 - delay timer = VX */
            c->dt = vx;
            break;

        case 0x18: /* Fx18 - sound timer = VX */
            c->st = vx;
            break;

        case 0x1E: /* Fx1E - I += VX. No VF change on original CHIP-8. */
            c->i = (uint16_t)(c->i + vx);
            break;

        case 0x29: /* Fx29 - I = address of the glyph for digit VX */
            /* The COSMAC VIP manual calls this "LSD" (Least Significant
             * Digit) and the disassembly masks with 0x0F explicitly. There
             * are ROMs that leave upper bits set in VX and depend on that,
             * so the mask is required rather than defensive. */
            c->i = (uint16_t)(FONT_BASE + ((vx & 0x0F) * FONT_H));
            break;

        case 0x33: /* Fx33 - BCD of VX into mem[I..I+2] */
            c->mem[c->i & 0x0FFF] = (uint8_t)(vx / 100);
            c->mem[(c->i + 1) & 0x0FFF] = (uint8_t)((vx / 10) % 10);
            c->mem[(c->i + 2) & 0x0FFF] = (uint8_t)(vx % 10);
            break;

        case 0x55: /* Fx55 - store V0..VX at I */
            for (unsigned r = 0; r <= x; r++)
                c->mem[(c->i + r) & 0x0FFF] = (uint8_t)(c->regs[r] & 0xFF);
            /* The original COSMAC VIP leaves I pointing just past the last
             * register written (I += X + 1). Chip-48 and most later
             * interpreters leave I alone. This build targets original CHIP-8,
             * so it follows the VIP. */
            c->i = (uint16_t)(c->i + x + 1);
            break;

        case 0x65: /* Fx65 - load V0..VX from I */
            for (unsigned r = 0; r <= x; r++)
                c->regs[r] = c->mem[(c->i + r) & 0x0FFF];
            c->i = (uint16_t)(c->i + x + 1);   /* same VIP rule as Fx55 */
            break;

        default:
            /* No such instruction: F055 is never "I = 0x55", and 8 unused
             * low bytes per register are simply inert. */
            break;
        }
        break;
    }

    default:
        break;
    }

    c->pc = (uint16_t)((c->pc + step) & 0x0FFF);
}

void chip8_run(Chip8 *c, int cycles)
{
    /* Dxyn raises vblank to say "no further drawing this frame". The
     * instruction has already completed and advanced pc, so the remaining
     * cycles of this slice are not spent; the frontend clears the flag when
     * it ticks the timers. */
    for (int i = 0; i < cycles && !c->vblank; i++)
        chip8_execute(c, chip8_fetch(c));
}
